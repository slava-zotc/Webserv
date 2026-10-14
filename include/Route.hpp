#ifndef ROUTE_HPP
#define ROUTE_HPP
#include <string>
#include <iostream>

struct CFG_Route
{
	std::string prefix_;
	std::string root_;
	bool autoindex_;
	std::string index_file_;
	bool has_redirect_;
	std::string redirect_target_;
	int redirect_code_;
	int allowed_methods_;
	bool upload_enabled_;
	std::string upload_dir_;
	std::string extensions_;
	std::string path_interpreter_;
	bool has_max_body_size_;		///< client_max_body_size задан в location
	unsigned long max_body_size_;
	int cgi_methods_;				///< 0 = CGI на все разрешённые методы
	bool cgi_require_file_;			///< 404, если файла скрипта нет (по умолчанию)
	int cgi_timeout_;				///< секунд без обмена данными до 504

};


class Route
{
public:
	Route(const CFG_Route &config);
	~Route();
	enum Method
	{
		GET = 1 << 0,
		POST = 1 << 1,
		DELETE = 1 << 2,
		HEAD = 1 << 3	///< как GET, но без тела (RFC 9110 §9.3.2)

	};

	const std::string& get_prefix() const;
	const std::string& get_root() const;
	const std::string& get_index_file() const;
	const std::string& get_redirect_target() const;
	int get_redirect_code() const;
	bool get_autoindex() const;
	bool get_has_redirect() const;
	int get_allowed_methods() const;
	bool get_upload_enabled() const;
	const std::string& get_upload_dir() const;
	const std::string& get_extensions() const;
	const std::string& get_path_interpreter() const;
	bool has_max_body_size() const;
	unsigned long get_max_body_size() const;
	int get_cgi_methods() const;
	bool get_cgi_require_file() const;
	int get_cgi_timeout() const;
	Route(const Route& src);
	Route& operator=(const Route& rhs);

private:
	std::string prefix_;
	std::string root_;
	bool autoindex_;
	std::string index_file_;
	bool has_redirect_;
	std::string redirect_target_;
	int redirect_code_;
	int allowed_methods_;
	bool upload_enabled_;
	std::string upload_dir_;
	std::string extensions_;
	std::string path_interpreter_;
	bool has_max_body_size_;
	unsigned long max_body_size_;
	int cgi_methods_;
	bool cgi_require_file_;
	int cgi_timeout_;
	Route();
};

std::ostream& operator<<(std::ostream& os, const Route& v);

#endif
