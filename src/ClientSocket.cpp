#include "ClientSocket.hpp"
#include <sys/socket.h>

ClientSocket::ClientSocket(int fd)
    : socket_fd(fd), partial_write(0), state_client(READ_HEADERS) {}

short ClientSocket::get_ready_events() const
{
    short mask = 0;
    if (is_ready_send())
        mask |= WANT_WRITE;
    if (is_reading_phase())
        mask |= WANT_READ;
    return mask;
}
bool ClientSocket::is_ready_send() const
{
    return partial_write < response_buffer.size();
}
bool ClientSocket::is_reading_phase() const
{
    return state_client != READY_DELETE && state_client != READY_SEND;
}
bool ClientSocket::is_ready_delete() const
{
    return state_client == READY_DELETE;
}
void ClientSocket::handle_read()
{
    char tmp_buffer[4096];
    int byte_recv = recv(socket_fd.get_fd(), tmp_buffer, sizeof(tmp_buffer), 0);
    if (byte_recv <= 0)
    {
        state_client = READY_DELETE;
        return;
    }
    request_buffer.append(tmp_buffer, byte_recv);
    response_buffer = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
    state_client = READY_SEND;
    std::cout << request_buffer; // LOG
}
void ClientSocket::handle_write()
{
    int byte_send = send(socket_fd.get_fd(),
                         response_buffer.c_str() + partial_write,
                         response_buffer.size() - partial_write, 0);
    if (byte_send == -1)
    {
        state_client = READY_DELETE;
        return;
    }
    partial_write += byte_send;
    if (partial_write == response_buffer.size())
    {
        partial_write = 0;
        state_client = READY_DELETE;
    }
}
int ClientSocket::get_client_socket_fd() const
{
    return socket_fd.get_fd();
}

ClientSocket::~ClientSocket() {}