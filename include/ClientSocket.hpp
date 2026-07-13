#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

#include "Socket.hpp"
#include <string>
#include <iostream>
#include <unistd.h>

/**
 * @class ClientSocket
 * @brief Управляет HTTP-соединением с одним клиентом.
 *
 * Реализует конечный автомат для обработки HTTP-запросов и ответов.
 * Состояния: READ_HEADERS -> READ_BODY -> PROCESS -> READY_SEND -> READY_DELETE
 */
class ClientSocket
{
public:
    /**
     * @enum Events
     * @brief Маски событий для poll() - какие операции готовы
     */
    enum Events
    {
        WANT_READ = 1 << 0, ///< Готов читать данные из сокета
        WANT_WRITE = 1 << 1 ///< Готов писать данные в сокет
    };
    /**
     * @brief Деструктор ClientSocket
     */
    ~ClientSocket();

    /**
     * @brief Конструктор ClientSocket
     * @param fd Файловый дескриптор клиентского сокета
     */
    ClientSocket(int fd);

    /**
     * @brief Возвращает маску готовых событий для poll()
     * @return Комбинация WANT_READ и/или WANT_WRITE
     */
    short get_ready_events() const;

    /**
     * @brief Проверяет, готов ли сокет к отправке ответа
     * @return true если есть данные для отправки, false иначе
     */
    bool is_ready_send() const;

    /**
     * @brief Проверяет, находится ли клиент в фазе чтения
     * @return true если клиент ещё читает запрос, false если данные полностью получены
     */
    bool is_reading_phase() const;

    /**
     * @brief Проверяет, готов ли сокет к удалению
     * @return true если соединение закончено, false иначе
     */
    bool is_ready_delete() const;

    /**
     * @brief Обрабатывает получение данных от клиента
     * Читает данные, обновляет состояние автомата, готовит ответ
     */
    void handle_read();

    /**
     * @brief Обрабатывает отправку ответа клиенту
     * Отправляет HTTP-ответ частями, отслеживает прогресс
     */
    void handle_write();

    /**
     * @brief Возвращает файловый дескриптор клиентского сокета
     * @return Файловый дескриптор
     */
    int get_client_socket_fd() const;

private:
    enum State
    {
        READ_HEADERS,
        READ_BODY,
        PROCESS,
        READY_SEND,
        READY_DELETE
    };
    ClientSocket();
    ClientSocket(const ClientSocket &src);
    ClientSocket &operator=(const ClientSocket &rhs);
    Socket socket_fd;
    std::string response_buffer;
    std::string request_buffer;
    std::string::size_type partial_write;
    State state_client;
};

#endif // CLIENT_SOCKET_HPP