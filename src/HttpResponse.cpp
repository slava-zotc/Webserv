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
