#ifndef CGI_PROCESS_HPP
#define CGI_PROCESS_HPP

#include <sys/types.h>
#include <string>
#include <vector>

class Server;
class HttpRequest;
class CgiProcess
{
	public:
	CgiProcess();
	~CgiProcess();
	static std::vector<std::string> get_dir_and_filename(const std::string& path);
	static std::vector<std::string> get_env_string(const HttpRequest& reguest, const Server& server, const std::string& filename);
	
	private:
	CgiProcess(const CgiProcess& src);
	CgiProcess& operator=(const CgiProcess& rhs);
	pid_t pid_;
	int fd_read_;
	std::string buffer_output_;
};

#endif // CGI_PROCESS_HPP
