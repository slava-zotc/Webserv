#include "ListeningSocket.hpp"

#include <netdb.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <sstream>
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
 * @brief Строка "host:port" для сообщений об ошибках
 */
static std::string endpoint(const std::string& host, int port)
{
	std::stringstream ss;
	ss << host << ":" << port;
	return ss.str();
}

/**
 * @brief Переводит host в IPv4-адрес и добавляет порт
 *
 * getaddrinfo понимает и "127.0.0.1", и "localhost". AF_INET - потому что
 * сокет создаётся как IPv4 (PF_INET). AI_PASSIVE - адрес нужен для bind().
 */
struct sockaddr_in ListeningSocket::resolve(const std::string& host, int port)
{
	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo* res = NULL;
	int rc = getaddrinfo(host.c_str(), NULL, &hints, &res);
	if (rc != 0 || res == NULL)
	{
		throw std::runtime_error("Cannot resolve listen host "
								 + endpoint(host, port) + ": "
								 + gai_strerror(rc));
	}
	struct sockaddr_in addr;
	std::memcpy(&addr, res->ai_addr, sizeof(addr));
	freeaddrinfo(res);
	addr.sin_port = htons(port);  // порт в сетевом порядке байт
	return addr;
}

/**
 * @brief Конструктор. Настраивает слушающий сокет
 * @param host Интерфейс ("0.0.0.0" = все)
 * @param port Номер порта для прослушивания
 * @throw std::runtime_error если ошибка при bind() или listen()
 *
 * Если конструктор бросит исключение, fd всё равно закроется: член
 * fd_socket уже полностью сконструирован, и C++ вызовет его деструктор.
 */
ListeningSocket::ListeningSocket(const std::string& host, int port)
	: fd_socket(create_non_blocking_socket_fd())
{
	int server_fd = get_listen_socket_fd();
	struct sockaddr_in addr = resolve(host, port);

	// SO_REUSEADDR: после перезапуска сервера порт занят ещё ~60 с
	// (TIME_WAIT); без опции bind() в это время бы падал.
	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))
		== -1)
		std::cerr << "Warning: setsockopt(SO_REUSEADDR) failed" << std::endl;

	// errno здесь читать можно: запрет subject касается только read/write.
	if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
		throw std::runtime_error("Cannot listen on " + endpoint(host, port)
								 + ": " + std::strerror(errno));

	// Очередь ещё не принятых соединений (backlog)
	if (listen(server_fd, 128) == -1)
		throw std::runtime_error("Cannot listen on " + endpoint(host, port)
								 + ": " + std::strerror(errno));
}

/**
 * @brief Деструктор
 */
ListeningSocket::~ListeningSocket(void)
{
}

/**
 * @brief IPv4-адрес в виде "a.b.c.d"
 *
 * inet_ntoa в subject не разрешён, поэтому собираем строку сами:
 * ntohl переводит адрес в порядок байт хоста, старший байт - первое число.
 */
static std::string ipv4_to_string(const struct sockaddr_in& addr)
{
	unsigned long ip = ntohl(addr.sin_addr.s_addr);
	std::stringstream ss;
	ss << ((ip >> 24) & 0xFF) << "." << ((ip >> 16) & 0xFF) << "."
	   << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF);
	return ss.str();
}

/**
 * @brief Принимает входящее клиентское соединение
 * @param client_ip [out] Адрес клиента для CGI (REMOTE_ADDR)
 * @return Файловый дескриптор нового соединения или -1 при ошибке
 *
 * Новые клиентские соединения также устанавливаются в неблокирующий режим.
 */
int ListeningSocket::accept_conection(std::string& client_ip)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	std::memset(&addr, 0, sizeof(addr));

	int fd_client =
		accept(get_listen_socket_fd(), (struct sockaddr*)&addr, &addr_len);

	// Ошибка accept() или non-blocking режим (нет данных)
	if (fd_client < 0) return -1;
	// Устанавливаем non-blocking режим для клиента
	if (fcntl(fd_client, F_SETFL, O_NONBLOCK) == -1)
	{
		close(fd_client);
		return -1;
	}
	client_ip = ipv4_to_string(addr);
	return fd_client;
}
