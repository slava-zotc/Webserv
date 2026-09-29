#pragma once

#include <sys/types.h>
#include <string>
#include <vector>
class CgiProcess
{
public:
	CgiProcess();
	~CgiProcess();
    static std::vector<std::string> get_dir_and_filename(const std::string& path);

private:
	CgiProcess(const CgiProcess& src);
	CgiProcess& operator=(const CgiProcess& rhs);
	pid_t pid_;
	int fd_read_;
	std::string buffer_output_;
};