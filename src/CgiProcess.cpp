#include "CgiProcess.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpUtils.hpp"
#include "Server.hpp"

CgiProcess::CgiProcess(void) : pid_(-1), fd_read_(-1)
{
}

CgiProcess::~CgiProcess(void)
{
	if (fd_read_ != -1)
	{
		close(fd_read_);
	}
	if (pid_ != -1)
	{
		kill(pid_, SIGKILL);
		wait_for_child();
	}
}

std::vector<std::string> CgiProcess::get_dir_and_filename(
	const std::string& path)
{
	std::vector<std::string> resul(2);

	size_t pos = path.find_last_of('/');

	if (pos != std::string::npos)
	{
		if (pos != 0)
			resul[0] = path.substr(0, pos);
		else
			resul[0] = path[0];
		resul[1] = path.substr(pos + 1);
	}
	else
	{
		resul[0] = ".";
		resul[1] = path;
	}
	return resul;
}

std::vector<std::string> CgiProcess::get_env_string(const HttpRequest& request,
													const Server& server,
													const std::string& filename)
{
	std::vector<std::string> result;
	result.push_back("REQUEST_METHOD=" + request.get_str_method());
	result.push_back("SCRIPT_FILENAME=" + filename);
	result.push_back("QUERY_STRING=" + request.get_query());

	std::stringstream ss;
	ss << request.get_body().size();
	result.push_back("CONTENT_LENGTH=" + ss.str());
	result.push_back("SERVER_PROTOCOL=" + request.get_version());
	result.push_back("GATEWAY_INTERFACE=CGI/1.1");
	result.push_back("REDIRECT_STATUS=200");
	ss.str("");
	ss << server.get_port();
	result.push_back("SERVER_PORT=" + ss.str());
	result.push_back("SCRIPT_NAME=" + request.get_path());
	result.push_back("SERVER_NAME=localhost");

	const std::map<std::string, std::string>& headers = request.get_headers();

	std::map<std::string, std::string>::const_iterator it =
		headers.find("content-type");
	if (it != headers.end())
	{
		result.push_back("CONTENT_TYPE=" + it->second);
	}

	it = headers.begin();
	for (; it != headers.end(); ++it)
	{
		std::string key = it->first;
		if (key == "content-type" || key == "content-length" || key == "proxy"
			|| key == "connection")
			continue;
		std::transform(key.begin(), key.end(), key.begin(), ::toupper);
		std::replace(key.begin(), key.end(), '-', '_');
		result.push_back("HTTP_" + key + "=" + it->second);
	}

	return result;
}

std::vector<char*> CgiProcess::convert_string_to_char_array(
	const std::vector<std::string>& env_string)
{
	std::vector<char*> env_array;
	for (size_t i = 0; i < env_string.size(); ++i)
	{
		env_array.push_back(const_cast<char*>(env_string[i].c_str()));
	}
	env_array.push_back(NULL);
	return env_array;
}

bool CgiProcess::is_output_done() const
{
	return (fd_read_ == -1);
}

int CgiProcess::get_status() const
{
	return status_child_;
}

const std::string& CgiProcess::get_output() const
{
	return buffer_output_;
}

bool CgiProcess::read_cgi_output()
{
	const size_t buffer_size = 4096;
	char buffer[buffer_size];
	ssize_t bytes_read = read(fd_read_, buffer, buffer_size);
	if (bytes_read == -1)
	{
		close(fd_read_);
		fd_read_ = -1;
		return false;
	}
	else if (bytes_read == 0)
	{
		close(fd_read_);
		fd_read_ = -1;
		return true;  // End of output
	}
	else
	{
		buffer_output_.append(buffer, bytes_read);
		return true;
	}
}

bool CgiProcess::handle_cgi_response(HttpResponse& response)
{
	std::istringstream response_stream(buffer_output_);
	std::string line;
	bool has_status_line = false;
	bool has_content_type = false;
	bool has_header_end = false;
	std::streampos pos;
	while (std::getline(response_stream, line))
	{
		// Условие для окончания заголовков: пустая строка или только \r
		// (в зависимости от того, как CGI выводит заголовки)
		if ((line == "\r" || line.empty()) && !response_stream.eof())
		{
			pos = response_stream.tellg();
			if (pos != -1)
			{
				std::string body;
				body = buffer_output_.substr(pos);
				response.set_body(body);
			}
			has_header_end = true;
			break;	// End of headers
		}

		if (!line.empty() && line[line.size() - 1] == '\r')
		{
			line.erase(line.size() - 1);  // Remove trailing \r
		}
		std::string key, value;
		if (!parse_header_line(line, key, value))
		{
			response.set_status_code(502);	// Bad Gateway
			return false;					// Invalid header line
		}
		if (key == "status")
		{
			if (has_status_line)
			{
				response.set_status_code(502);	// Bad Gateway
				return false;					// Multiple status lines
			}
			if (value.size() < 3
				|| !std::isdigit(static_cast<unsigned char>(value[0]))
				|| !std::isdigit(static_cast<unsigned char>(value[1]))
				|| !std::isdigit(static_cast<unsigned char>(value[2]))
				|| (value.size() > 3 && value[3] != ' '))
			{
				response.set_status_code(502);	// Bad Gateway
				return false;					// Invalid status line
			}
			std::istringstream status_stream(value);
			int status_code;
			status_stream >> status_code;
			if (status_stream.fail() || status_code < 100 || status_code > 599)
			{
				response.set_status_code(502);	// Bad Gateway
				return false;					// Invalid status code
			}
			response.set_status_code(status_code);
			has_status_line = true;
			continue;  // Skip adding this header to the response
		}
		else if (key == "content-type" && !has_content_type)
		{
			has_content_type = true;
		}
		else if (key == "content-type" && has_content_type)
		{
			response.set_status_code(502);	// Bad Gateway
			return false;					// Multiple Content-Type headers
		}
		else if (key == "content-length")
		{
			continue;  // Ignore Content-Length header from CGI
		}

		response.set_header(key, value);
	}
	if (!has_header_end || !has_content_type)
	{
		response.set_status_code(502);	// Bad Gateway
		return false;					// Headers not properly terminated
	}
	return true;
}

void CgiProcess::wait_for_child()
{
	if (pid_ != -1)
	{
		int status;
		waitpid(pid_, &status, 0);
		status_child_ = status;
		pid_ = -1;
	}
}

bool CgiProcess::execute_cgi(const std::string& path_interpreter,
							 const std::string& directory,
							 const std::string& filename,
							 const std::vector<std::string>& env)
{
	std::vector<std::string> argv(2);
	argv[0] = path_interpreter;
	argv[1] = filename;
	const std::vector<char*> env_array =
		CgiProcess::convert_string_to_char_array(env);
	const std::vector<char*> argv_array =
		CgiProcess::convert_string_to_char_array(argv);

	int out_fd[2];
	int in_fd[2];
	if (pipe(out_fd) == -1)
	{
		return false;
	}
	if (pipe(in_fd) == -1)
	{
		close(out_fd[0]);
		close(out_fd[1]);
		return false;
	}
	pid_ = fork();
	if (pid_ == -1)
	{
		close(out_fd[0]);
		close(out_fd[1]);
		close(in_fd[0]);
		close(in_fd[1]);
		return false;
	}
	if (pid_ == 0)
	{
		close(out_fd[0]);
		close(in_fd[1]);
		if (dup2(out_fd[1], STDOUT_FILENO) == -1 || dup2(in_fd[0], STDIN_FILENO) == -1)
		{
			std::exit(EXIT_FAILURE);
		}
		close(in_fd[0]);
		close(out_fd[1]);
		if (chdir(directory.c_str()) == -1)
		{
			std::exit(EXIT_FAILURE);
		}
		execve(path_interpreter.c_str(), &argv_array[0], &env_array[0]);
		std::exit(EXIT_FAILURE);
	}
	else
	{
		close(in_fd[0]);
		close(in_fd[1]); // в этом примере мы не используем стандартный ввод для CGI, поэтому закрываем его
		close(out_fd[1]);
		fd_read_ = out_fd[0];
	}

	return true;
}