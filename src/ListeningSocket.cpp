#include "ListeningSocket.hpp"

#include <sys/socket.h>

#include <cstring>
#include <iostream>

/**
 * @brief Статический метод для создания неблокирующего TCP-сокета
 * @return Новый файловый дескриптор
 * @throw std::runtime_error если ошибка
 *
 * Создаёт TCP-сокет и вустанавливает для него флаг non-blocking.
 */
int ListeningSocket::create_non_blocking_socket_fd()
{
	// Создаём IPv4 TCP сокет
	int server_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) throw std::runtime_error("ERROR func socket");

	// Устанавливаем non-blocking мод
	if (fcntl(server_fd, F_SETFL, O_NONBLOCK) == -1)
	{
		close(server_fd);
		throw std::runtime_error("ERROR func fcntl");
	}
	return server_fd;
}

/**
 * @brief Возвращает файловый дескриптор слушающего сокета
 * @return Файловый дескриптор
 */
int ListeningSocket::get_listen_socket_fd() const
{
	return fd_socket.get_fd();
}

/**
 * @brief Конструктор. Настраивает слушающий сокет
 * @param port Номер порта для прослушивания
 * @throw std::runtime_error если ошибка при bind() или listen()
 */
ListeningSocket::ListeningSocket(int port)
	: fd_socket(create_non_blocking_socket_fd())
{
	int server_fd = get_listen_socket_fd();

	// Настраиваем адрес для bind()
	struct sockaddr_in adrr;
	std::memset(&adrr, 0, sizeof(adrr));
	adrr.sin_family = AF_INET;	// IPv4
	adrr.sin_port =
		htons(port);  // Преобразуем номер порта в нетверковой формат
	adrr.sin_addr.s_addr = INADDR_ANY;	// Послушиваем все интерфейсы

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
		== -1)
		std::cerr << "Error func setsockopt" << std::endl;
	// Привязываем сокет к порту
	if (bind(server_fd, (struct sockaddr*)&adrr, sizeof(adrr)) == -1)
		throw std::runtime_error("ERROR func bind");

	// Настраиваем ковартилу входящих необработанных соединений
	// (128 соединения)
	if (listen(server_fd, 128) == -1)
		throw std::runtime_error("ERROR func listen");
}

/**
 * @brief Деструктор
 */
ListeningSocket::~ListeningSocket(void)
{
}

/**
 * @brief Принимает входящее клиентское соединение
 * @return Файловый дескриптор нового соединения или -1 при ошибке
 *
 * Новые клиентские соединения также устанавливаются в неблокирующий режим.
 */
int ListeningSocket::accept_conection()
{
	// Принимаем соединение
	int fd_client = accept(get_listen_socket_fd(), NULL, NULL);

	// Ошибка accept() или non-blocking режим (нет данных)
	if (fd_client < 0) return -1;
	// Устанавливаем non-blocking режим для клиента
	if (fcntl(fd_client, F_SETFL, O_NONBLOCK) == -1)
	{
		close(fd_client);
		return -1;
	}
	return fd_client;
}
