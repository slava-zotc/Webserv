#include "Router.hpp"

#include <cstdio>
#include <dirent.h>
#include <sstream>

std::string Router::resolve_path(const std::string& path, const Route& route)
{
	std::string tail = path.substr(route.get_prefix().length());

	if (!tail.empty() && tail[0] == '/') tail.erase(0, 1);

	std::string resolve_path = route.get_root() + "/" + tail;
	return resolve_path;
}

std::string Router::resolve_upload_path(const std::string& path,
										 const Route& route)
{
	std::string tail = path.substr(route.get_prefix().length());

	if (!tail.empty() && tail[0] == '/') tail.erase(0, 1);

	std::string upload_path = route.get_upload_dir() + "/" + tail;
	return upload_path;
}

const HttpResponse Router::handle_request(const HttpRequest& request,
										  const Server& server)
{
	const Route* route = matching(request.get_path(), server.get_route());
	HttpResponse response(404);

	if (route == NULL)
	{
		return apply_error_page(response, server);
	}
	else if (request.get_method() == HttpRequest::GET)
	{
		if (!(route->get_allowed_methods() & Route::GET))
			response = HttpResponse(405);
		else
		{
			std::string path = resolve_path(request.get_path(), *route);
			response = handle_get_method(path, request.get_path(), *route);
		}
	}
	else if (request.get_method() == HttpRequest::POST)
	{
		if (!(route->get_allowed_methods() & Route::POST))
			response = HttpResponse(405);
		else if (!route->get_upload_enabled())
			response = HttpResponse(403);
		else
		{
			std::string path = resolve_upload_path(request.get_path(), *route);
			response = handle_post_method(path, request.get_body());
		}
	}
	else if (request.get_method() == HttpRequest::DELETE)
	{
		if (!(route->get_allowed_methods() & Route::DELETE))
			response = HttpResponse(405);
		else
		{
			std::string path = resolve_path(request.get_path(), *route);
			response = handle_delete_method(path);
		}
	}
	else
	{
		response = HttpResponse(405);
	}

	return apply_error_page(response, server);
}

std::string Router::default_error_body(const HttpResponse& response)
{
	std::stringstream html;
	html << "<html><head><title>" << response.get_status_code() << " "
		 << response.get_reason_phrase()
		 << "</title></head><body><h1>" << response.get_status_code() << " "
		 << response.get_reason_phrase() << "</h1></body></html>";
	return html.str();
}

HttpResponse Router::apply_error_page(HttpResponse response,
									   const Server& server)
{
	if (response.get_status_code() < 400 || !response.get_body().empty())
		return response;

	const std::map<int, std::string>& error_pages = server.get_error_pages();
	std::map<int, std::string>::const_iterator it =
		error_pages.find(response.get_status_code());

	if (it != error_pages.end())
	{
		struct stat file_stat;
		if (stat(it->second.c_str(), &file_stat) == 0
			&& S_ISREG(file_stat.st_mode))
		{
			int fd = open(it->second.c_str(), O_RDONLY);
			if (fd != -1)
			{
				std::string::size_type total_size =
					static_cast<std::string::size_type>(file_stat.st_size);
				std::string body(total_size, '\0');
				size_t total_read = 0;
				bool ok = true;

				while (total_read < total_size)
				{
					int tmp_read_byte =
						read(fd, &body[0] + total_read, total_size - total_read);
					if (tmp_read_byte <= 0)
					{
						ok = false;
						break;
					}
					total_read += tmp_read_byte;
				}
				close(fd);
				if (ok)
				{
					response.set_body(body);
					response.set_header("Content-Type", get_content_type(it->second));
					return response;
				}
			}
		}
	}

	response.set_body(default_error_body(response));
	response.set_header("Content-Type", "text/html");
	return response;
}

std::string Router::get_content_type(const std::string& path)
{
	size_t dot_pos = path.find_last_of('.');

	if (dot_pos == std::string::npos) return "application/octet-stream";

	std::string ext = path.substr(dot_pos);

	if (ext == ".html" || ext == ".htm") return "text/html";
	if (ext == ".css") return "text/css";
	if (ext == ".js") return "application/javascript";
	if (ext == ".json") return "application/json";
	if (ext == ".txt") return "text/plain";
	if (ext == ".png") return "image/png";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".gif") return "image/gif";
	if (ext == ".svg") return "image/svg+xml";
	if (ext == ".ico") return "image/x-icon";
	if (ext == ".pdf") return "application/pdf";

	return "application/octet-stream";
}

std::string Router::join_path(const std::string& base, const std::string& name)
{
	if (!base.empty() && base[base.size() - 1] == '/') return base + name;
	return base + "/" + name;
}

HttpResponse Router::read_file_response(const std::string& file_path)
{
	struct stat file_stat;

	if (stat(file_path.c_str(), &file_stat) == -1) return HttpResponse(404);

	int fd = open(file_path.c_str(), O_RDONLY);
	if (fd == -1) return HttpResponse(500);

	size_t total_read = 0;
	std::string::size_type total_size =
		static_cast<std::string::size_type>(file_stat.st_size);
	std::string body(total_size, '\0');

	while (total_read < total_size)
	{
		int tmp_read_byte = 0;
		tmp_read_byte =
			read(fd, &body[0] + total_read, total_size - total_read);
		if (tmp_read_byte == -1 || tmp_read_byte == 0)
		{
			close(fd);
			return HttpResponse(500);
		}
		total_read += tmp_read_byte;
	}
	close(fd);

	HttpResponse result(200);
	result.set_body(body);
	result.set_header("Content-Type", get_content_type(file_path));
	return result;
}

HttpResponse Router::generate_autoindex(const std::string& dir_path,
										 const std::string& url_path)
{
	DIR* dir = opendir(dir_path.c_str());
	if (dir == NULL) return HttpResponse(500);

	// Ссылки в листинге должны указывать на URL, а не на путь в
	// файловой системе — url_path гарантированно заканчивается на '/',
	// потому что мы вызываем это только когда запрос указывает на директорию.
	std::string url = url_path;
	if (url.empty() || url[url.size() - 1] != '/') url += "/";

	std::stringstream html;
	html << "<html><head><title>Index of " << url << "</title></head><body>";
	html << "<h1>Index of " << url << "</h1><ul>";

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == ".") continue;  // "." — сама директория, смысла нет
		html << "<li><a href=\"" << url << name << "\">" << name
			 << "</a></li>";
	}
	closedir(dir);

	html << "</ul></body></html>";

	HttpResponse result(200);
	result.set_body(html.str());
	result.set_header("Content-Type", "text/html");
	return result;
}

HttpResponse Router::handle_get_method(const std::string& resolve_path,
										const std::string& url_path,
										const Route& route)
{
	struct stat file_stat;

	if (stat(resolve_path.c_str(), &file_stat) == -1) return HttpResponse(404);

	if (S_ISDIR(file_stat.st_mode))
	{
		// Шаг 1: пробуем index_file_ внутри директории.
		std::string index_path = join_path(resolve_path, route.get_index_file());
		struct stat index_stat;

		if (stat(index_path.c_str(), &index_stat) == 0
			&& S_ISREG(index_stat.st_mode))
			return read_file_response(index_path);

		// Шаг 2: index_file_ не найден — смотрим на autoindex_.
		if (route.get_autoindex())
			return generate_autoindex(resolve_path, url_path);

		// autoindex выключен, индекса нет — намеренно отказываем.
		return HttpResponse(403);
	}

	return read_file_response(resolve_path);
}

HttpResponse Router::handle_post_method(const std::string& upload_path,
										 const std::string& body)
{
	int fd = open(upload_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd == -1) return HttpResponse(500);

	size_t total_written = 0;
	while (total_written < body.size())
	{
		int tmp_write_byte =
			write(fd, body.c_str() + total_written, body.size() - total_written);
		if (tmp_write_byte == -1)
		{
			close(fd);
			return HttpResponse(500);
		}
		total_written += tmp_write_byte;
	}
	close(fd);

	return HttpResponse(201);
}

HttpResponse Router::handle_delete_method(const std::string& resolve_path)
{
	struct stat file_stat;

	if (stat(resolve_path.c_str(), &file_stat) == -1) return HttpResponse(404);

	if (S_ISDIR(file_stat.st_mode)) return HttpResponse(409);

	if (std::remove(resolve_path.c_str()) == -1) return HttpResponse(500);

	return HttpResponse(204);
}

const Route* Router::matching(const std::string& path,
							  const std::vector<Route>& route)
{
	const Route* best_route = NULL;
	size_t lenght_prefix_route = 0;

	for (size_t i = 0; i < route.size(); i++)
	{
		if (path.compare(0, route[i].get_prefix().length(),
						 route[i].get_prefix())
				== 0
			&& (path.length() == route[i].get_prefix().length()
				|| route[i].get_prefix() == "/"
				|| path[route[i].get_prefix().length()] == '/'))
		{
			if (best_route == NULL)
			{
				best_route = &route[i];
				lenght_prefix_route = route[i].get_prefix().length();
			}
			else
			{
				if (route[i].get_prefix().length() > lenght_prefix_route)
				{
					best_route = &route[i];
					lenght_prefix_route = route[i].get_prefix().length();
				}
			}
		}
	}

	return best_route;
}
