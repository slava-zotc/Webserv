#include "ListeningSocket.hpp"

int ListeningSocket::create_non_blocking_socket_fd()
{
    int server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        throw std::runtime_error("ERROR func socket");

    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1)
    {
        close(server_fd);
        throw std::runtime_error("ERROR func fcntl");
    }
    return server_fd;
}

int ListeningSocket::get_listen_socket_fd() const
{
    return fd_socket.get_fd();
}

ListeningSocket::ListeningSocket(int port)
    : fd_socket(create_non_blocking_socket_fd())
{
    int server_fd = get_listen_socket_fd();

    struct sockaddr_in adrr = {0};
    adrr.sin_family = AF_INET;
    adrr.sin_port = htons(port);
    adrr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&adrr, sizeof(adrr)) == -1)
        throw std::runtime_error("ERROR func bind");

    if (listen(server_fd, 128) == -1)
        throw std::runtime_error("ERROR func listen");
}

ListeningSocket::ListeningSocket(const ListeningSocket &src)
{
    *this = src;
}

ListeningSocket &ListeningSocket::operator=(const ListeningSocket &rhs)
{
    if (this != &rhs)
    {
        // copy members
    }
    return (*this);
}

ListeningSocket::~ListeningSocket(void)
{
}

int ListeningSocket::accept_conection()
{
    int fd_client = accept(get_listen_socket_fd(), NULL, NULL);

    if (fd_client < 0)
        return -1;
    if (fcntl(fd_client, F_SETFL, O_NONBLOCK) == -1){
        close(fd_client);
        return -1;
    }
    return fd_client;
}