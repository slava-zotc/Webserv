#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <unistd.h>


class Socket
{
    public:
    Socket(int fd);
    ~Socket();
    int get_fd() const;
    
    private:
    const int	fd_;
    Socket();
    Socket(const Socket &src);
    Socket &operator=(const Socket &rhs);
};

#endif // SOCKET_HPP