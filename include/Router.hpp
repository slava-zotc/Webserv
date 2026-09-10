#ifndef ROUTER_HPP
#define ROUTER_HPP
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Server.hpp"
class Router
{
public:
	static const HttpResponse handle_request(const HttpRequest& request,
											 const Server& server);
	static const Route* matching(const std::string& path,
								 const std::vector<Route>& route);
	static HttpResponse handle_get_method(const std::string& resolve_path, const Route &route);
	static HttpResponse handle_post_method(const std::string& upload_path,
											const std::string& body);
	static HttpResponse handle_delete_method(const std::string& resolve_path);
	static std::string resolve_path(const std::string& path,
									const Route& route);
	static std::string resolve_upload_path(const std::string& path,
											const Route& route);

private:
	static std::string get_content_type(const std::string& path);
};

#endif	// ROUTER_HPP