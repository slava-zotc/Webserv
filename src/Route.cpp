#include "Route.hpp"

Route::Route(const CFG_Route& config)
	: prefix_(config.prefix_),
	  root_(config.root_),
	  autoindex_(config.autoindex_),
	  index_file_(config.index_file_),
	  has_redirect_(config.has_redirect_),
	  redirect_target_(config.redirect_target_),
	  redirect_code_(config.redirect_code_),
	  allowed_methods_(config.allowed_methods_),
	  upload_enabled_(config.upload_enabled_),
	  upload_dir_(config.upload_dir_),
	  extensions_(config.extensions_),
	  path_interpreter_(config.path_interpreter_)
{
}

Route::Route(void)
{
}

Route::Route(const Route& src)
{
	*this = src;
}

Route& Route::operator=(const Route& rhs)
{
	if (this != &rhs)
	{
		prefix_ = rhs.prefix_;
		root_ = rhs.root_;
		autoindex_ = rhs.autoindex_;
		index_file_ = rhs.index_file_;
		has_redirect_ = rhs.has_redirect_;
		redirect_target_ = rhs.redirect_target_;
		redirect_code_ = rhs.redirect_code_;
		allowed_methods_ = rhs.allowed_methods_;
		upload_enabled_ = rhs.upload_enabled_;
		upload_dir_ = rhs.upload_dir_;
		extensions_ = rhs.extensions_;
		path_interpreter_ = rhs.path_interpreter_;
	}
	return (*this);
}

Route::~Route(void)
{
}

const std::string& Route::get_prefix() const
{
	return prefix_;
}

const std::string& Route::get_root() const
{
	return root_;
}

const std::string& Route::get_index_file() const
{
	return index_file_;
}

const std::string& Route::get_redirect_target() const
{
	return redirect_target_;
}

int Route::get_redirect_code() const
{
	return redirect_code_;
}

int Route::get_allowed_methods() const
{
	return allowed_methods_;
}

bool Route::get_autoindex() const
{
	return autoindex_;
}

bool Route::get_has_redirect() const
{
	return has_redirect_;
}

bool Route::get_upload_enabled() const
{
	return upload_enabled_;
}

const std::string& Route::get_upload_dir() const
{
	return upload_dir_;
}

const std::string& Route::get_extensions() const
{
	return extensions_;
}

const std::string& Route::get_path_interpreter() const
{
	return path_interpreter_;
}

static std::string methods_to_string(int mask)
{
	std::string result;
	if (mask & Route::GET) result += "GET ";
	if (mask & Route::POST) result += "POST ";
	if (mask & Route::DELETE) result += "DELETE ";
	if (result.empty()) return "(none)";
	result.erase(result.size() - 1);  // убрать последний пробел
	return result;
}

static std::string or_none(const std::string& value)
{
	if (value.empty()) return "(none)";
	return value;
}

static const char* on_off(bool value)
{
	return value ? "on" : "off";
}

std::ostream& operator<<(std::ostream& os, const Route& v)
{
	os << "location " << v.get_prefix() << "\n";
	os << "  root:      " << v.get_root() << "\n";
	os << "  index:     " << or_none(v.get_index_file()) << "\n";
	os << "  autoindex: " << on_off(v.get_autoindex()) << "\n";
	os << "  methods:   " << methods_to_string(v.get_allowed_methods()) << "\n";

	os << "  redirect:  ";
	if (v.get_has_redirect())
		os << v.get_redirect_code() << " -> " << v.get_redirect_target();
	else
		os << "(none)";
	os << "\n";

	os << "  upload:    " << on_off(v.get_upload_enabled());
	if (v.get_upload_enabled()) os << ", dir: " << or_none(v.get_upload_dir());
	os << "\n";

	os << "  cgi:       ";
	if (v.get_extensions().empty())
		os << "(none)";
	else
		os << v.get_extensions() << " -> " << or_none(v.get_path_interpreter());
	os << "\n";

	return os;
}
