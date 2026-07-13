#ifndef LISTENING_SOCKET_HPP
#define LISTENING_SOCKET_HPP

#include "Socket.hpp"
#include <sys/socket.h>
#include <stdexcept>
#include <fcntl.h>
#include <netinet/in.h>

/**
 * @class ListeningSocket
 * @brief Слушающий сокет для приёма входящих HTTP-соединений.
 *
 * Создаёт неблокирующий TCP-сокет, привязанный к указанному порту,
 * и принимает входящие соединения от клиентов.
 */
class ListeningSocket
{
public:
    /**
     * @brief Конструктор. Создаёт и настраивает слушающий сокет.
     * @param port Номер порта для прослушивания (0-65535)
     * @throw std::runtime_error если ошибка при создании/конфигурировании сокета
     */
    ListeningSocket(int port);

    /**
     * @brief Деструктор. Закрывает слушающий сокет.
     */
    ~ListeningSocket();

    /**
     * @brief Принимает входящее клиентское соединение
     * @return Файловый дескриптор нового клиентского сокета или -1 при ошибке
     */
    int accept_conection();

    /**
     * @brief Возвращает файловый дескриптор слушающего сокета
     * @return Файловый дескриптор
     */
    int get_listen_socket_fd() const;

private:
    /**
     * @brief Создаёт неблокирующий TCP-сокет
     * @return Файловый дескриптор нового сокета
     * @throw std::runtime_error если ошибка при socket() или fcntl()
     */
    static int create_non_blocking_socket_fd();

    ListeningSocket(const ListeningSocket &src);
    ListeningSocket &operator=(const ListeningSocket &rhs);
    ListeningSocket();
    Socket fd_socket;
};

#endif // LISTENING_SOCKET_HPP