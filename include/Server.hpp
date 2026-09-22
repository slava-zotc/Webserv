#ifndef SERVER_HPP
#define SERVER_HPP
#include <map>
#include <string>
#include <vector>
#include "Route.hpp"

struct CFG_Server
{
	int port_;
	unsigned long max_body_size_;
	std::vector<Route> routes_;
	std::map<int, std::string> error_pages_;
};

class Server
{
public:
	Server(const CFG_Server& config);
	~Server();
	const std::vector<Route>& get_route() const;
	int get_port() const;
	unsigned long get_max_body_size() const;
	const std::map<int, std::string>& get_error_pages() const;
private:
	int port;
	unsigned long max_body_size_;
	std::vector<Route> route;
	std::map<int, std::string> error_pages_;
	Server();
	Server(const Server& src);
	Server& operator=(const Server& rhs);
};
#endif