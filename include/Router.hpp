#ifndef ROUTER_HPP
#define ROUTER_HPP
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Server.hpp"
class Router
{
public:
	static const HttpResponse handle_request(const HttpRequest& request,
											 const Server& server);
	static const Route* matching(const std::string& path,
								 const std::vector<Route>& route);
	static HttpResponse handle_get_method(const std::string& resolve_path,
										   const std::string& url_path,
										   const Route& route);
	static HttpResponse handle_post_method(const std::string& upload_path,
											const std::string& body);
	static HttpResponse handle_delete_method(const std::string& resolve_path);
	static std::string resolve_path(const std::string& path,
									const Route& route);
	static std::string resolve_upload_path(const std::string& path,
											const Route& route);

private:
	static std::string get_content_type(const std::string& path);
	/**
	 * @brief Склеивает base и name ровно одним '/' между ними,
	 * независимо от того, заканчивается ли base уже на '/'.
	 */
	static std::string join_path(const std::string& base,
								  const std::string& name);
	/**
	 * @brief Открывает и читает уже проверенный (существующий, обычный)
	 * файл, возвращает готовый 200-ответ с Content-Type по расширению.
	 * Используется и для прямого GET файла, и для найденного index_file_.
	 */
	static HttpResponse read_file_response(const std::string& file_path);
	/**
	 * @brief Генерирует HTML-листинг директории (autoindex).
	 * @param dir_path Путь к директории в файловой системе (для opendir)
	 * @param url_path Исходный URL-путь запроса (для построения ссылок)
	 */
	static HttpResponse generate_autoindex(const std::string& dir_path,
											const std::string& url_path);
};

#endif	// ROUTER_HPP