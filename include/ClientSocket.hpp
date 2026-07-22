#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

#include <unistd.h>

#include <string>

#include "Socket.hpp"

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

	void handle_read();

	void handle_write();

	int get_client_socket_fd() const;

private:
	enum State
	{
		READ_HEADERS,
		READ_BODY,
		PROCESS,
		READY_SEND,
		READY_DELETE
	};
	ClientSocket();
	ClientSocket(const ClientSocket& src);
	ClientSocket& operator=(const ClientSocket& rhs);
	Socket socket_fd;
	std::string response_buffer;
	std::string request_buffer;
	std::string::size_type partial_write;
	State state_client;
};

#endif	// CLIENT_SOCKET_HPP
