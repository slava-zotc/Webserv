#include "Server.hpp"

Server::Server(const CFG_Server& config)
	: port(config.port_),
	  max_body_size_(config.max_body_size_),
	  route(config.routes_),
	  error_pages_(config.error_pages_)
{
}

Server::~Server()
{
}

const std::vector<Route>& Server::get_route() const
{
	return route;
}

int Server::get_port() const
{
	return port;
}

unsigned long Server::get_max_body_size() const
{
	return max_body_size_;
}

const std::map<int, std::string>& Server::get_error_pages() const
{
	return error_pages_;
}