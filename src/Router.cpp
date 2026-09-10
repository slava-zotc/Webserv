#include "Router.hpp"

std::string Router::resolve_path(const std::string& path, const Route& route)
{
	std::string tail = path.substr(route.get_prefix().length());

	if (!tail.empty() && tail[0] == '/') tail.erase(0, 1);

	std::string resolve_path = route.get_root() + "/" + tail;
	return resolve_path;
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
		if (route->get_allowed_methods() & Route::GET)
		{
			std::string path = resolve_path(request.get_path(), *route);
			HttpResponse result = handle_get_method(path);
			return result;
		}
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

HttpResponse Router::handle_get_method(const std::string& resolve_path)
{
	HttpResponse result(200);

	struct stat file_stat;

	if (stat(resolve_path.c_str(), &file_stat) == -1) return HttpResponse(404);

	if (S_ISDIR(file_stat.st_mode))
		return HttpResponse(404);  // вернутся кога буду реализовывать autoindex

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
