#include "Core.hpp"

volatile sig_atomic_t Core::g_signal_status = 0;

void signal_handler(int signum)
{
     if (signum == SIGINT)
     {
        Core::g_signal_status = signum;
     }
}

Core::Core(int port)
{
    ListeningSocket *server_socket = new ListeningSocket(port);
    listening_sockets[server_socket->get_listen_socket_fd()] = server_socket;
}

short Core::translate_client_mask_in_posix(short mask)
{
    short result = 0;
    if (mask & ClientSocket::WANT_READ)
        result |= POLLIN;
    if (mask & ClientSocket::WANT_WRITE)
        result |= POLLOUT;
    return result;
}

void Core::core_loop()
{
    int ret = 0;
    signal(SIGINT, signal_handler);
    while (Core::g_signal_status == 0)
    {

        std::vector<int> delete_client;
        fds.clear();
        struct pollfd tmp_pollfd;
        for (std::map<int, ListeningSocket *>::iterator it = listening_sockets.begin(); it != listening_sockets.end(); it++)
        {
            tmp_pollfd.fd = it->first;
            tmp_pollfd.events = POLLIN;
            fds.push_back(tmp_pollfd);
        }
        for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
        {
            short want_events = translate_client_mask_in_posix(
                it->second->get_ready_events());
            tmp_pollfd.fd = it->first;
            tmp_pollfd.events = want_events;
            fds.push_back(tmp_pollfd);
        }

        ret = poll(fds.data(), fds.size(), 1000);

        if (ret < 0)
        {
            std::cerr << "ERROR Poll failed" << std::endl;
            continue;
        }

        if (ret == 0)
            continue;

        for (size_t i = 0; i < fds.size(); i++)
        {
            if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
            {
                std::map<int, ListeningSocket *>::iterator it_listening =
                    listening_sockets.find(fds[i].fd);

                if (it_listening != listening_sockets.end())
                {
                    int client_fd = it_listening->second->accept_conection();
                    if (client_fd != -1)
                    {
                        client_sockets[client_fd] = new ClientSocket(client_fd);
                    }
                }
                else
                {
                    std::map<int, ClientSocket *>::iterator it_client =
                        client_sockets.find(fds[i].fd);

                    if (it_client != client_sockets.end())
                    {
                        it_client->second->handle_read();
                    }
                }
            }
            if (fds[i].revents & POLLOUT)
            {
                std::map<int, ClientSocket *>::iterator it_client =
                    client_sockets.find(fds[i].fd);

                if (it_client != client_sockets.end())
                {
                    it_client->second->handle_write();
                }
            }
        }
        for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
        {
            if (it->second->is_ready_delete())
                delete_client.push_back(it->first);
        }

        for (size_t i = 0; i < delete_client.size(); i++)
        {
            int fd_client_delete = delete_client[i];
            delete client_sockets[fd_client_delete];
            client_sockets.erase(fd_client_delete);
        }
    }
}

Core::~Core(void)
{
    for (std::map<int, ListeningSocket *>::iterator it = listening_sockets.begin(); it != listening_sockets.end(); it++)
    {
        delete it->second;
    }
    for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
    {
        delete it->second;
    }
}