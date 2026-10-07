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
	  error_status_(400)
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
			// TODO Написать обработчик для chanked transfer-encoding
			// Пока что отказываем в обработке запроса с 501 Not Implemented
			error_status_ = 501;
			parsing_state_ = PARSING_ERROR;
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
	size_t pos = path_str.find_first_of('?');
	if (pos != std::string::npos)
	{
		path_ = path_str.substr(0, pos);
		query_string_ = path_str.substr(pos + 1);
	}
	else
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