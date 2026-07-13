#include "Socket.hpp"

/**
 * @brief Конструктор Socket
 * @param fd Файловый дескриптор сокета для обёртывания
 */
Socket::Socket(int fd) : fd_(fd)
{
}

/**
 * @brief Деструктор Socket. Закрывает файловый дескриптор.
 * Гарантирует освобождение системного ресурса при удалении объекта.
 */
Socket::~Socket(void)
{
    close(fd_);
}

/**
 * @brief Возвращает файловый дескриптор
 * @return Файловый дескриптор, переданный при конструировании
 */
int Socket::get_fd() const
{
    return fd_;
}