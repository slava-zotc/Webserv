#include "HttpRequest.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

HttpRequest::HttpRequest(void)
	: internal_buffer_(),
	  method_(UNKNOWN),
	  parsing_state_(PARSING_START),
	  path_(),
	  version_(),
	  body_(),
	  headers_(),
	  content_length_(0)
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

HttpRequest::~HttpRequest(void)
{
}
