#include "CgiProcess.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include "HttpRequest.hpp"
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

	int pipe_fd[2];
	if (pipe(pipe_fd) == -1)
	{
		return false;
	}
	pid_ = fork();
	if (pid_ == -1)
	{
		close(pipe_fd[0]);
		close(pipe_fd[1]);
		return false;
	}
	if (pid_ == 0)
	{
		close(pipe_fd[0]);
		dup2(pipe_fd[1], STDOUT_FILENO);
		close(pipe_fd[1]);
		if (chdir(directory.c_str()) == -1)
		{
			std::exit(EXIT_FAILURE);
		}
		execve(path_interpreter.c_str(), &argv_array[0], &env_array[0]);
		std::exit(EXIT_FAILURE);
	}
	else
	{
		close(pipe_fd[1]);
		fd_read_ = pipe_fd[0];
		
	}

	return true;
}