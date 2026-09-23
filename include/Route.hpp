#ifndef ROUTE_HPP
#define ROUTE_HPP
#include <string>


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
		DELETE = 1 << 2

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
	Route();
};
#endif
