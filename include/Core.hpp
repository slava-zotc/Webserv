#ifndef CORE_HPP
#define CORE_HPP

#include <poll.h>
#include <signal.h>

#include <iostream>
#include <map>
#include <vector>

#include "ClientSocket.hpp"
#include "ListeningSocket.hpp"
#include "Server.hpp"

/**
 * @struct ClientConnection
 * @brief Связывает клиентский сокет с сервером, к которому он подключился.
 *
 * Core — единственное место, которое знает, какому Server принадлежит
 * тот или иной клиент. ClientSocket сам по себе про Server ничего не знает.
 */
struct ClientConnection
{
	ClientSocket* socket;
	const Server* server;
};

/**
 * @class Core
 * @brief Главный event loop сервера.
 *
 * Управляет слушающими сокетами и всеми активными клиентскими соединениями.
 * Использует poll() для асинхронной обработки множественных соединений.
 * Перехватывает SIGINT для корректного завершения работы.
 */
class Core
{
public:


	Core(std::vector<Server*>& servers);
	/**
	 * @brief Главный event loop сервера.
	 * Обрабатывает входящие соединения, чтение/запись данных, закрытие
	 * соединений. Продолжает работать до получения сигнала SIGINT.
	 */
	void core_loop();

	/**
	 * @brief Деструктор. Закрывает все сокеты и освобождает ресурсы.
	 */
	~Core();

	/**
	 * @brief Флаг для отслеживания сигналов (SIGINT)
	 */
	static volatile sig_atomic_t g_signal_status;

private:
	/**
	 * @brief Приватный конструктор по умолчанию (запрещён)
	 */
	Core();

	/**
	 * @brief Карта соответствия CGI fd -> client fd
	 */
	std::map<int, int> cgi_fd_to_client_fd;

	/**
	 * @brief Карта соответствия fd stdin CGI (пишем тело) -> client fd
	 */
	std::map<int, int> cgi_write_fd_to_client_fd;	///< Карта соответствия CGI fd -> client fd

	/**
	 * @brief Карта активных клиентских соединений (fd -> ClientConnection)
	 */
	std::map<int, ClientConnection> client_sockets;

	/**
	 * @brief Карта слушающих сокетов (fd -> ListeningSocket*)
	 */
	std::map<int, ListeningSocket*> listening_sockets;

	std::map<int, Server*> server_for_listening_fd;

	/**
	 * @brief Запасной fd на случай, когда fd у процесса закончились
	 *
	 * Если accept() падает с EMFILE, соединение остаётся в очереди, и poll()
	 * сразу снова будит нас - сервер крутится вхолостую на 100% CPU.
	 * Тогда закрываем запасной fd, принимаем лишнего клиента, сразу его
	 * закрываем (очередь пустеет) и снова занимаем запасной fd.
	 */
	int reserve_fd;
	void shed_connection(int listen_fd);
	
	/**
	 * @brief Массив структур pollfd для poll()
	 */
	std::vector<struct pollfd> fds;

	/**
	 * @brief Приватный конструктор копирования (запрещён)
	 */
	Core(const Core& src);

	/**
	 * @brief Приватный оператор присваивания (запрещён)
	 */
	Core& operator=(const Core& rhs);

	/**
	 * @brief Преобразует маску событий ClientSocket в маску poll()
	 * @param mask Маска из ClientSocket::Events
	 * @return Маска для poll() (POLLIN, POLLOUT)
	 */
	short translate_client_mask_in_posix(short mask);

	void fill_pollfds();
	int accept_new_client(int indx);
	void dispatch_client_events(short revents, int fd);
	void cleanup_closed_connections();
};

#endif	// CORE_HPP
