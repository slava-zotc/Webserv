#include "ClientSocket.hpp"

#include <sys/socket.h>
#include <sys/wait.h>

#include <exception>
#include <iostream>

#include "CgiProcess.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"
#include "Server.hpp"
/**
 * @brief Конструктор ClientSocket
 * @param fd Файловый дескриптор клиентского сокета
 * @param client_ip IPv4-адрес клиента (из accept)
 *
 * Инициализирует клиентское соединение в начальном состоянии (READ_HEADERS).
 */
ClientSocket::ClientSocket(int fd, const std::string& client_ip)
	: socket_fd(fd),
	  partial_write(0),
	  state_client(READING),
	  cgi_process(NULL),
	  last_client_activity_time(std::time(NULL)),
	  client_ip_(client_ip),
	  body_limit_set_(false),
	  linger_start_(0),
	  cgi_timeout_(0)
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
	if (state_client == WAITING_CGI) mask = 0;	// Ждём cgi
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
	return state_client == READING || state_client == LINGERING;
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
 * @brief Возвращает файловый дескриптор для чтения из CGI
 * @return Файловый дескриптор или -1, если CGI не запущен
 */
int ClientSocket::get_cgi_fd_read() const
{
	if (cgi_process)
	{
		return cgi_process->get_fd_read();
	}
	return -1;
}

/**
 * @brief Файловый дескриптор stdin скрипта, в который ещё пишется тело
 * @return Дескриптор или -1, если CGI нет или тело уже передано
 */
int ClientSocket::get_cgi_fd_write() const
{
	if (cgi_process)
	{
		return cgi_process->get_fd_write();
	}
	return -1;
}

/**
 * @brief poll() сообщил, что в stdin скрипта можно писать
 */
void ClientSocket::handle_cgi_write()
{
	if (cgi_process)
	{
		cgi_process->write_cgi_input();
	}
}

bool ClientSocket::is_waiting_cgi_exit() const
{
	return cgi_process != NULL && cgi_process->is_output_done();
}

bool ClientSocket::has_cgi() const
{
	return cgi_process != NULL;
}

void ClientSocket::check_client_timeout(std::time_t now, const Server& server)
{
	if (state_client == LINGERING && (now - linger_start_) >= LINGER_TIMEOUT)
	{
		state_client = READY_DELETE;  // клиент так и не закрыл - закрываем сами
		return;
	}
	if (state_client == READING && (now - last_client_activity_time) > CLIENT_TIMEOUT)
	{
		std::cerr << "[ClientSocket] fd=" << socket_fd.get_fd()
				  << ": client timeout exceeded, closing connection" << std::endl;
		response_buffer = Router::apply_error_page(HttpResponse(408), server).serialize();
		state_client = READY_SEND;  // Переводим в режим отправки ответа с ошибкой
	}
}

/**
 * @brief Обрабатывает получение данных от клиента
 * @param server Сервер, к которому подключён этот клиент (сообщает Core)
 *
 * Читает данные из сокета, передаёт разобранный запрос в Router
 * и сериализует полученный HttpResponse в буфер на отправку.
 */
void ClientSocket::handle_read(const Server& server)
{
	char tmp_buffer[65536];	// 64 КиБ: большие тела (100 МБ) читаются быстрее

	int byte_recv = recv(socket_fd.get_fd(), tmp_buffer, sizeof(tmp_buffer), 0);
	// Клиент рассоединился или ошибка
	if (byte_recv <= 0)
	{
		state_client = READY_DELETE;
		return;
	}
	// Ответ на ошибку уже ушёл: остаток запроса просто выбрасываем.
	if (state_client == LINGERING) return;
	std::string recv_string(tmp_buffer, byte_recv);
	last_client_activity_time = std::time(NULL);	// Обновляем время последней активности
	try
	{
		request.parse(recv_string);
		// parse() останавливается сразу после request-line: путь известен,
		// тело ещё не читалось. Выбираем лимит: client_max_body_size из
		// location, если он там задан, иначе из server. Затем продолжаем.
		if (!body_limit_set_
			&& request.get_parsing_state() != HttpRequest::PARSING_START)
		{
			body_limit_set_ = true;
			const Route* limit_route =
				Router::matching(request.get_path(), server.get_route());
			if (limit_route && limit_route->has_max_body_size())
				request.set_max_body_size(limit_route->get_max_body_size());
			else
				request.set_max_body_size(server.get_max_body_size());
			if (request.get_parsing_state() == HttpRequest::PARSING_HEADERS)
				request.parse("");
		}
		if (request.get_parsing_state() == HttpRequest::PARSING_DONE)
		{
			// TODO: подумать о том стоит ли хранить роут в клиент сокет и
			// вычислять его в конструкторе(не в конструкторе но где то где это
			// нативно) и может быть выставлять флаг is_cgi
			const Route* route =
				Router::matching(request.get_path(), server.get_route());
			if (route
				&& Router::is_cgi(request.get_path(), request.get_method(),
								  *route))
			{
				start_cgi(*route, server);
			}
			else
			{
				HttpResponse response = Router::handle_request(request, server);
				response.serialize_to(response_buffer);	 // без копии тела
			}
		}
		else if (request.get_parsing_state() == HttpRequest::PARSING_ERROR)
		{
			HttpResponse response = Router::apply_error_page(
				HttpResponse(request.get_error_status()), server);
			response_buffer = response.serialize();
		}
		else
			return;
	}
	// Исключение при разборе запроса или его обработке (например,
	// std::bad_alloc/std::length_error на аномально большом теле) не должно
	// убивать весь процесс — гасим его здесь и закрываем только это
	// соединение, остальные клиенты не затрагиваются.
	catch (const std::exception& e)
	{
		std::cerr << "[ClientSocket] fd=" << socket_fd.get_fd()
				  << ": exception while handling request: " << e.what()
				  << " -- closing this connection with 500" << std::endl;
		HttpResponse response =
			Router::apply_error_page(HttpResponse(500), server);
		response_buffer = response.serialize();
	}
	catch (...)
	{
		std::cerr << "[ClientSocket] fd=" << socket_fd.get_fd()
				  << ": unknown exception while handling request"
				  << " -- closing this connection with 500" << std::endl;
		HttpResponse response =
			Router::apply_error_page(HttpResponse(500), server);
		response_buffer = response.serialize();
	}
	if (state_client != WAITING_CGI)
	{
		state_client = READY_SEND;	// Переводим в режим отправки
	}
}

void ClientSocket::handle_cgi_events(const Server& server)
{
	if (cgi_process == NULL)
	{
		return;	 // No CGI process to handle
	}
	HttpResponse response(200);
	int cgi_result = cgi_process->process_output(response);
	if (cgi_result == CgiProcess::CGI_ERROR)
	{
		HttpResponse error_response =
			Router::apply_error_page(HttpResponse(502), server);
		response_buffer = error_response.serialize();
		delete cgi_process;
		cgi_process = NULL;
		state_client = READY_SEND;
	}
	else if (cgi_result == CgiProcess::CGI_SUCCESS)
	{
		response.serialize_to(response_buffer);	 // без копии тела
		delete cgi_process;
		cgi_process = NULL;
		state_client = READY_SEND;
	}
	else
	{
		state_client = WAITING_CGI;
		return;	 // CGI is still running
	}
}

void ClientSocket::start_cgi(const Route& route, const Server& server)
{
	// /cgi-bin/env.py/a/b -> скрипт /cgi-bin/env.py, PATH_INFO /a/b.
	// Путь уже декодирован и нормализован в HttpRequest (без "..").
	std::string script_name;
	std::string path_info;
	Router::split_cgi_path(request.get_path(), route.get_extensions(),
						   script_name, path_info);
	std::string resolve_path = Router::resolve_path(script_name, route);
	struct stat st;
	bool exists =
		stat(resolve_path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
	if (!exists && route.get_cgi_require_file())
	{
		HttpResponse response =
			Router::apply_error_page(HttpResponse(404), server);
		response_buffer = response.serialize();
	}
	else
	{
		std::vector<std::string> dir_and_filename =
			CgiProcess::get_dir_and_filename(resolve_path);
		const std::vector<std::string> env = CgiProcess::get_env_string(
			request, server, dir_and_filename[1], script_name, path_info,
			client_ip_);
		CgiProcess* cgi = new CgiProcess();
		// CONTENT_LENGTH уже в env, теперь тело можно отдать CGI без копии.
		std::string body;
		request.take_body(body);
		if (!cgi->execute_cgi(route.get_path_interpreter(), dir_and_filename[0],
							  dir_and_filename[1], env, body))
		{
			std::cerr << "webserv: cannot start CGI " << resolve_path
					  << std::endl;
			HttpResponse response =
				Router::apply_error_page(HttpResponse(500), server);
			response_buffer = response.serialize();
			delete cgi;
		}
		else
		{
			cgi_process = cgi;
			cgi_timeout_ = route.get_cgi_timeout();
			state_client = WAITING_CGI;
			return;
		}
	}
}

/**
 * @brief Обрабатывает отправку HTTP-ответа клиенту
 *
 * Отправляет данные блоками и отслеживает прогресс.
 * Нормально все данные передаются с одно призыва.
 */
void ClientSocket::handle_write()
{
	// RFC 9110 §9.3.2: ответ на HEAD - те же статус и заголовки, что у GET
	// (включая Content-Length), но без тела. Отрезаем тело здесь, в одном
	// месте, перед первой отправкой: ответ мог собрать Router, CGI или
	// обработчик ошибок, и всем им не нужно знать про HEAD.
	if (partial_write == 0 && request.get_method() == HttpRequest::HEAD)
	{
		std::string::size_type end = response_buffer.find("\r\n\r\n");
		if (end != std::string::npos) response_buffer.erase(end + 4);
	}
	// Отправляем неотправленную часть ответа
	int byte_send =
		send(socket_fd.get_fd(), response_buffer.c_str() + partial_write,
			 response_buffer.size() - partial_write, 0);
	// -1 — ошибка, 0 — ничего не отправлено при непустом буфере: в обоих
	// случаях соединение бесполезно, клиента удаляем (так требует чек-лист:
	// проверять и -1, и 0).
	if (byte_send <= 0)
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
		response_buffer.clear();
		// Запрос отклонён посреди передачи (413, 400...): в сокете может
		// остаться непрочитанное тело. close() с непрочитанными данными
		// ядро превращает в RST, и RST может стереть наш ответ у клиента
		// раньше, чем тот его прочитает. Поэтому «lingering close»: ещё
		// до LINGER_TIMEOUT секунд читаем и выбрасываем остаток, пока
		// клиент сам не закроет соединение (recv вернёт 0).
		if (request.get_parsing_state() == HttpRequest::PARSING_ERROR)
		{
			state_client = LINGERING;
			linger_start_ = std::time(NULL);
		}
		else
			state_client = READY_DELETE;
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

void ClientSocket::check_cgi_timeout(std::time_t now, const Server& server)
{
	if (cgi_process
		&& cgi_process->is_timeout(now, cgi_timeout_))	 // cgi_timeout с без обмена
	{
		std::cerr << "[Client Socket] CGI process timeout for fd="
				  << socket_fd.get_fd() << std::endl;
		HttpResponse response =
			Router::apply_error_page(HttpResponse(504), server);
		response_buffer = response.serialize();
		delete cgi_process;
		cgi_process = NULL;
		state_client = READY_SEND;
	}
}

/**
 * @brief Деструктор ClientSocket
 */
ClientSocket::~ClientSocket()
{
	delete cgi_process;
	cgi_process = NULL;
}
