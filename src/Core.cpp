#include "Core.hpp"

/**
 * @brief Глобальная переменная для отслеживания статуса сигналов
 */
volatile sig_atomic_t Core::g_signal_status = 0;

/**
 * @brief Обработчик сигналов (SIGINT)
 * @param signum Номер полученного сигнала
 *
 * Устанавливает флаг, сигнализирующий о необходимости завершить программу.
 * Используется для корректного выхода из event loop при нажатии Ctrl+C.
 */
void signal_handler(int signum)
{
    if (signum == SIGINT)
    {
        Core::g_signal_status = signum;
    }
}

/**
 * @brief Конструктор Core
 * @param port Номер порта для прослушивания входящих соединений
 *
 * Создаёт слушающий сокет и добавляет его в карту активных сокетов.
 */
Core::Core(int port)
{
    ListeningSocket *server_socket = new ListeningSocket(port);
    listening_sockets[server_socket->get_listen_socket_fd()] = server_socket;
}

/**
 * @brief Преобразует маску событий ClientSocket в маску для poll()
 * @param mask Маска из ClientSocket::Events (WANT_READ, WANT_WRITE)
 * @return Маска для poll() (POLLIN, POLLOUT)
 *
 * Преобразует внутренние флаги клиента в флаги, понятные для poll().
 */
short Core::translate_client_mask_in_posix(short mask)
{
    short result = 0;
    if (mask & ClientSocket::WANT_READ)
        result |= POLLIN;
    if (mask & ClientSocket::WANT_WRITE)
        result |= POLLOUT;
    return result;
}

/**
 * @brief Главный event loop сервера
 *
 * Основной цикл обработки событий:
 * 1. Заполняет массив фд для poll() из всех активных сокетов
 * 2. Вызывает poll() с таймаутом 1 секунда
 * 3. Обрабатывает готовые события (прием новых клиентов, чтение/запись)
 * 4. Удаляет закрытые соединения
 * 5. Повторяет, пока не получен сигнал SIGINT (Ctrl+C)
 */
void Core::core_loop()
{
    int ret = 0;
    signal(SIGINT, signal_handler);
    while (Core::g_signal_status == 0)
    {
        // Список файловых дескрипторов клиентов, готовых к удалению
        std::vector<int> delete_client;
        fds.clear();
        struct pollfd tmp_pollfd;
        // Добавляем в poll() все слушающие сокеты
        for (std::map<int, ListeningSocket *>::iterator it = listening_sockets.begin(); it != listening_sockets.end(); it++)
        {
            tmp_pollfd.fd = it->first;
            tmp_pollfd.events = POLLIN; // Ждём входящих соединений
            fds.push_back(tmp_pollfd);
        }
        // Добавляем в poll() все активные клиентские соединения
        // События зависят от состояния обработки (чтение, запись)
        for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
        {
            short want_events = translate_client_mask_in_posix(
                it->second->get_ready_events());
            tmp_pollfd.fd = it->first;
            tmp_pollfd.events = want_events;
            fds.push_back(tmp_pollfd);
        }

        // poll() ждёт готовых событий (таймаут 1000ms = 1 секунда)
        ret = poll(fds.data(), fds.size(), 1000);

        // Ошибка poll()
        if (ret < 0)
        {
            std::cerr << "ERROR Poll failed" << std::endl;
            continue;
        }

        // Таймаут poll() истёк, событий нет
        if (ret == 0)
            continue;

        // Обрабатываем все готовые события
        for (size_t i = 0; i < fds.size(); i++)
        {
            // Событие чтения (новое соединение или данные от клиента)
            if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
            {
                std::map<int, ListeningSocket *>::iterator it_listening =
                    listening_sockets.find(fds[i].fd);

                // Это слушающий сокет - приём нового клиента
                if (it_listening != listening_sockets.end())
                {
                    int client_fd = it_listening->second->accept_conection();
                    if (client_fd != -1)
                    {
                        client_sockets[client_fd] = new ClientSocket(client_fd);
                    }
                }
                // Это клиентский сокет - чтение данных
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
            // Событие записи - отправка данных клиенту
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
        // Находим готовые к удалению клиентские соединения
        for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
        {
            if (it->second->is_ready_delete())
                delete_client.push_back(it->first);
        }

        // Удаляем закрытые соединения
        for (size_t i = 0; i < delete_client.size(); i++)
        {
            int fd_client_delete = delete_client[i];
            delete client_sockets[fd_client_delete];
            client_sockets.erase(fd_client_delete);
        }
    }
}

/**
 * @brief Деструктор Core
 *
 * Закрывает все активные соединения (клиентские и слушающие сокеты)
 * и освобождает выделенную память.
 */
Core::~Core(void)
{
    // Удаляем все слушающие сокеты
    for (std::map<int, ListeningSocket *>::iterator it = listening_sockets.begin(); it != listening_sockets.end(); it++)
    {
        delete it->second;
    }
    // Удаляем все клиентские соединения
    for (std::map<int, ClientSocket *>::iterator it = client_sockets.begin(); it != client_sockets.end(); it++)
    {
        delete it->second;
    }
}