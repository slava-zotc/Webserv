#include "Router.hpp"

#include <cstdio>

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

	if (route == NULL)
	{
		return HttpResponse(404);
	}

	if (request.get_method() == HttpRequest::GET)
	{
		if (!(route->get_allowed_methods() & Route::GET))
			return HttpResponse(405);

		std::string path = resolve_path(request.get_path(), *route);
		return handle_get_method(path);
	}

	if (request.get_method() == HttpRequest::POST)
	{
		if (!(route->get_allowed_methods() & Route::POST))
			return HttpResponse(405);
		if (!route->get_upload_enabled())
			return HttpResponse(403);

		std::string path = resolve_upload_path(request.get_path(), *route);
		return handle_post_method(path, request.get_body());
	}

	if (request.get_method() == HttpRequest::DELETE)
	{
		if (!(route->get_allowed_methods() & Route::DELETE))
			return HttpResponse(405);

		std::string path = resolve_path(request.get_path(), *route);
		return handle_delete_method(path);
	}

	return HttpResponse(405);
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

HttpResponse Router::handle_get_method(const std::string& resolve_path, const Route &route)
{
	HttpResponse result(200);

	struct stat file_stat;

	if (stat(resolve_path.c_str(), &file_stat) == -1) return HttpResponse(404);

	if (S_ISDIR(file_stat.st_mode))
	{
		if (!route.get_autoindex())
			return HttpResponse(404);
		resolve_path + route.get_index_file();  // вернутся кога буду реализовывать autoindex
	}

	int fd = open(resolve_path.c_str(), O_RDONLY);
	if (fd == -1)
	{
		// подумать как должна реагировать
		return HttpResponse(500);
	}

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

	result.set_body(body);

	result.set_header("Content-Type", get_content_type(resolve_path));

	close(fd);
	return result;
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
