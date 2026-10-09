#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

#include <unistd.h>

#include <string>

#include "HttpRequest.hpp"
#include "Route.hpp"
#include "Socket.hpp"

class Server;
class CgiProcess;
class HttpResponse;
class ClientSocket
{
public:
	enum Events
	{
		WANT_READ = 1 << 0,	 ///< Готов читать данные из сокета
		WANT_WRITE = 1 << 1	 ///< Готов писать данные в сокет
	};
	~ClientSocket();

	ClientSocket(int fd);

	short get_ready_events() const;

	bool is_ready_send() const;

	bool is_reading_phase() const;

	bool is_ready_delete() const;

	void handle_read(const Server& server);

	void handle_write();

	void handle_cgi_events(const Server& server);

	int get_cgi_fd_read() const;

	int get_client_socket_fd() const;

	bool has_cgi() const;

	bool is_waiting_cgi_exit() const;

private:
	enum State
	{
		READING,
		WAITING_CGI,
		READY_SEND,
		READY_DELETE
	};
	ClientSocket();
	ClientSocket(const ClientSocket& src);
	ClientSocket& operator=(const ClientSocket& rhs);
	Socket socket_fd;
	std::string response_buffer;
	HttpRequest request;
	std::string::size_type partial_write;
	State state_client;
	CgiProcess* cgi_process;
	void start_cgi(const Route& route, const Server& server);
};

#endif	// CLIENT_SOCKET_HPP
