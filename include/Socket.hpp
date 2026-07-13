#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <unistd.h>

/**
 * @class Socket
 * @brief Базовый класс-обёртка для работы с файловыми дескрипторами сокетов.
 *
 * Обеспечивает RAII-парадигму для управления ресурсами файловых дескрипторов.
 * Гарантирует закрытие дескриптора при уничтожении объекта.
 */
class Socket
{
public:
    /**
     * @brief Конструктор Socket
     * @param fd Файловый дескриптор сокета
     */
    Socket(int fd);

    /**
     * @brief Деструктор. Закрывает файловый дескриптор.
     */
    ~Socket();

    /**
     * @brief Возвращает файловый дескриптор сокета
     * @return Файловый дескриптор
     */
    int get_fd() const;

private:
    const int fd_;
    Socket();
    Socket(const Socket &src);
    Socket &operator=(const Socket &rhs);
};

#endif // SOCKET_HPP