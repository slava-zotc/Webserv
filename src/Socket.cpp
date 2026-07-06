#include "Socket.hpp"


Socket::Socket(int fd) : fd_(fd)
{
}


Socket::~Socket(void)
{
    close(fd_);
}

int Socket::get_fd() const {
    return fd_;
}