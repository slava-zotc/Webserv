#include "HttpResponse.hpp"

#include <cctype>
#include <sstream>
#include <string>

HttpResponse::HttpResponse(int status_code)
	: status_code_(status_code),
	  reason_phrase_(get_reason_phrase(status_code)),
	  http_version_("HTTP/1.1")
{
}

HttpResponse::HttpResponse(const HttpResponse& src)
	: status_code_(src.status_code_),
	  reason_phrase_(src.reason_phrase_),
	  headers_(src.headers_),
	  body_(src.body_),
	  http_version_(src.http_version_)
{
}

HttpResponse& HttpResponse::operator=(const HttpResponse& rhs)
{
	if (this != &rhs)
	{
		status_code_ = rhs.status_code_;
		reason_phrase_ = rhs.reason_phrase_;
		headers_ = rhs.headers_;
		body_ = rhs.body_;
		http_version_ = rhs.http_version_;
	}
	return (*this);
}

std::string HttpResponse::get_reason_phrase(int status_code)
{
	switch (status_code)
	{
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 204:
			return "No Content";
		case 301:
			return "Moved Permanently";
		case 302:
			return "Found";
		case 400:
			return "Bad Request";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 408:
			return "Request Timeout";
		case 409:
			return "Conflict";
		case 413:
			return "Payload Too Large";
		case 414:
			return "URI Too Long";
		case 431:
			return "Request Header Fields Too Large";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implemented";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Timeout";
		case 505:
			return "HTTP Version Not Supported";
		default:
			return "Unknown";
	}
}

bool HttpResponse::has_header(const std::string& key) const
{
	return headers_.count(key) != 0;
}

void HttpResponse::set_status_code(int status_code)
{
	status_code_ = status_code;
	reason_phrase_ = get_reason_phrase(status_code);
}

void HttpResponse::set_body(const std::string& body)
{
	body_ = body;
}

/**
 * @brief Забирает тело без копирования (обмен буферами)
 *
 * Ответ CGI может весить 100 МБ; set_body() сделал бы ещё одну копию.
 */
void HttpResponse::swap_body(std::string& body)
{
	body_.swap(body);
}

int HttpResponse::get_status_code() const
{
	return status_code_;
}

const std::string& HttpResponse::get_body() const
{
	return body_;
}

const std::string& HttpResponse::get_reason_phrase() const
{
	return reason_phrase_;
}

void HttpResponse::set_header(const std::string& key, const std::string& value)
{
	// Content-Length и Connection всегда выставляет serialize(): длину он
	// считает сам по телу, а соединение мы после ответа всегда закрываем.
	// Чужое значение (например, из заголовков CGI) дало бы дубль или ложь.
	std::string lower = key;
	for (std::string::size_type i = 0; i < lower.size(); ++i)
		lower[i] = std::tolower(static_cast<unsigned char>(lower[i]));
	if (lower == "content-length" || lower == "connection")
	{
		return;
	}
	headers_[key] = value;
}

/**
 * @brief Статус-строка и заголовки (с пустой строкой в конце), без тела
 * @param body_size Длина тела для Content-Length
 */
std::string HttpResponse::build_head(std::string::size_type body_size) const
{
	std::stringstream ss;

	ss << http_version_ << " " << status_code_ << " " << reason_phrase_
	   << "\r\n";
	std::map<std::string, std::string>::const_iterator it = headers_.begin();
	for (; it != headers_.end(); it++)
	{
		ss << it->first << ": " << it->second << "\r\n";
	}
	ss << "Content-Length: " << body_size << "\r\n";
	// RFC 9112 §9.6: сервер, который закроет соединение после ответа,
	// ДОЛЖЕН (SHOULD) сообщить об этом "Connection: close". Мы обслуживаем
	// один запрос на соединение, поэтому шлём его всегда; так и клиент
	// HTTP/1.1 не ждёт следующего ответа на том же сокете.
	ss << "Connection: close\r\n";
	ss << "\r\n";
	return ss.str();
}

std::string HttpResponse::serialize() const
{
	return build_head(body_.size()) + body_;
}

/**
 * @brief Как serialize(), но без копии тела (тело переносится в out)
 *
 * serialize() создаёт новую строку «заголовки + тело»: для ответа CGI в
 * 100 МБ это ещё 100 МБ памяти на каждого клиента. Здесь заголовки
 * вставляются в начало уже существующего буфера тела (memmove внутри той
 * же памяти), а потом буфер обменом уходит в out. После вызова тело
 * ответа пустое.
 */
void HttpResponse::serialize_to(std::string& out)
{
	body_.insert(0, build_head(body_.size()));
	out.swap(body_);
	std::string().swap(body_);
}

HttpResponse::~HttpResponse()
{
}
