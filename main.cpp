#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>

int main()
{
    int server_fd = socket(PF_INET, SOCK_STREAM, 0);

    if (server_fd == -1)
        return 1;

    struct sockaddr_in adrr;
    adrr.sin_family = PF_INET;
    adrr.sin_port = htons(8080);
    adrr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&adrr, sizeof(adrr)) == -1)
        return -1;
    if (listen(server_fd, 128) == -1)
        return -1;

    std::cout << "SErver listening on port 8080" << std::endl;

    struct sockaddr_in client_address;
    socklen_t adrr_size = sizeof(client_address);
    std::vector<int> client_fd;
    int tmp_fd;
    while (true)
    {
        tmp_fd = accept(server_fd, (struct sockaddr *)&client_address, &adrr_size);
        if (tmp_fd < 0)
            std::cout << "HANDLE ERROR" << std::endl;
        client_fd.push_back(tmp_fd);
        std::cout << client_fd.back() << std::endl;
        char temp_bufer[4096];
        std::string mesage = "";
        int byte_recv = recv(tmp_fd, temp_bufer, sizeof(temp_bufer), 0);
        mesage.append(temp_bufer, byte_recv);
        std::cout << mesage << std::endl;
        std::string response("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!");
        int byte_send = send(tmp_fd, response.c_str(), response.size(), 0);
        if (byte_send == -1)
            return -1;
    }
}