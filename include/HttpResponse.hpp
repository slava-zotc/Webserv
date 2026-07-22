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
