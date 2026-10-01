#include "CgiProcess.hpp"
#include <sstream>
#include <algorithm>
#include "HttpRequest.hpp"
#include "Server.hpp"

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

std::vector<std::string> CgiProcess::get_env_string(const HttpRequest& request, const Server& server, const std::string& filename)
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
	
	std::map<std::string, std::string>::const_iterator it = headers.find("content-type");
	if (it != headers.end())
	{
		result.push_back("CONTENT_TYPE=" + it->second);
	}

	it = headers.begin();
	for (; it != headers.end(); ++it)
	{
		std::string key = it->first;
		if (key == "content-type" || key == "content-length" || key == "proxy" || key == "connection")
			continue;
		std::transform(key.begin(), key.end(), key.begin(), ::toupper);
		std::replace(key.begin(), key.end(), '-', '_');
		result.push_back("HTTP_" + key + "=" + it->second);
	}
	
	return result;
}