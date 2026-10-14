#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <map>
#include <string>

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
		PARSING_CHUNKED,
		PARSING_DONE,
		PARSING_ERROR
	};

	HttpRequest();
	ParsingState parse(const std::string& data);
	void set_max_body_size(size_t max_body_size);
	const std::string& get_path() const;
	const std::string& get_version() const;
	const std::string& get_body() const;
	const std::string& get_query() const;
	std::string get_str_method() const;
	const std::map<std::string, std::string>& get_headers() const;
	Methods get_method() const;
	ParsingState get_parsing_state() const;
	int get_error_status() const;
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
	size_t max_body_size_;
	int error_status_;
	std::string query_string_;

	// Разбор тела в chunked-кодировке (RFC 9112 §7.1)
	enum ChunkState
	{
		CHUNK_SIZE,		 // ждём строку "<hex-размер>[;расширения]\r\n"
		CHUNK_DATA,		 // читаем chunk_remaining_ байт данных
		CHUNK_DATA_END,	 // после данных обязателен "\r\n"
		CHUNK_TRAILER	 // после последнего (нулевого) чанка: трейлеры до пустой строки
	};
	ChunkState chunk_state_;
	size_t chunk_remaining_;
	void parse_chunked();
	bool start_transfer_encoding();

	void process_start_line(const std::string& line);
	Methods convert_method_str(const std::string& method_str);

	void process_header_line(const std::string& line);
	HttpRequest(const HttpRequest& src);
	HttpRequest& operator=(const HttpRequest& rhs);
};

#endif	// HTTP_REQUEST_HPP
