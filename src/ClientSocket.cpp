#include "ClientSocket.hpp"

#include <sys/socket.h>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
/**
 * @brief Конструктор ClientSocket
 * @param fd Файловый дескриптор клиентского сокета
 *
 * Инициализирует клиентское соединение в начальном состоянии (READ_HEADERS).
 */
ClientSocket::ClientSocket(int fd, const Server* server)
	: socket_fd(fd), partial_write(0), state_client(READING), server_(server)
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
		HttpResponse response = Router::handle_request(request, *server_);
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
