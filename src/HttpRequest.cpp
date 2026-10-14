#include "HttpRequest.hpp"
#include "HttpUtils.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

// Дефолтный потолок для Content-Length, пока ClientSocket не вызвал
// set_max_body_size() со значением client_max_body_size из конфига
// конкретного сервера — не даёт клиенту заставить сервер выделить
// std::string произвольного размера через body_.append(...) в parse().
static const size_t DEFAULT_MAX_BODY_SIZE = 10 * 1024 * 1024;  // 10 MiB

HttpRequest::HttpRequest(void)
	: method_(UNKNOWN),
	  parsing_state_(PARSING_START),
	  content_length_(0),
	  max_body_size_(DEFAULT_MAX_BODY_SIZE),
	  error_status_(400),
	  chunk_state_(CHUNK_SIZE),
	  chunk_remaining_(0)
{
}

void HttpRequest::set_max_body_size(size_t max_body_size)
{
	max_body_size_ = max_body_size;
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
	// Если встретили пустую строку, значит, заголовки закончились.
	if (line.empty())
	{
		if (version_ == "HTTP/1.1" && headers_.count("host") == 0)
		{
			parsing_state_ = PARSING_ERROR;
			return;
		}

		if (headers_.count("transfer-encoding") > 0)
		{
			start_transfer_encoding();
			return;
		}
		// Проверяем, есть ли Content-Length и не превышает ли он лимит
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
			if (content_length_ > max_body_size_)
			{
				std::cerr << "[HttpRequest] Content-Length " << content_length_
						  << " exceeds limit " << max_body_size_
						  << " -- rejecting request with 413" << std::endl;
				error_status_ = 413;
				parsing_state_ = PARSING_ERROR;
				return;
			}
			if (content_length_ > 0)
				parsing_state_ = PARSING_BODY;
			else
				parsing_state_ = PARSING_DONE;
		}
		else
		{
			parsing_state_ = PARSING_DONE;
		}
		return;
	}
	if (!has_forbidden_chars(line))
	{
		if (parse_header_line(line, key, value))
		{
			if ((key == "content-length" || key == "host") && headers_.count(key) > 0)
			{
				parsing_state_ = PARSING_ERROR;
				return;
			}
			
			headers_[key] = value;
		}
		else
		{
			parsing_state_ = PARSING_ERROR;
			return;
		}
	}
	else
	{
		parsing_state_ = PARSING_ERROR;
		return;
	}
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

	// Версия: "HTTP/<цифра>.<цифра>" (RFC 9112 §2.3). Не тот формат — 400,
	// другая старшая версия (HTTP/2.0, HTTP/3.0) — 505 (RFC 9110 §15.6.6).
	if (version_str.size() != 8 || version_str.compare(0, 5, "HTTP/") != 0
		|| !std::isdigit(static_cast<unsigned char>(version_str[5]))
		|| version_str[6] != '.'
		|| !std::isdigit(static_cast<unsigned char>(version_str[7])))
	{
		parsing_state_ = PARSING_ERROR;	 // 400
		return;
	}
	if (version_str[5] != '1')
	{
		error_status_ = 505;
		parsing_state_ = PARSING_ERROR;
		return;
	}

	std::string raw_path;
	size_t pos = path_str.find_first_of('?');
	if (pos != std::string::npos)
	{
		raw_path = path_str.substr(0, pos);
		query_string_ = path_str.substr(pos + 1);
	}
	else
		raw_path = path_str;

	// Защита от выхода за root через "..": путь декодируется (%XX) и
	// нормализуется здесь, ДО поиска маршрута, поэтому GET/POST/DELETE и
	// CGI получают уже безопасный путь. Некорректный путь — 400.
	if (!normalize_path(raw_path, path_))
	{
		parsing_state_ = PARSING_ERROR;	 // 400
		return;
	}
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
	if (parsing_state_ == PARSING_CHUNKED)
	{
		parse_chunked();
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

const std::string& HttpRequest::get_path() const
{
	return path_;
}
const std::string& HttpRequest::get_version() const
{
	return version_;
}
const std::string& HttpRequest::get_body() const
{
	return body_;
}

HttpRequest::Methods HttpRequest::get_method() const
{
	return method_;
}

int HttpRequest::get_error_status() const
{
	return error_status_;
}

const std::string& HttpRequest::get_query() const
{
	return query_string_;
}

const std::map<std::string, std::string>& HttpRequest::get_headers() const
{
	return headers_;
}

std::string HttpRequest::get_str_method() const
{
	switch (method_)
	{
		case GET:
			return std::string("GET");
		case POST:
			return std::string("POST");
		case DELETE:
			return std::string("DELETE");
		default:
			return std::string("UNKNOWN");
	}
}

// Максимальная длина строки с размером чанка или трейлера. Защита от клиента,
// который шлёт бесконечную строку без \r\n: буфер не должен расти без предела.
static const size_t MAX_CHUNK_LINE = 4096;

/**
 * @brief Проверяет Transfer-Encoding и переводит разбор в режим chunked.
 * @return false — запрос отклонён (parsing_state_ = PARSING_ERROR).
 *
 * - TE вместе с Content-Length — 400: RFC 9112 §6.3 п. 3 говорит, что такое
 *   сообщение «ought to be handled as an error» (типичный признак request
 *   smuggling).
 * - Поддерживаем только "chunked". Любое другое кодирование (gzip, ...) —
 *   501 (RFC 9112 §6.1: server SHOULD respond 501 to an unknown coding).
 */
bool HttpRequest::start_transfer_encoding()
{
	if (headers_.count("content-length") > 0)
	{
		error_status_ = 400;
		parsing_state_ = PARSING_ERROR;
		return false;
	}
	std::string te = headers_["transfer-encoding"];
	std::transform(te.begin(), te.end(), te.begin(), ::tolower);
	if (te != "chunked")
	{
		error_status_ = 501;
		parsing_state_ = PARSING_ERROR;
		return false;
	}
	chunk_state_ = CHUNK_SIZE;
	chunk_remaining_ = 0;
	parsing_state_ = PARSING_CHUNKED;
	return true;
}

/**
 * @brief Раскодирует chunked-тело из internal_buffer_ в body_ (RFC 9112 §7.1).
 *
 * Формат:
 *     chunk         = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
 *     last-chunk    = 1*("0") [ chunk-ext ] CRLF
 *     chunked-body  = *chunk last-chunk trailer-section CRLF
 *
 * Данные приходят кусками произвольного размера (хоть по одному байту),
 * поэтому разбор — автомат: на каждом вызове продвигаемся, пока хватает
 * данных, и останавливаемся, когда их нет, запомнив состояние.
 *
 * Сабжект: «for chunked requests, your server needs to un-chunk them, the CGI
 * will expect EOF as the end of the body». После разбора в body_ лежит уже
 * обычное тело, а CONTENT_LENGTH для CGI считается по его размеру
 * (RFC 3875 §4.2: «the server MUST remove any such codings ... and
 * recalculate the CONTENT_LENGTH»).
 */
void HttpRequest::parse_chunked()
{
	while (parsing_state_ == PARSING_CHUNKED)
	{
		if (chunk_state_ == CHUNK_SIZE || chunk_state_ == CHUNK_TRAILER)
		{
			size_t pos = internal_buffer_.find("\r\n");
			if (pos == std::string::npos)
			{
				if (internal_buffer_.size() > MAX_CHUNK_LINE)
				{
					parsing_state_ = PARSING_ERROR;	 // 400
				}
				return;	 // ждём остаток строки
			}
			std::string line = internal_buffer_.substr(0, pos);
			internal_buffer_.erase(0, pos + 2);

			if (chunk_state_ == CHUNK_TRAILER)
			{
				// Трейлеры нам не нужны: RFC 9112 §7.1.2 разрешает их
				// отбросить. Пустая строка — конец сообщения.
				if (line.empty())
				{
					content_length_ = body_.size();
					// Тело уже раскодировано: убираем TE, чтобы он не ушёл
					// в CGI как HTTP_TRANSFER_ENCODING.
					headers_.erase("transfer-encoding");
					parsing_state_ = PARSING_DONE;
				}
				continue;
			}

			// chunk-size [ ";" chunk-ext ] — расширения игнорируем
			std::string size_str = line.substr(0, line.find(';'));
			std::string::size_type last = size_str.find_last_not_of(" \t");
			size_str.erase(last == std::string::npos ? 0 : last + 1);
			// chunk-size = 1*HEXDIG (RFC 9112 §7.1). Сначала проверяем
			// грамматику: сам по себе >> std::hex принял бы и "0x1a", и
			// "+1a", и " 1a". Больше 15 цифр не помещается в size_t без
			// переполнения, а такой чанк всё равно больше любого лимита.
			if (size_str.empty() || size_str.size() > 15
				|| size_str.find_first_not_of("0123456789abcdefABCDEF")
					   != std::string::npos)
			{
				parsing_state_ = PARSING_ERROR;	 // 400
				return;
			}
			size_t size = 0;
			std::istringstream(size_str) >> std::hex >> size;
			if (size == 0)
			{
				chunk_state_ = CHUNK_TRAILER;  // last-chunk
				continue;
			}
			// Лимит client_max_body_size действует на раскодированное тело
			if (size > max_body_size_ || body_.size() + size > max_body_size_)
			{
				error_status_ = 413;
				parsing_state_ = PARSING_ERROR;
				return;
			}
			chunk_remaining_ = size;
			chunk_state_ = CHUNK_DATA;
		}
		else if (chunk_state_ == CHUNK_DATA)
		{
			if (internal_buffer_.empty()) return;  // ждём данные
			size_t take = std::min(chunk_remaining_, internal_buffer_.size());
			body_.append(internal_buffer_, 0, take);
			internal_buffer_.erase(0, take);
			chunk_remaining_ -= take;
			if (chunk_remaining_ == 0) chunk_state_ = CHUNK_DATA_END;
		}
		else  // CHUNK_DATA_END
		{
			if (internal_buffer_.size() < 2) return;  // ждём "\r\n"
			if (internal_buffer_.compare(0, 2, "\r\n") != 0)
			{
				parsing_state_ = PARSING_ERROR;	 // 400: размер не совпал
				return;
			}
			internal_buffer_.erase(0, 2);
			chunk_state_ = CHUNK_SIZE;
		}
	}
}
