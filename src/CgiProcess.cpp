#include "CgiProcess.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpUtils.hpp"
#include "Server.hpp"

// Наибольший fd, который может быть открыт у сервера: стандартный лимит
// открытых файлов на процесс (ulimit -n). Точный лимит узнаётся через
// getrlimit/sysconf, но их нет в списке разрешённых функций сабжекта.
static const int MAX_INHERITED_FD = 1024;

/**
 * @brief Аналог closefrom(3): в процессе-ребёнке закрывает всё, что он
 * унаследовал от сервера, кроме stdin/stdout/stderr.
 *
 * Иначе скрипт держит копии слушающих и клиентских сокетов и pipe других
 * CGI. Пример: скрипт B унаследовал пишущий конец stdin скрипта A — тогда A
 * не получит EOF на stdin, пока жив B (EOF наступает, только когда закрыты
 * ВСЕ ссылки на пишущий конец). FD_CLOEXEC решил бы это при создании fd, но
 * он ставится только через fcntl(F_SETFD), а сабжект разрешает лишь F_SETFL.
 */
static void close_inherited_fds()
{
	for (int fd = 3; fd < MAX_INHERITED_FD; ++fd)
	{
		close(fd);
	}
}

CgiProcess::CgiProcess(void)
	: pid_(-1),
	  status_child_(0),
	  fd_read_(-1),
	  buffer_output_(),
	  last_activity_(0),
	  fd_write_(-1),
	  body_(),
	  body_offset_(0)
{
}

CgiProcess::~CgiProcess(void)
{
	if (fd_read_ != -1)
	{
		close(fd_read_);
	}
	if (fd_write_ != -1)
	{
		close(fd_write_);
	}
	if (pid_ != -1)
	{
		kill(pid_, SIGKILL);
		waitpid(pid_, NULL, 0);
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

static std::string server_name(const HttpRequest& request,
							   const Server& server)
{
	const std::map<std::string, std::string>& headers = request.get_headers();
	std::map<std::string, std::string>::const_iterator it =
		headers.find("host");
	if (it != headers.end() && !it->second.empty())
	{
		std::string host = it->second;
		std::string::size_type colon = host.find(':');
		if (colon != std::string::npos) host.erase(colon);
		if (!host.empty()) return host;
	}
	if (server.get_host() != "0.0.0.0") return server.get_host();
	return "localhost";
}

/**
 * @brief Переменные окружения для скрипта (RFC 3875 §4.1)
 * @param filename    Имя файла скрипта (относительно его каталога)
 * @param script_name URL скрипта без PATH_INFO: /cgi-bin/env.py
 * @param path_info   Хвост после скрипта: /a/b (или пусто)
 * @param remote_addr IP клиента из accept()
 */
std::vector<std::string> CgiProcess::get_env_string(
	const HttpRequest& request, const Server& server,
	const std::string& filename, const std::string& script_name,
	const std::string& path_info, const std::string& remote_addr)
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
	result.push_back("SCRIPT_NAME=" + script_name);		  // §4.1.13
	// RFC 3875 §4.1.5: PATH_INFO - хвост пути после имени скрипта
	// (/env.py/a/b -> /a/b). Если хвоста нет, по RFC это пустое значение,
	// но cgi_tester из subject 42 тогда отвечает "PATH_INFO not found":
	// он ждёт полный путь запроса. Так же поступают многие серверы для
	// совместимости (php-cgi и др.), поэтому без хвоста передаём весь путь.
	result.push_back("PATH_INFO="
					 + (path_info.empty() ? request.get_path() : path_info));
	result.push_back("REMOTE_ADDR=" + remote_addr);		  // §4.1.8
	// REQUEST_URI нет в RFC 3875, но его ставят Apache и nginx (fastcgi),
	// и на него опираются скрипты, в том числе cgi_tester из subject 42.
	result.push_back("REQUEST_URI=" + request.get_path()
					 + (request.get_query().empty() ? "" : "?" + request.get_query()));
	result.push_back("SERVER_SOFTWARE=webserv/1.0");	  // §4.1.17
	// RFC 3875 §4.1.14: SERVER_NAME - имя сервера, к которому обратился
	// клиент; берём его из Host без порта, иначе из listen.
	result.push_back("SERVER_NAME=" + server_name(request, server));

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
	const size_t buffer_size = 65536;
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
		last_activity_ = std::time(NULL);
		return true;
	}
}

int CgiProcess::process_output(HttpResponse& response)
{
	if (fd_read_ != -1 && !read_cgi_output())
	{
		return CGI_ERROR;
	}
	if (is_output_done())
	{
		int wait_result = wait_for_child();
		if (wait_result == -1)
		{
			return CGI_ERROR;
		}
		else if (wait_result == 0)
		{
			return CGI_RUNNING;
		}
		else if (wait_result == 1
				 && (!WIFEXITED(get_status())
					 || WEXITSTATUS(get_status()) != 0))
		{
			return CGI_ERROR;
		}
		if (!handle_cgi_response(response))
		{
			return CGI_ERROR;
		}
		return CGI_SUCCESS;
	}
	else
	{
		return CGI_RUNNING;
	}
}

bool CgiProcess::handle_cgi_response(HttpResponse& response)
{
	// Разбираем только заголовки: копировать в istringstream весь вывод
	// (а он бывает 100 МБ) незачем. Конец заголовков - первая пустая
	// строка, "\r\n\r\n" или "\n\n" (RFC 3875 §6.2 допускает оба вида).
	std::string::size_type crlf = buffer_output_.find("\r\n\r\n");
	std::string::size_type lf = buffer_output_.find("\n\n");
	std::string::size_type body_start = std::string::npos;
	if (crlf != std::string::npos && (lf == std::string::npos || crlf < lf))
		body_start = crlf + 4;
	else if (lf != std::string::npos)
		body_start = lf + 2;
	std::istringstream response_stream(body_start == std::string::npos
										   ? buffer_output_
										   : buffer_output_.substr(0, body_start));
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
			if (pos != -1 && body_start != std::string::npos)
			{
				// Тело - всё после заголовков: сдвигаем и забираем буфер
				// обменом, без копии.
				buffer_output_.erase(0, body_start);
				response.swap_body(buffer_output_);
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

int CgiProcess::wait_for_child()
{
	if (pid_ != -1)
	{
		int status;

		int status_proc = waitpid(pid_, &status, WNOHANG);
		if (status_proc == 0)
		{
			return 0;  // Child process is still running
		}
		else if (status_proc == -1)
		{
			return -1;	// Error occurred while waiting
		}
		status_child_ = status;
		pid_ = -1;
	}
	return 1;  // Child process has finished
}

bool CgiProcess::execute_cgi(const std::string& path_interpreter,
							 const std::string& directory,
							 const std::string& filename,
							 const std::vector<std::string>& env,
							 std::string& body)
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
	// Сбрасываем буферы до fork: иначе ребёнок унаследует недописанный
	// вывод сервера, и std::exit() в нём напечатает его второй раз.
	std::cout.flush();
	std::cerr.flush();
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
		// Игнорирование SIGPIPE, в отличие от обработчиков, переживает
		// execve. Возвращаем поведение по умолчанию, чтобы скрипт, чей
		// вывод никто не читает, завершался, как обычная программа.
		signal(SIGPIPE, SIG_DFL);
		close(out_fd[0]);
		close(in_fd[1]);
		if (dup2(out_fd[1], STDOUT_FILENO) == -1
			|| dup2(in_fd[0], STDIN_FILENO) == -1)
		{
			std::exit(EXIT_FAILURE);
		}
		close_inherited_fds();
		if (chdir(directory.c_str()) == -1)
		{
			std::exit(EXIT_FAILURE);
		}
		execve(path_interpreter.c_str(), &argv_array[0], &env_array[0]);
		std::exit(EXIT_FAILURE);
	}

	// Родитель. Концы, которыми пользуется скрипт, закрываем сразу:
	// пока они открыты у нас, скрипт не получит EOF на stdin.
	close(in_fd[0]);
	close(out_fd[1]);

	// O_NONBLOCK ставим ТОЛЬКО на свои концы: флаг живёт в открытом файле
	// (общем с процессом-ребёнком после fork), а не в дескрипторе.
	if (fcntl(out_fd[0], F_SETFL, O_NONBLOCK) == -1
		|| fcntl(in_fd[1], F_SETFL, O_NONBLOCK) == -1)
	{
		close(out_fd[0]);
		close(in_fd[1]);
		kill(pid_, SIGKILL);
		waitpid(pid_, NULL, 0);
		pid_ = -1;
		return false;
	}
	fd_read_ = out_fd[0];
	last_activity_ = std::time(NULL);

	// Тело запроса уходит скрипту через stdin (RFC 3875 §4.2) кусками, по
	// мере того как poll() сообщает POLLOUT. Пустое тело — сразу EOF.
	if (body.empty())
	{
		close(in_fd[1]);
	}
	else
	{
		fd_write_ = in_fd[1];
		body_.swap(body);	// забираем тело запроса без копии (может быть 100 МБ)
		body_offset_ = 0;
	}
	return true;
}

int CgiProcess::get_fd_read() const
{
	return fd_read_;
}

int CgiProcess::get_fd_write() const
{
	return fd_write_;
}

/**
 * @brief Один write() очередного куска тела запроса в stdin скрипта.
 *
 * Вызывается только когда poll() сообщил POLLOUT (или POLLERR/POLLHUP) на
 * fd_write_. Когда тело передано целиком — закрываем pipe: скрипт получит EOF.
 * Ошибка write() (обычно EPIPE: скрипт завершился, не дочитав stdin) — не
 * ошибка сервера: RFC 3875 §4.2 разрешает скрипту не читать тело. Просто
 * закрываем pipe, вывод скрипта по-прежнему читается и отдаётся клиенту.
 * SIGPIPE игнорируется в Core::core_loop, поэтому сервер не упадёт.
 */
void CgiProcess::write_cgi_input()
{
	if (fd_write_ == -1)
	{
		return;
	}
	ssize_t written = write(fd_write_, body_.data() + body_offset_,
							body_.size() - body_offset_);
	if (written > 0)
	{
		body_offset_ += static_cast<std::size_t>(written);
		last_activity_ = std::time(NULL);
	}
	if (written <= 0 || body_offset_ >= body_.size())
	{
		close(fd_write_);
		fd_write_ = -1;
		std::string().swap(body_);	// clear() не отдаёт память, swap - отдаёт
	}
}

bool CgiProcess::is_timeout(std::time_t now, int limit) const
{
	if (last_activity_ == 0)
	{
		return false;  // Process has not started yet
	}
	// Таймаут от бездействия, а не от запуска: скрипт, который честно
	// принимает или отдаёт 100 МБ, может работать дольше limit, а
	// зависший (infinite.py) молчит и будет убит через limit секунд.
	return (now - last_activity_) > limit;
}
