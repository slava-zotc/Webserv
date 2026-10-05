#ifndef CGI_PROCESS_HPP
#define CGI_PROCESS_HPP

#include <sys/types.h>

#include <string>
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
	int get_status() const;
	const std::string& get_output() const;
	bool read_cgi_output();
	void wait_for_child();

private:
	static std::vector<char*> convert_string_to_char_array(
	const std::vector<std::string>& env_string);
	CgiProcess(const CgiProcess& src);
	CgiProcess& operator=(const CgiProcess& rhs);
	pid_t pid_;
	int status_child_;
	int fd_read_;
	std::string buffer_output_;
};

#endif	// CGI_PROCESS_HPP
