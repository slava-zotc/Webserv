#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

#include <unistd.h>

#include <string>

#include "HttpRequest.hpp"
#include "Server.hpp"
#include "Socket.hpp"
#include "Router.hpp"

class ClientSocket
{
public:
	enum Events
	{
		WANT_READ = 1 << 0,	 ///< Готов читать данные из сокета
		WANT_WRITE = 1 << 1	 ///< Готов писать данные в сокет
	};
	~ClientSocket();

	ClientSocket(int fd, const Server* server);

	short get_ready_events() const;

	bool is_ready_send() const;

	bool is_reading_phase() const;

	bool is_ready_delete() const;

	void handle_read();

	void handle_write();

	int get_client_socket_fd() const;

private:
	enum State
	{
		READING,
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
	const Server* server_;
};

#endif	// CLIENT_SOCKET_HPP
