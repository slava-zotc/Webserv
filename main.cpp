#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

void accept_connection()
{
}

void core_loop(int server_fd)
{
    std::vector<struct pollfd> fds(1);
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    struct pollfd tmp_pollfd;
    int ret;

    while (true)
    {
        ret = poll(fds.data(), fds.size(), 1000);

        if (ret < 0)
        {
            std::cerr << "ERROR Poll failed" << std::endl;
            return;
        }
        if (ret == 0)
        {
            continue;
        }

        if (fds[0].revents & POLLIN)
        {

            int fd_client = accept(server_fd, NULL, NULL);

            if (fd_client < 0)
                return; //! TODO Handke error
            if (fcntl(fd_client, F_SETFL, O_NONBLOCK) == -1)
                return; // handle error
            tmp_pollfd.fd = fd_client;
            tmp_pollfd.events = POLLIN;

            fds.push_back(tmp_pollfd);
        }

        for (size_t i = 1; i < fds.size() && ret != 0; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                char tmp_buffer[4096];
                std::string mesage = "";
                int byte_recv = recv(fds[i].fd, tmp_buffer, sizeof(tmp_buffer), 0);
                mesage.append(tmp_buffer, byte_recv);
                std::cout << mesage << std::endl;
                std::string response("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!");
                int byte_send = send(fds[i].fd, response.c_str(), response.size(), 0);
                if (byte_send == -1)
                    return; //! TODO Handke error
                close(fds[i].fd);
                fds.erase(fds.begin() + i);
                i--;
            }
        }
    }
}

int main()
{
    int server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
        return 1;
    if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1)
        return -1;
    struct sockaddr_in adrr;
    adrr.sin_family = PF_INET;
    adrr.sin_port = htons(8080);
    adrr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&adrr, sizeof(adrr)) == -1)
        return -1;
    if (listen(server_fd, 128) == -1)
        return -1;

    std::cout << "SErver listening on port 8080" << std::endl;

    core_loop(server_fd);
}