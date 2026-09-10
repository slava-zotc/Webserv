#include <exception>
#include <iostream>
#include <vector>

#include "Core.hpp"
#include "Route.hpp"
#include "Server.hpp"

int main()
{
	try
	{
		CFG_Route cfg_route;
		cfg_route.prefix_ = "/";
		cfg_route.root_ = "/home/dev/dev/projects/WebServ/www";
		cfg_route.autoindex_ = false;
		cfg_route.index_file_ = "index.html";
		cfg_route.has_redirect_ = false;
		cfg_route.redirect_target_ = "";
		cfg_route.allowed_methods_ = Route::GET | Route::POST | Route::DELETE;

		std::vector<Route> routes;
		routes.push_back(Route(cfg_route));

		CFG_Server cfg_server;
		cfg_server.port_ = 8080;
		cfg_server.max_body_size_ = 1048576;
		cfg_server.routes_ = routes;

		Server server(cfg_server);

		std::vector<Server*> servers;
		servers.push_back(&server);

		Core core(servers);
		core.core_loop();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}