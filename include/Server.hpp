#ifndef SERVER_HPP
#define SERVER_HPP
#include <map>
#include <string>
#include <vector>
class Server
{
public:
	Server();
	~Server();

private:
	int port;
	unsigned long max_body_size_;
	std::map<int, std::string> error_pages_;
};
#endif
