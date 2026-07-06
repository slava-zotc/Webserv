#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

class ClientSocket
{
public:
    ClientSocket();
    ClientSocket(const ClientSocket &src);
    ClientSocket &operator=(const ClientSocket &rhs);
    ~ClientSocket();

private:
    int	value_;
};

#endif // CLIENT_SOCKET_HPP