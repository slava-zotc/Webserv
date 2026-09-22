#include "ClientSocket.hpp"

#include <sys/socket.h>

#include <exception>
#include <iostream>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"
#include "Server.hpp"
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
 * @param server Сервер, к которому подключён этот клиент (сообщает Core)
 *
 * Читает данные из сокета, передаёт разобранный запрос в Router
 * и сериализует полученный HttpResponse в буфер на отправку.
 */
void ClientSocket::handle_read(const Server& server)
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
	try
	{
		// Лимит тела запроса — из конфига конкретного сервера, а не
		// глобальная константа.
		request.set_max_body_size(server.get_max_body_size());
		request.parse(recv_string);
		if (request.get_parsing_state() == HttpRequest::PARSING_DONE)
		{
			HttpResponse response = Router::handle_request(request, server);
			response_buffer = response.serialize();
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
		HttpResponse response = Router::apply_error_page(HttpResponse(500), server);
		response_buffer = response.serialize();
	}
	catch (...)
	{
		std::cerr << "[ClientSocket] fd=" << socket_fd.get_fd()
				  << ": unknown exception while handling request"
				  << " -- closing this connection with 500" << std::endl;
		HttpResponse response = Router::apply_error_page(HttpResponse(500), server);
		response_buffer = response.serialize();
	}
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
