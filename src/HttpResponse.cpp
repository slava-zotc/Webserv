#include "HttpResponse.hpp"

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
		case 400:
			return "Bad Request";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 409:
			return "Conflict";
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
