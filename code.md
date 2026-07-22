#ifndef CLIENT_SOCKET_HPP
#define CLIENT_SOCKET_HPP

#include <unistd.h>

#include <string>

#include "HttpRequest.hpp"
#include "Socket.hpp"

class ClientSocket
{
public:
	enum Events
	{
		WANT_READ = 1 << 0,	 ///< Готов читать данные из сокета
		WANT_WRITE = 1 << 1	 ///< Готов писать данные в сокет
	};
	~ClientSocket();

	ClientSocket(int fd);

	short get_ready_events() const;

	bool is_ready_send() const;

	bool is_reading_phase() const;

	bool is_ready_delete() const;

	void handle_read();

	void handle_write();

	int get_client_socket_fd() const;

private:
	enum State
	{
		READING,
		READY_SEND,
		READY_DELETE
	};
	ClientSocket();
	ClientSocket(const ClientSocket& src);
	ClientSocket& operator=(const ClientSocket& rhs);
	Socket socket_fd;
	std::string response_buffer;
	HttpRequest request;
	std::string::size_type partial_write;
	State state_client;
};

#endif	// CLIENT_SOCKET_HPP
#ifndef CORE_HPP
#define CORE_HPP

#include <map>
#include <vector>
#include <iostream>
#include <poll.h>
#include "ListeningSocket.hpp"
#include "ClientSocket.hpp"
#include <algorithm>
#include <signal.h>

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
     * Обрабатывает входящие соединения, чтение/запись данных, закрытие соединений.
     * Продолжает работать до получения сигнала SIGINT.
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
    std::map<int, ClientSocket *> client_sockets;

    /**
     * @brief Карта слушающих сокетов (fd -> ListeningSocket*)
     */
    std::map<int, ListeningSocket *> listening_sockets;

    /**
     * @brief Массив структур pollfd для poll()
     */
    std::vector<struct pollfd> fds;

    /**
     * @brief Приватный конструктор копирования (запрещён)
     */
    Core(const Core &src);

    /**
     * @brief Приватный оператор присваивания (запрещён)
     */
    Core &operator=(const Core &rhs);

    /**
     * @brief Преобразует маску событий ClientSocket в маску poll()
     * @param mask Маска из ClientSocket::Events
     * @return Маска для poll() (POLLIN, POLLOUT)
     */
    short translate_client_mask_in_posix(short mask);
};

#endif // CORE_HPP#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <map>
#include <string>

class HttpRequest
{
public:
	enum Methods
	{
		GET,
		POST,
		DELETE,
		UNKNOWN
	};
	enum ParsingState
	{
		PARSING_START,
		PARSING_HEADERS,
		PARSING_BODY,
		PARSING_DONE,
		PARSING_ERROR
	};

	HttpRequest();
	ParsingState parse(const std::string& data);
	const std::string& get_path() const;
	const std::string& get_version() const;
	const std::string& get_body() const;
	const std::map<std::string, std::string>& get_headers() const;
	Methods get_method() const;
	ParsingState get_parsing_state() const;
	~HttpRequest();

private:
	std::string internal_buffer_;
	Methods method_;
	ParsingState parsing_state_;
	std::string path_;
	std::string version_;
	std::string body_;
	std::map<std::string, std::string> headers_;
	size_t content_length_;

	void process_start_line(const std::string& line);
	Methods convert_method_str(const std::string& method_str);

	void process_header_line(const std::string& line);
	HttpRequest(const HttpRequest& src);
	HttpRequest& operator=(const HttpRequest& rhs);
};

#endif	// HTTP_REQUEST_HPP
#ifndef HTTP_RESPONSE
#define HTTP_RESPONSE

#include <map>
#include <string>

class HttpResponse
{
public:
	HttpResponse(int status_code);
	std::string serialize() const;
	void set_header(const std::string& key, const std::string& value);
	void set_body(const std::string& body);
	~HttpResponse();

private:
	int status_code_;
	std::string reason_phrase_;
	std::map<std::string, std::string> headers_;
	std::string body_;
	std::string http_version_;

	std::string get_reason_phrase(int status_code);
	HttpResponse();
	HttpResponse(const HttpResponse& src);
	HttpResponse& operator=(const HttpResponse& rhs);
};
#endif	// !HTTP_RESPONSE
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

#endif // LISTENING_SOCKET_HPP#ifndef SOCKET_HPP
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

#endif // SOCKET_HPP#include "ClientSocket.hpp"

#include <sys/socket.h>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
/**
 * @brief Конструктор ClientSocket
 * @param fd Файловый дескриптор клиентского сокета
 *
 * Инициализирует клиентское соединение в начальном состоянии (READ_HEADERS).
 */
ClientSocket::ClientSocket(int fd)
	: socket_fd(fd), partial_write(0), state_client(READING)
{
}

/**
 * @brief Возвращает маску готовых событий
 * @return Комбинация флагов WANT_READ/WANT_WRITE в зависимости от статуса
 * обработки
 */
short ClientSocket::get_ready_events() const
{
	short mask = 0;
	if (is_ready_send()) mask |= WANT_WRITE;	// Эта клиент готов до отправки
	if (is_reading_phase()) mask |= WANT_READ;	// Равно, я может читать ещё
	return mask;
}
/**
 * @brief Проверяет, готов ли ответ к отправке
 * @return true если это данные готовые к отправке
 */
bool ClientSocket::is_ready_send() const
{
	return partial_write < response_buffer.size();
}
/**
 * @brief Проверяет, находится ли клиент в фазе чтения
 * @return true если эта не читает данные
 */
bool ClientSocket::is_reading_phase() const
{
	return state_client == READING;
}
/**
 * @brief Проверяет, готов ли сокет к удалению
 * @return true если соединение закончено
 */
bool ClientSocket::is_ready_delete() const
{
	return state_client == READY_DELETE;
}
/**
 * @brief Обрабатывает получение данных от клиента
 *
 * Читает данные из сокета, примец ресурс и генерирует HTTP ответ.
 * В актуальной имплементации ответ - это постоянная строка "Hello, World!".
 */
void ClientSocket::handle_read()
{
	char tmp_buffer[4096];

	int byte_recv = recv(socket_fd.get_fd(), tmp_buffer, sizeof(tmp_buffer), 0);
	// Клиент рассоединился или ошибка
	if (byte_recv <= 0)
	{
		state_client = READY_DELETE;
		return;
	}
	std::string recv_string(tmp_buffer, byte_recv);
	// Скопируем данные в буфер ресурс
	request.parse(recv_string);
	// Временно до роутера
	if (request.get_parsing_state() == HttpRequest::PARSING_DONE)
	{
		HttpResponse response(200);
		response.set_body("Hello, World!");
		response_buffer = response.serialize();
	}
	else if (request.get_parsing_state() == HttpRequest::PARSING_ERROR)
	{
		HttpResponse response(400);
		response_buffer = response.serialize();
	}
	else
		return;
	// На данный момент генерируем тривиальный HTTP ответ
	state_client = READY_SEND;	// Переводим в режим отправки
}
/**
 * @brief Обрабатывает отправку HTTP-ответа клиенту
 *
 * Отправляет данные блоками и отслеживает прогресс.
 * Нормально все данные передаются с одно призыва.
 */
void ClientSocket::handle_write()
{
	// Отправляем неотправленную часть ответа
	int byte_send =
		send(socket_fd.get_fd(), response_buffer.c_str() + partial_write,
			 response_buffer.size() - partial_write, 0);
	// Ошибка жати
	if (byte_send == -1)
	{
		state_client = READY_DELETE;
		return;
	}
	// Обновляем номер невыдаченных байтов
	partial_write += byte_send;
	// Проверяем, не отправлен ли все данные
	if (partial_write == response_buffer.size())
	{
		partial_write = 0;
		state_client = READY_DELETE;  // Да это соединение достаточно
	}
}
/**
 * @brief Возвращает файловый дескриптор клиентского сокета
 * @return Файловый дескриптор
 */
int ClientSocket::get_client_socket_fd() const
{
	return socket_fd.get_fd();
}

/**
 * @brief Деструктор ClientSocket
 */
ClientSocket::~ClientSocket()
{
}
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
		// Список файловых дескрипторов клиентов, готовых к удалению
		std::vector<int> delete_client;
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
		for (std::map<int, ClientSocket*>::iterator it = client_sockets.begin();
			 it != client_sockets.end(); it++)
		{
			short want_events =
				translate_client_mask_in_posix(it->second->get_ready_events());
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
		if (ret == 0) continue;

		// Обрабатываем все готовые события
		for (size_t i = 0; i < fds.size(); i++)
		{
			// Событие чтения (новое соединение или данные от клиента)
			if (fds[i].revents & (POLLIN | POLLERR | POLLHUP))
			{
				std::map<int, ListeningSocket*>::iterator it_listening =
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
					std::map<int, ClientSocket*>::iterator it_client =
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
				std::map<int, ClientSocket*>::iterator it_client =
					client_sockets.find(fds[i].fd);

				if (it_client != client_sockets.end())
				{
					if (!it_client->second->is_ready_delete())
						it_client->second->handle_write();
				}
			}
		}
		// Находим готовые к удалению клиентские соединения
		for (std::map<int, ClientSocket*>::iterator it = client_sockets.begin();
			 it != client_sockets.end(); it++)
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
	for (std::map<int, ListeningSocket*>::iterator it =
			 listening_sockets.begin();
		 it != listening_sockets.end(); it++)
	{
		delete it->second;
	}
	// Удаляем все клиентские соединения
	for (std::map<int, ClientSocket*>::iterator it = client_sockets.begin();
		 it != client_sockets.end(); it++)
	{
		delete it->second;
	}
}
#include "HttpRequest.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

HttpRequest::HttpRequest(void)
	: method_(UNKNOWN), parsing_state_(PARSING_START), content_length_(0)
{
}

HttpRequest::Methods HttpRequest::convert_method_str(
	const std::string& method_str)
{
	if (method_str == "GET")
	{
		return GET;
	}
	else if (method_str == "POST")
	{
		return POST;
	}
	else if (method_str == "DELETE")
	{
		return DELETE;
	}
	else
	{
		return UNKNOWN;
	}
}
void HttpRequest::process_header_line(const std::string& line)
{
	std::string key;
	std::string value;
	if (line.empty())
	{
		if (headers_.count("content-length") > 0)
		{
			std::stringstream ss(headers_["content-length"]);
			long long tmp_content_length;
			ss >> tmp_content_length;
			if (ss.fail() || !ss.eof() || tmp_content_length < 0)
			{
				parsing_state_ = PARSING_ERROR;
				return;
			}
			content_length_ = static_cast<size_t>(tmp_content_length);
			if (content_length_ > 0)
				parsing_state_ = PARSING_BODY;
			else
				parsing_state_ = PARSING_DONE;
		}
		else
		{
			parsing_state_ = PARSING_DONE;
		}
		// TODO Написать обработчик для chanked transfer-encoding
		return;
	}
	size_t pos = line.find(':');
	if (pos == std::string::npos)
	{
		parsing_state_ = PARSING_ERROR;
		return;
	}
	key = line.substr(0, pos);
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);
	value = line.substr(pos + 1);
	size_t first_not_space = value.find_first_not_of("\t ");
	if (first_not_space == std::string::npos)
	{
		value = "";
	}
	else
	{
		value = value.substr(first_not_space);
	}
	headers_[key] = value;
}

void HttpRequest::process_start_line(const std::string& line)
{
	std::stringstream ss(line);
	std::string method_str, path_str, version_str;

	ss >> method_str >> path_str >> version_str;
	if (method_str.empty() || path_str.empty() || version_str.empty())
	{
		parsing_state_ = PARSING_ERROR;
		return;
	}
	method_ = convert_method_str(method_str);
	path_ = path_str;
	version_ = version_str;
	parsing_state_ = PARSING_HEADERS;
}

HttpRequest::ParsingState HttpRequest::parse(const std::string& data)
{
	internal_buffer_ += data;

	while (parsing_state_ == PARSING_START || parsing_state_ == PARSING_HEADERS)
	{
		size_t pos = internal_buffer_.find("\r\n");
		if (std::string::npos == pos)
		{
			break;
		}
		std::string line = internal_buffer_.substr(0, pos);
		internal_buffer_.erase(0, pos + 2);
		if (parsing_state_ == PARSING_START)
		{
			if (!line.empty()) process_start_line(line);
		}
		else if (parsing_state_ == PARSING_HEADERS)
		{
			process_header_line(line);
		}
	}
	if (parsing_state_ == PARSING_BODY)
	{
		size_t need_byte = content_length_ - body_.size();
		size_t to_copy = std::min(internal_buffer_.size(), need_byte);

		body_.append(internal_buffer_, 0, to_copy);
		internal_buffer_.erase(0, to_copy);
		if (body_.size() == content_length_) parsing_state_ = PARSING_DONE;
	}

	return parsing_state_;
}

HttpRequest::ParsingState HttpRequest::get_parsing_state() const
{
	return parsing_state_;
}

HttpRequest::~HttpRequest(void)
{
}
#include "HttpResponse.hpp"

#include <sstream>
#include <string>

HttpResponse::HttpResponse(int status_code)
	: status_code_(status_code),
	  reason_phrase_(get_reason_phrase(status_code)),
	  http_version_("HTTP/1.1")
{
}

std::string HttpResponse::get_reason_phrase(int status_code)
{
	switch (status_code)
	{
		case 200:
			return "OK";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 500:
			return "Internal Server Error";
		default:
			return "Unknown";
	}
}

void HttpResponse::set_body(const std::string& body)
{
	body_ = body;
}

void HttpResponse::set_header(const std::string& key, const std::string& value)
{
	if (key == "Content-Length" || key == "contennt-length")
	{
		return;
	}
	headers_[key] = value;
}

std::string HttpResponse::serialize() const
{
	std::stringstream ss;

	ss << http_version_ << " " << status_code_ << " " << reason_phrase_
	   << "\r\n";
	std::map<std::string, std::string>::const_iterator it = headers_.begin();
	for (; it != headers_.end(); it++)
	{
		ss << it->first << ": " << it->second << "\r\n";
	}
	ss << "Content-Length: " << body_.size() << "\r\n";
	ss << "\r\n";
	ss << body_;
	return ss.str();
}

HttpResponse::~HttpResponse()
{
}
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
}#include <exception>
#include <iostream>

#include "Core.hpp"

int main()
{
	try
	{
		Core core(8080);
		core.core_loop();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;
}
