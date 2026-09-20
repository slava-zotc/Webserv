#include "Core.hpp"

#include <csignal>

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
	ListeningSocket* server_socket = new ListeningSocket(port);
	listening_sockets[server_socket->get_listen_socket_fd()] = server_socket;
}

Core::Core(std::vector<Server*>& servers)
{
	for (size_t i = 0; i < servers.size(); i++)
	{
		ListeningSocket* server_socket =
			new ListeningSocket(servers[i]->get_port());
		int fd = server_socket->get_listen_socket_fd();
		listening_sockets[fd] = server_socket;
		server_for_listening_fd[fd] = servers[i];
	}
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
	if (mask & ClientSocket::WANT_READ) result |= POLLIN;
	if (mask & ClientSocket::WANT_WRITE) result |= POLLOUT;
	return result;
}

void Core::fill_pollfds()
{
	fds.clear();
	struct pollfd tmp_pollfd;
	// Добавляем в poll() все слушающие сокеты
	for (std::map<int, ListeningSocket*>::iterator it =
			 listening_sockets.begin();
		 it != listening_sockets.end(); it++)
	{
		tmp_pollfd.fd = it->first;
		tmp_pollfd.events = POLLIN;	 // Ждём входящих соединений
		fds.push_back(tmp_pollfd);
	}
	// Добавляем в poll() все активные клиентские соединения
	// События зависят от состояния обработки (чтение, запись)
	for (std::map<int, ClientConnection>::iterator it = client_sockets.begin();
		 it != client_sockets.end(); it++)
	{
		short want_events = translate_client_mask_in_posix(
			it->second.socket->get_ready_events());
		tmp_pollfd.fd = it->first;
		tmp_pollfd.events = want_events;
		fds.push_back(tmp_pollfd);
	}
}

int Core::accept_new_client(int indx)
{
	std::map<int, ListeningSocket*>::iterator it_listening =
		listening_sockets.find(fds[indx].fd);

	if (it_listening == listening_sockets.end()) return 0;
	// Это слушающий сокет - приём нового клиента

	int client_fd = it_listening->second->accept_conection();
	if (client_fd != -1)
	{
		ClientConnection conn;
		conn.socket = new ClientSocket(client_fd);
		conn.server = server_for_listening_fd[fds[indx].fd];
		client_sockets[client_fd] = conn;
		return 1;
	}
	return -1;
}

void Core::dispatch_client_events(short revents, int fd)
{
	if (revents & (POLLIN | POLLERR | POLLHUP))
	{
		std::map<int, ClientConnection>::iterator it_client =
			client_sockets.find(fd);

		if (it_client != client_sockets.end())
		{
			it_client->second.socket->handle_read(*(it_client->second.server));
		}
	}

	if (revents & POLLOUT)
	{
		std::map<int, ClientConnection>::iterator it_client =
			client_sockets.find(fd);

		if (it_client != client_sockets.end())
		{
			if (!it_client->second.socket->is_ready_delete())
				it_client->second.socket->handle_write();
		}
	}
}

void Core::cleanup_closed_connections()
{
	// Список файловых дескрипторов клиентов, готовых к удалению
	std::vector<int> delete_client;
	for (std::map<int, ClientConnection>::iterator it = client_sockets.begin();
		 it != client_sockets.end(); it++)
	{
		if (it->second.socket->is_ready_delete())
			delete_client.push_back(it->first);
	}

	// Удаляем закрытые соединения
	for (size_t i = 0; i < delete_client.size(); i++)
	{
		int fd_client_delete = delete_client[i];
		delete client_sockets[fd_client_delete].socket;
		client_sockets.erase(fd_client_delete);
	}
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
	signal(SIGPIPE, SIG_IGN);
	while (Core::g_signal_status == 0)
	{
		fill_pollfds();

		// poll() ждёт готовых событий (таймаут 1000ms = 1 секунда)
		ret = poll(fds.data(), fds.size(), 1000);

		// Ошибка poll()
		if (ret < 0)
		{
			std::cerr << "ERROR Poll failed" << std::endl;
			continue;
		}

		// Таймаут poll() истёк, событий нет
		if (ret == 0) continue;

		// Обрабатываем все готовые события
		for (size_t i = 0; i < fds.size(); i++)
		{
			// Событие чтения (новое соединение или данные от клиента)
			if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
			{
				if (accept_new_client(i) != 0)
					continue;
			}
			// Событие записи - отправка данных клиенту
			dispatch_client_events(fds[i].revents, fds[i].fd);
		}
		// Находим готовые к удалению клиентские соединения
		cleanup_closed_connections();
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
	for (std::map<int, ListeningSocket*>::iterator it =
			 listening_sockets.begin();
		 it != listening_sockets.end(); it++)
	{
		delete it->second;
	}
	// Удаляем все клиентские соединения
	for (std::map<int, ClientConnection>::iterator it = client_sockets.begin();
		 it != client_sockets.end(); it++)
	{
		delete it->second.socket;
	}
}
