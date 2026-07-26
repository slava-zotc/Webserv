#ifndef CORE_HPP
#define CORE_HPP

#include <poll.h>
#include <signal.h>

#include <map>
#include <vector>

#include "ClientSocket.hpp"
#include "ListeningSocket.hpp"

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
	/**
	 * @brief Конструктор. Инициализирует сервер и начинает слушать на порте.
	 * @param port Номер порта для прослушивания
	 */
	Core(int port);

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
	 * @brief Карта активных клиентских соединений (fd -> ClientSocket*)
	 */
	std::map<int, ClientSocket*> client_sockets;

	/**
	 * @brief Карта слушающих сокетов (fd -> ListeningSocket*)
	 */
	std::map<int, ListeningSocket*> listening_sockets;

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
};

#endif	// CORE_HPP
