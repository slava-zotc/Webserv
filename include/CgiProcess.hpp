#ifndef CGI_PROCESS_HPP
#define CGI_PROCESS_HPP

#include <sys/types.h>

#include <string>
#include <ctime>
#include <vector>

class Server;
class HttpRequest;
class HttpResponse;
class CgiProcess
{
public:
	CgiProcess();
	~CgiProcess();
	static std::vector<std::string> get_dir_and_filename(
		const std::string& path);
	static std::vector<std::string> get_env_string(const HttpRequest& reguest,
												   const Server& server,
												   const std::string& filename);

	bool execute_cgi(const std::string& path_interpreter,
					 const std::string& directory, const std::string& filename,
					 const std::vector<std::string>& env);
	bool is_output_done() const;
	int get_fd_read() const;
	int get_status() const;
	bool handle_cgi_response(HttpResponse& response);
	int process_output(HttpResponse& response);
	const std::string& get_output() const;
	bool read_cgi_output();
	int wait_for_child();
	bool is_timeout(std::time_t now, int limit) const;
	enum
	{
		CGI_SUCCESS = 0,
		CGI_RUNNING = 1,
		CGI_ERROR = -1
	};

private:
	static std::vector<char*> convert_string_to_char_array(
		const std::vector<std::string>& env_string);
	CgiProcess(const CgiProcess& src);
	CgiProcess& operator=(const CgiProcess& rhs);
	pid_t pid_;
	int status_child_;
	int fd_read_;
	std::string buffer_output_;
	std::time_t start_time_;
};

#endif	// CGI_PROCESS_HPP
