#include "CgiProcess.hpp"

CgiProcess::CgiProcess(void) : pid_(-1), fd_read_(-1)
{
}

CgiProcess::~CgiProcess(void)
{
}

std::vector<std::string> CgiProcess::get_dir_and_filename(const std::string& path)
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