#include "Route.hpp"

Route::Route(const CFG_Route& config)
	: prefix_(config.prefix_),
	  root_(config.root_),
	  autoindex_(config.autoindex_),
	  index_file_(config.index_file_),
	  has_redirect_(config.has_redirect_),
	  redirect_target_(config.redirect_target_),
	  allowed_methods_(config.allowed_methods_),
	  upload_enabled_(config.upload_enabled_),
	  upload_dir_(config.upload_dir_)
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
		allowed_methods_ = rhs.allowed_methods_;
		upload_enabled_ = rhs.upload_enabled_;
		upload_dir_ = rhs.upload_dir_;
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