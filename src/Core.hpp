#ifndef CORE_HPP
#define CORE_HPP

#include <map>
#include <vector>
#include <iostream>
#include <poll.h>
#include "ListeningSocket.hpp"
#include "ClientSocket.hpp"
#include <algorithm>
#include <signal.h>

class Core
{
public:
    Core(int port);
    void core_loop();
    ~Core();
    static volatile sig_atomic_t g_signal_status;

private:
    Core();
    std::map<int, ClientSocket *> client_sockets;
    std::map<int, ListeningSocket *> listening_sockets;
    std::vector<struct pollfd> fds;
    Core(const Core &src);
    Core &operator=(const Core &rhs);
    short translate_client_mask_in_posix(short mask);
};

#endif // CORE_HPP