#include "Router.hpp"

#include "Multipart.hpp"

#include <dirent.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <sstream>

/**
 * @brief Ответ 405 с заголовком Allow
 *
 * RFC 9110 §15.5.6: сервер, отвечающий 405, ОБЯЗАН (MUST) прислать Allow
 * со списком методов, которые этот ресурс поддерживает (§10.2.1).
 */
static HttpResponse method_not_allowed(const Route& route)
{
	std::string allow;
	int mask = route.get_allowed_methods();
	if (mask & Route::GET) allow += "GET";
	if (mask & Route::HEAD) allow += allow.empty() ? "HEAD" : ", HEAD";
	if (mask & Route::POST) allow += allow.empty() ? "POST" : ", POST";
	if (mask & Route::DELETE) allow += allow.empty() ? "DELETE" : ", DELETE";

	HttpResponse response(405);
	response.set_header("Allow", allow);  // пустой Allow = "ничего нельзя"
	return response;
}

int Router::method_to_route_bit(HttpRequest::Methods method)
{
	switch (method)
	{
		case HttpRequest::GET:
			return Route::GET;
		case HttpRequest::POST:
			return Route::POST;
		case HttpRequest::DELETE:
			return Route::DELETE;
		case HttpRequest::HEAD:
			return Route::HEAD;
		default:
			return 0;  // UNKNOWN: ни один бит маски не совпадёт
	}
}

/**
 * @brief Делит путь CGI-запроса на скрипт и PATH_INFO (RFC 3875 §4.1.5)
 * @param path        Путь из запроса, например /cgi-bin/env.py/a/b
 * @param ext         Расширение CGI из location, например .py
 * @param script_name [out] /cgi-bin/env.py  (SCRIPT_NAME, §4.1.13)
 * @param path_info   [out] /a/b             (PATH_INFO, §4.1.5)
 * @return true, если в пути есть сегмент, оканчивающийся на ext
 *
 * Скрипт - первый сегмент пути, который заканчивается на ext; всё после
 * него - «дополнительный путь» для скрипта. /cgi-bin/x.pyc не подходит:
 * после ".py" должен идти '/' или конец строки.
 */
bool Router::split_cgi_path(const std::string& path, const std::string& ext,
							std::string& script_name, std::string& path_info)
{
	if (ext.empty()) return false;

	std::string::size_type pos = path.find(ext);
	while (pos != std::string::npos)
	{
		std::string::size_type end = pos + ext.length();
		if (end == path.length() || path[end] == '/')
		{
			script_name = path.substr(0, end);
			path_info = path.substr(end);
			return true;
		}
		pos = path.find(ext, pos + 1);
	}
	return false;
}

bool Router::is_cgi(const std::string& path, HttpRequest::Methods method,
					const Route& route)
{
	// Редирект важнее CGI: такой запрос обрабатывает handle_request.
	if (route.get_has_redirect()) return false;

	// В пути должен быть сегмент с расширением CGI этого location
	// (если CGI в location не настроен, ext пустой и split вернёт false).
	std::string script_name;
	std::string path_info;
	if (!split_cgi_path(path, route.get_extensions(), script_name, path_info))
		return false;

	// Метод должен быть разрешён в этом location.
	if (!(route.get_allowed_methods() & method_to_route_bit(method)))
		return false;

	// cgi_methods: CGI только для перечисленных методов, остальные
	// (например, GET на youpi.bla) обрабатываются как статика.
	if (route.get_cgi_methods() != 0
		&& !(route.get_cgi_methods() & method_to_route_bit(method)))
		return false;

	return true;
}

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

/**
 * @brief POST с включённой загрузкой: multipart-форма или «сырое» тело
 *
 * multipart/form-data (форма в браузере, curl -F) - разбираем на части и
 * сохраняем файлы; любой другой Content-Type - тело целиком в файл.
 */
static HttpResponse handle_post(const HttpRequest& request,
								const std::string& upload_path)
{
	const std::map<std::string, std::string>& headers = request.get_headers();
	std::map<std::string, std::string>::const_iterator it =
		headers.find("content-type");

	if (it != headers.end())
	{
		std::string lower = it->second;
		for (std::string::size_type i = 0; i < lower.size(); ++i)
			lower[i] = std::tolower(static_cast<unsigned char>(lower[i]));
		if (lower.compare(0, 19, "multipart/form-data") == 0)
		{
			std::string boundary;
			if (!get_multipart_boundary(it->second, boundary))
				return HttpResponse(400);  // multipart без boundary
			return Router::handle_multipart_upload(
				upload_path, request.get_path(), request.get_body(), boundary);
		}
	}
	return Router::handle_post_method(upload_path, request.get_path(),
									  request.get_body());
}

const HttpResponse Router::handle_request(const HttpRequest& request,
										  const Server& server)
{
	const Route* route = matching(request.get_path(), server.get_route());
	HttpResponse response(404);

	// RFC 9110 §9.1 и §15.6.2: метод, который сервер не знает, - это 501,
	// а не 405 (405 значит "метод знаю, но здесь нельзя"). Проверяем до
	// поиска location, чтобы ответ не зависел от пути.
	if (request.get_method() == HttpRequest::UNKNOWN)
	{
		return apply_error_page(HttpResponse(501), server);
	}

	if (route == NULL)
	{
		return apply_error_page(response, server);
	}
	else if (route->get_has_redirect())
	{
		response = HttpResponse(route->get_redirect_code());
		response.set_header("Location", route->get_redirect_target());
	}
	else if (request.get_method() == HttpRequest::GET
			 || request.get_method() == HttpRequest::HEAD)
	{
		// HEAD обрабатывается как GET, а тело отрезает ClientSocket перед
		// отправкой. Разрешается отдельно: allowed_methods GET HEAD.
		if (!(route->get_allowed_methods()
			  & method_to_route_bit(request.get_method())))
			response = method_not_allowed(*route);
		else
		{
			std::string path = resolve_path(request.get_path(), *route);
			response = handle_get_method(path, request.get_path(), *route);
		}
	}
	else if (request.get_method() == HttpRequest::POST)
	{
		if (!(route->get_allowed_methods() & Route::POST))
			response = method_not_allowed(*route);
		else if (!route->get_upload_enabled())
			response = HttpResponse(403);
		else
		{
			std::string path = resolve_upload_path(request.get_path(), *route);
			response = handle_post(request, path);
		}
	}
	else if (request.get_method() == HttpRequest::DELETE)
	{
		if (!(route->get_allowed_methods() & Route::DELETE))
			response = method_not_allowed(*route);
		else
		{
			std::string path = resolve_path(request.get_path(), *route);
			response = handle_delete_method(path);
		}
	}

	return apply_error_page(response, server);
}

std::string Router::default_error_body(const HttpResponse& response)
{
	std::stringstream html;
	html << "<html><head><title>" << response.get_status_code() << " "
		 << response.get_reason_phrase() << "</title></head><body><h1>"
		 << response.get_status_code() << " " << response.get_reason_phrase()
		 << "</h1></body></html>";
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
					int tmp_read_byte = read(fd, &body[0] + total_read,
											 total_size - total_read);
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
					response.set_header("Content-Type",
										get_content_type(it->second));
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
		if (name == ".") continue;	// "." — сама директория, смысла нет
		html << "<li><a href=\"" << url << name << "\">" << name << "</a></li>";
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
		std::string index_path =
			join_path(resolve_path, route.get_index_file());
		struct stat index_stat;

		if (stat(index_path.c_str(), &index_stat) == 0
			&& S_ISREG(index_stat.st_mode))
			return read_file_response(index_path);

		// Шаг 2: index_file_ не найден — смотрим на autoindex_.
		if (route.get_autoindex())
			return generate_autoindex(resolve_path, url_path);

		// autoindex выключен, индекса нет - отвечаем 404, а не 403.
		// RFC 9110 §15.5.4: сервер, который не хочет раскрывать, что
		// ресурс существует, MAY ответить 404 вместо 403. Так ведёт себя
		// и тестер 42 (ждёт 404 на /directory/Yeah).
		return HttpResponse(404);
	}

	return read_file_response(resolve_path);
}

/**
 * @brief Код ответа по errno после неудачного open()
 *
 * errno здесь читать можно: запрет subject касается только read/write.
 */
static int open_error_status()
{
	if (errno == ENOENT || errno == ENOTDIR) return 404;  // нет каталога
	if (errno == EACCES) return 403;					  // нет прав
	if (errno == EISDIR) return 409;					  // это каталог
	return 500;
}

/**
 * @brief Записывает данные в открытый файл целиком и закрывает его
 * @return false при ошибке write (недописанный файл удаляется)
 *
 * Это обычный файл на диске, а не сокет: subject разрешает работать
 * с файлами без poll(). Проверяем и -1, и 0.
 */
static bool write_and_close(int fd, const std::string& data,
							const std::string& path)
{
	size_t total_written = 0;
	while (total_written < data.size())
	{
		int n = write(fd, data.c_str() + total_written,
					  data.size() - total_written);
		if (n <= 0)
		{
			close(fd);
			std::remove(path.c_str());	// не оставляем обрезанный файл
			return false;
		}
		total_written += n;
	}
	close(fd);
	return true;
}

static std::string html_escape(const std::string& s)
{
	std::string out;
	for (std::string::size_type i = 0; i < s.size(); ++i)
	{
		if (s[i] == '<') out += "&lt;";
		else if (s[i] == '>') out += "&gt;";
		else if (s[i] == '&') out += "&amp;";
		else if (s[i] == '"') out += "&quot;";
		else out += s[i];
	}
	return out;
}

/**
 * @brief Создаёт в каталоге файл с новым именем upload_<время>_<n>
 * @param dir  Каталог на диске
 * @param name [out] Придуманное имя
 * @return fd открытого файла или -1
 *
 * O_EXCL: open() откажет (EEXIST), если файл уже есть, - так два запроса
 * в одну секунду не перезапишут друг друга, а получат разные имена.
 */
static int create_unique_file(const std::string& dir, std::string& name)
{
	static unsigned long counter = 0;

	for (int attempt = 0; attempt < 100; ++attempt)
	{
		std::stringstream ss;
		ss << "upload_" << std::time(NULL) << "_" << counter++;
		name = ss.str();
		std::string path = (dir[dir.size() - 1] == '/') ? dir + name
														 : dir + "/" + name;
		int fd = open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
		if (fd != -1 || errno != EEXIST) return fd;
	}
	return -1;
}

/**
 * @brief Сохраняет тело POST в файл
 * @param upload_path Путь на диске (upload_dir + хвост URL)
 * @param url_path    Путь из запроса - для заголовка Location
 * @param body        Тело запроса
 *
 * POST /uploads/a.txt -> файл a.txt (перезаписывается, если был).
 * POST /uploads/      -> URL указывает на каталог, имя придумывает сервер.
 *
 * RFC 9110 §9.3.3 и §15.3.2: если POST создал ресурс, сервер SHOULD
 * ответить 201 с Location - адресом нового ресурса.
 */
HttpResponse Router::handle_post_method(const std::string& upload_path,
										const std::string& url_path,
										const std::string& body)
{
	std::string file_path = upload_path;
	std::string location = url_path;
	int fd;

	struct stat st;
	if (stat(upload_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
	{
		std::string name;
		fd = create_unique_file(upload_path, name);
		file_path = join_path(upload_path, name);
		location = join_path(url_path, name);
	}
	else if (upload_path[upload_path.size() - 1] == '/')
	{
		return HttpResponse(404);  // URL на каталог, а каталога нет
	}
	else
	{
		fd = open(upload_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
	}
	if (fd == -1) return HttpResponse(open_error_status());
	if (!write_and_close(fd, body, file_path)) return HttpResponse(500);

	HttpResponse response(201);
	response.set_header("Location", location);
	return response;
}

/**
 * @brief Сохраняет файлы из multipart/form-data (RFC 7578)
 * @param upload_dir Каталог на диске (форма шлёт POST на /uploads/)
 * @param url_path   URL каталога - для Location и ссылок в ответе
 * @param body       Тело запроса
 * @param boundary   Разделитель из Content-Type
 *
 * Сначала проверяем ВСЕ части (формат, имена), потом пишем: так битый
 * запрос не оставит на диске половину файлов. Поля без filename (обычные
 * <input type="text">) пропускаем. Файл с таким именем перезаписывается.
 */
HttpResponse Router::handle_multipart_upload(const std::string& upload_dir,
											 const std::string& url_path,
											 const std::string& body,
											 const std::string& boundary)
{
	struct stat st;
	if (stat(upload_dir.c_str(), &st) == -1) return HttpResponse(404);
	if (!S_ISDIR(st.st_mode)) return HttpResponse(409);	 // нужен каталог

	std::vector<MultipartPart> parts;
	if (!parse_multipart(body, boundary, parts)) return HttpResponse(400);

	std::vector<const MultipartPart*> files;
	std::vector<std::string> names;
	for (size_t i = 0; i < parts.size(); ++i)
	{
		// Пустой filename - браузер так шлёт <input type="file"> без файла.
		if (parts[i].filename.empty()) continue;
		std::string name = sanitize_filename(parts[i].filename);
		if (name.empty()) return HttpResponse(400);
		files.push_back(&parts[i]);
		names.push_back(name);
	}
	if (files.empty()) return HttpResponse(400);  // нечего сохранять

	std::string list;
	for (size_t i = 0; i < files.size(); ++i)
	{
		std::string path = join_path(upload_dir, names[i]);
		int fd = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (fd == -1) return HttpResponse(open_error_status());
		if (!write_and_close(fd, files[i]->data, path))
			return HttpResponse(500);

		std::stringstream size;
		size << files[i]->data.size();
		std::string url = html_escape(join_path(url_path, names[i]));
		list += "<li><a href=\"" + url + "\">" + html_escape(names[i])
				+ "</a> (" + size.str() + " bytes)</li>";
	}

	// RFC 9110 §15.3.2: 201 + Location нового ресурса. Файлов может быть
	// несколько - в Location первый, полный список в теле.
	HttpResponse response(201);
	response.set_header("Location", join_path(url_path, names[0]));
	response.set_header("Content-Type", "text/html");
	response.set_body("<html><body><h1>Uploaded</h1><ul>" + list
					  + "</ul><a href=\"" + html_escape(url_path)
					  + "\">back to directory</a></body></html>");
	return response;
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
