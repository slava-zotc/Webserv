#include <iostream>
#include <string>
#include <vector>

#include "Route.hpp"
#include "Router.hpp"

static Route build_route(const std::string& prefix)
{
	CFG_Route cfg;
	cfg.prefix_ = prefix;
	cfg.root_ = "/var/www";
	cfg.autoindex_ = false;
	cfg.index_file_ = "index.html";
	cfg.has_redirect_ = false;
	cfg.redirect_target_ = "";
	cfg.allowed_methods_ = Route::GET | Route::POST | Route::DELETE;
	return Route(cfg);
}

static Route build_route_with_root(const std::string& prefix,
									const std::string& root)
{
	CFG_Route cfg;
	cfg.prefix_ = prefix;
	cfg.root_ = root;
	cfg.autoindex_ = false;
	cfg.index_file_ = "index.html";
	cfg.has_redirect_ = false;
	cfg.redirect_target_ = "";
	cfg.allowed_methods_ = Route::GET | Route::POST | Route::DELETE;
	return Route(cfg);
}

static void check(const std::string& test_name,
				   const Route* result,
				   const Route* expected)
{
	if (result == expected)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected: "
				   << (expected ? expected->get_prefix() : "NULL")
				   << ", got: "
				   << (result ? result->get_prefix() : "NULL")
				   << ")" << std::endl;
	}
}

static void check_null(const std::string& test_name, const Route* result)
{
	if (result == NULL)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected NULL, got: "
				   << result->get_prefix() << ")" << std::endl;
	}
}

static void check_string(const std::string& test_name,
						  const std::string& result,
						  const std::string& expected)
{
	if (result == expected)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected: \"" << expected
				   << "\", got: \"" << result << "\")" << std::endl;
	}
}

void test_matching()
{
	std::vector<Route> routes;
	routes.push_back(build_route("/"));		// routes[0]
	routes.push_back(build_route("/api"));		// routes[1]
	routes.push_back(build_route("/api/v1"));	// routes[2]
	routes.push_back(build_route("/static"));	// routes[3]

	// 1. Точное совпадение
	check("exact match /api",
		  Router::matching("/api", routes), &routes[1]);

	// 2. Совпадение с продолжением сегмента пути
	check("segment continuation /api/foo",
		  Router::matching("/api/foo", routes), &routes[1]);

	// 3. Ложное совпадение по префиксу слова — /api НЕ должен матчить
	//    /apikey; вместо этого должен сработать fallback на корень
	check("word-prefix false match /apikey",
		  Router::matching("/apikey", routes), &routes[0]);

	// 4. Корень как fallback для несовпавшего пути
	check("root fallback /foo",
		  Router::matching("/foo", routes), &routes[0]);

	// 5. Корень — точное совпадение
	check("root exact match /",
		  Router::matching("/", routes), &routes[0]);

	// 6. Выбор самого длинного из нескольких совпавших префиксов
	check("longest prefix wins /api/v1/users",
		  Router::matching("/api/v1/users", routes), &routes[2]);

	// 7. Путь короче любого нетривиального префикса —
	//    совпадает только с корнем, без выхода за границы строки
	check("path shorter than prefix /ap",
		  Router::matching("/ap", routes), &routes[0]);

	// 8. Пустой список routes — всегда NULL
	std::vector<Route> empty_routes;
	check_null("empty routes list",
			   Router::matching("/anything", empty_routes));
}

void test_resolve_path()
{
	// 1. Пример из сабжекта: /kapouet -> /tmp/www,
	//    /kapouet/pouic/toto/pouet -> /tmp/www/pouic/toto/pouet
	Route kapouet = build_route_with_root("/kapouet", "/tmp/www");
	check_string("subject example",
				 Router::resolve_path("/kapouet/pouic/toto/pouet", kapouet),
				 "/tmp/www/pouic/toto/pouet");

	// 2. Точное совпадение с prefix — хвост пустой
	check_string("exact prefix match",
				 Router::resolve_path("/kapouet", kapouet),
				 "/tmp/www/");

	// 3. Корень как prefix — не должен терять разделитель
	Route root = build_route_with_root("/", "/var/www");
	check_string("root prefix keeps separator",
				 Router::resolve_path("/foo", root),
				 "/var/www/foo");

	// 4. Корень, путь тоже корень
	check_string("root prefix, root path",
				 Router::resolve_path("/", root),
				 "/var/www/");

	// 5. Вложенный путь под обычным префиксом
	Route api = build_route_with_root("/api", "/srv/api");
	check_string("nested path under prefix",
				 Router::resolve_path("/api/v1/users", api),
				 "/srv/api/v1/users");
}

int main()
{
	test_matching();
	test_resolve_path();
	return 0;
}