#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include <sstream>

class HttpRequest
{
public:
    enum Methods
    {
        GET,
        POST,
        DELETE,
        UNKNOWN
    };
    enum ParsingState
    {
        PARSING_START,
        PARSING_HEADERS,
        PARSING_BODY,
        PARSING_DONE,
        PARSING_ERROR
    };

    HttpRequest();
    ParsingState parse(const std::string &data);
    const std::string &get_path() const;
    const std::string &get_version() const;
    const std::string &get_body() const;
    const std::map<std::string, std::string> &get_headers() const;
    Methods get_method() const;
    ParsingState get_parsing_state() const;
    ~HttpRequest();

private:
    std::string internal_buffer_;
    Methods method_;
    ParsingState parsing_state_;
    std::string path_;
    std::string version_;
    std::string body_;
    std::map<std::string, std::string> headers_;
    size_t content_length_;

    void process_start_line(const std::string &line);
    Methods convert_method_str(const std::string &method_str);

    void process_header_line(const std::string &line);
    HttpRequest(const HttpRequest &src);
    HttpRequest &operator=(const HttpRequest &rhs);
};

#endif // HTTP_REQUEST_HPP