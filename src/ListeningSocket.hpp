#ifndef LISTENING_SOCKET_HPP
#define LISTENING_SOCKET_HPP

#include "Socket.hpp"
#include <sys/socket.h>
#include <stdexcept>
#include <fcntl.h>
#include <netinet/in.h>

class ListeningSocket
{
public:
    ListeningSocket(int port);
    ~ListeningSocket();
    int accept_conection();
    int get_listen_socket_fd() const;

private:
    static int create_non_blocking_socket_fd();
    
    ListeningSocket(const ListeningSocket &src);
    ListeningSocket &operator=(const ListeningSocket &rhs);
    ListeningSocket();
    Socket fd_socket;
    
};

#endif // LISTENING_SOCKET_HPP