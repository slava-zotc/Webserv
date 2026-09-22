#include <cctype>
#include <cerrno>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include "ConfigBuilder.hpp"
#include "ConfigParser.hpp"
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

static void check_int(const std::string& test_name, int result, int expected)
{
	if (result == expected)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected: " << expected
				   << ", got: " << result << ")" << std::endl;
	}
}

static void check_bool(const std::string& test_name, bool result, bool expected)
{
	if (result == expected)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected: " << (expected ? "true" : "false")
				   << ", got: " << (result ? "true" : "false") << ")" << std::endl;
	}
}

static void check_ulong(const std::string& test_name,
						 unsigned long result,
						 unsigned long expected)
{
	if (result == expected)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected: " << expected
				   << ", got: " << result << ")" << std::endl;
	}
}

static void check_size(const std::string& test_name, size_t result, size_t expected)
{
	if (result == expected)
	{
		std::cout << "[PASS] " << test_name << std::endl;
	}
	else
	{
		std::cout << "[FAIL] " << test_name
				   << " (expected: " << expected
				   << ", got: " << result << ")" << std::endl;
	}
}

// Оборачивают expr в try/catch и сверяют, бросило ли исключение —
// нужно, чтобы не дублировать один и тот же try/catch в каждом
// негативном тесте на некорректный конфиг.
#define CHECK_THROWS(name, expr) \
	do { \
		bool threw_ = false; \
		try { (expr); } catch (const std::exception&) { threw_ = true; } \
		check_bool((name), threw_, true); \
	} while (0)

#define CHECK_NOTHROW(name, expr) \
	do { \
		bool threw_ = false; \
		try { (expr); } catch (const std::exception&) { threw_ = true; } \
		check_bool((name), threw_, false); \
	} while (0)

static const std::string FIXTURE_ROOT = "/tmp/webserv_test_fixtures";
static const std::string FIXTURE_MISSING = FIXTURE_ROOT + "/definitely_missing_xyz";

static void ensure_dir(const std::string& path)
{
	if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST)
	{
		throw std::runtime_error("Failed to create fixture directory: " + path);
	}
}

static void setup_config_fixtures()
{
	ensure_dir(FIXTURE_ROOT);
	ensure_dir(FIXTURE_ROOT + "/server_root");
	ensure_dir(FIXTURE_ROOT + "/alt_root");
	ensure_dir(FIXTURE_ROOT + "/upload_dir");
}

// Копия tokenize() из src/main.cpp: она static и нигде не экспонируется
// через заголовок, поэтому тестам, которые хотят скормить ConfigParser
// сырой текст конфига, приходится держать свою копию.
static std::vector<std::string> tokenize(const std::string& content)
{
	std::vector<std::string> tokens;
	std::string current;

	for (size_t i = 0; i < content.size(); ++i)
	{
		char c = content[i];

		if (c == '#')
		{
			while (i < content.size() && content[i] != '\n') ++i;
			continue;
		}
		if (c == '{' || c == '}' || c == ';')
		{
			if (!current.empty()) { tokens.push_back(current); current.clear(); }
			tokens.push_back(std::string(1, c));
		}
		else if (std::isspace(static_cast<unsigned char>(c)))
		{
			if (!current.empty()) { tokens.push_back(current); current.clear(); }
		}
		else
		{
			current += c;
		}
	}
	if (!current.empty()) tokens.push_back(current);
	return tokens;
}

static ConfigBlock make_server_block(int port, const std::string& root)
{
	ConfigBlock block;
	block.name = "server";

	std::stringstream ss;
	ss << port;
	std::vector<std::string> listenValues;
	listenValues.push_back(ss.str());
	block.directives["listen"] = listenValues;

	std::vector<std::string> rootValues;
	rootValues.push_back(root);
	block.directives["root"] = rootValues;

	return block;
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

void test_config_parser()
{
	// 1. Простой server{} с одной директивой
	{
		std::vector<std::string> tokens = tokenize("server { listen 8080; }");
		ConfigParser parser(tokens);
		std::vector<ConfigBlock> blocks = parser.parse();
		check_size("parser: one top-level block", blocks.size(), (size_t)1);
		if (!blocks.empty())
		{
			check_string("parser: block name is server", blocks[0].name, "server");
			std::map<std::string, std::vector<std::string> >::const_iterator it =
				blocks[0].directives.find("listen");
			check_bool("parser: listen directive present", it != blocks[0].directives.end(), true);
			if (it != blocks[0].directives.end())
			{
				check_size("parser: listen has one value", it->second.size(), (size_t)1);
				check_string("parser: listen value is 8080", it->second[0], "8080");
			}
		}
	}

	// 2. Вложенный location внутри server
	{
		std::vector<std::string> tokens = tokenize(
			"server { listen 8080; root /tmp; location /kapouet { autoindex on; } }");
		ConfigParser parser(tokens);
		std::vector<ConfigBlock> blocks = parser.parse();
		check_size("parser: server with nested location - block count", blocks.size(), (size_t)1);
		if (!blocks.empty())
		{
			check_size("parser: one nested location", blocks[0].children.size(), (size_t)1);
			if (!blocks[0].children.empty())
			{
				check_string("parser: location name", blocks[0].children[0].name, "location");
				check_string("parser: location arg is /kapouet", blocks[0].children[0].arg, "/kapouet");
			}
		}
	}

	// 3. Директива с несколькими значениями
	{
		std::vector<std::string> tokens = tokenize(
			"server { listen 8080; root /tmp; "
			"location / { allowed_methods GET POST DELETE; return 301 /new; } }");
		ConfigParser parser(tokens);
		std::vector<ConfigBlock> blocks = parser.parse();
		if (!blocks.empty() && !blocks[0].children.empty())
		{
			const ConfigBlock& loc = blocks[0].children[0];

			std::map<std::string, std::vector<std::string> >::const_iterator it =
				loc.directives.find("allowed_methods");
			check_bool("parser: allowed_methods present", it != loc.directives.end(), true);
			if (it != loc.directives.end())
				check_size("parser: allowed_methods has 3 values", it->second.size(), (size_t)3);

			it = loc.directives.find("return");
			check_bool("parser: return present", it != loc.directives.end(), true);
			if (it != loc.directives.end() && it->second.size() == 2)
			{
				check_string("parser: return code", it->second[0], "301");
				check_string("parser: return target", it->second[1], "/new");
			}
		}
	}

	// 4. Комментарии пропускаются
	{
		std::vector<std::string> tokens = tokenize(
			"server {\n# comment about listen\n listen 8080; # trailing comment\n}");
		ConfigParser parser(tokens);
		std::vector<ConfigBlock> blocks = parser.parse();
		check_size("parser: comments skipped - block count", blocks.size(), (size_t)1);
		if (!blocks.empty())
		{
			std::map<std::string, std::vector<std::string> >::const_iterator it =
				blocks[0].directives.find("listen");
			check_bool("parser: comments skipped - listen present",
					   it != blocks[0].directives.end(), true);
		}
	}

	// 5. Несколько верхнеуровневых server{} блоков (несколько interface:port пар)
	{
		std::vector<std::string> tokens = tokenize("server { listen 8080; } server { listen 9090; }");
		ConfigParser parser(tokens);
		std::vector<ConfigBlock> blocks = parser.parse();
		check_size("parser: two top-level server blocks", blocks.size(), (size_t)2);
		if (blocks.size() == 2)
		{
			check_string("parser: first server listen", blocks[0].directives.at("listen")[0], "8080");
			check_string("parser: second server listen", blocks[1].directives.at("listen")[0], "9090");
		}
	}
}

void test_config_builder_server()
{
	const std::string root = FIXTURE_ROOT + "/server_root";

	// 1. Валидный listen+root, дефолтный max_body_size_
	{
		ConfigBlock block = make_server_block(8080, root);
		CFG_Server cfg = buildCfgServer(block);
		check_int("builder(server): port parsed", cfg.port_, 8080);
		check_ulong("builder(server): default max_body_size", cfg.max_body_size_, 1048576UL);
	}

	// 2. Явный client_max_body_size
	{
		ConfigBlock block = make_server_block(8080, root);
		std::vector<std::string> v;
		v.push_back("2097152");
		block.directives["client_max_body_size"] = v;
		CFG_Server cfg = buildCfgServer(block);
		check_ulong("builder(server): explicit max_body_size", cfg.max_body_size_, 2097152UL);
	}

	// 3. error_page
	{
		ConfigBlock block = make_server_block(8080, root);
		std::vector<std::string> v;
		v.push_back("404");
		v.push_back("/404.html");
		block.directives["error_page"] = v;
		CFG_Server cfg = buildCfgServer(block);
		std::map<int, std::string>::const_iterator it = cfg.error_pages_.find(404);
		check_bool("builder(server): error_page recorded", it != cfg.error_pages_.end(), true);
		if (it != cfg.error_pages_.end())
			check_string("builder(server): error_page path", it->second, "/404.html");
	}

	// 4. Невалидный порт
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"][0] = "abc";
		CHECK_THROWS("builder(server): non-numeric port throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"][0] = "99999999";
		CHECK_THROWS("builder(server): out-of-range port throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"][0] = "0";
		CHECK_THROWS("builder(server): zero port throws", buildCfgServer(block));
	}

	// 5. Отсутствует listen/root
	{
		ConfigBlock block;
		block.name = "server";
		std::vector<std::string> v;
		v.push_back(root);
		block.directives["root"] = v;
		CHECK_THROWS("builder(server): missing listen throws", buildCfgServer(block));
	}
	{
		ConfigBlock block;
		block.name = "server";
		std::vector<std::string> v;
		v.push_back("8080");
		block.directives["listen"] = v;
		CHECK_THROWS("builder(server): missing root throws", buildCfgServer(block));
	}

	// 6. root указывает на несуществующий путь
	{
		ConfigBlock block = make_server_block(8080, FIXTURE_MISSING);
		CHECK_THROWS("builder(server): nonexistent root throws", buildCfgServer(block));
	}

	// 7. Пустые значения listen/root
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"] = std::vector<std::string>();
		CHECK_THROWS("builder(server): empty listen value throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["root"] = std::vector<std::string>();
		CHECK_THROWS("builder(server): empty root value throws", buildCfgServer(block));
	}

	// 8. Два location{} -> два route
	{
		ConfigBlock block = make_server_block(8080, root);
		ConfigBlock loc1;
		loc1.name = "location";
		loc1.arg = "/a";
		ConfigBlock loc2;
		loc2.name = "location";
		loc2.arg = "/b";
		block.children.push_back(loc1);
		block.children.push_back(loc2);
		CFG_Server cfg = buildCfgServer(block);
		check_size("builder(server): two locations produce two routes", cfg.routes_.size(), (size_t)2);
		if (cfg.routes_.size() == 2)
		{
			check_string("builder(server): first route prefix", cfg.routes_[0].get_prefix(), "/a");
			check_string("builder(server): second route prefix", cfg.routes_[1].get_prefix(), "/b");
		}
	}
}

void test_config_builder_route()
{
	const std::string root = FIXTURE_ROOT + "/server_root";
	const std::string altRoot = FIXTURE_ROOT + "/alt_root";
	const std::string uploadDir = FIXTURE_ROOT + "/upload_dir";

	// 1. allowed_methods bitmask
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> methods;
		methods.push_back("GET");
		block.directives["allowed_methods"] = methods;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_int("builder(route): GET only mask", cfg.allowed_methods_, Route::GET);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> methods;
		methods.push_back("GET");
		methods.push_back("POST");
		block.directives["allowed_methods"] = methods;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_int("builder(route): GET+POST mask", cfg.allowed_methods_, Route::GET | Route::POST);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> methods;
		methods.push_back("GET");
		methods.push_back("POST");
		methods.push_back("DELETE");
		block.directives["allowed_methods"] = methods;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_int("builder(route): GET+POST+DELETE mask",
				  cfg.allowed_methods_, Route::GET | Route::POST | Route::DELETE);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> methods;
		methods.push_back("PATCH");
		block.directives["allowed_methods"] = methods;
		CHECK_THROWS("builder(route): unknown method throws", buildCfgRoute(block, root));
	}

	// 2. redirect
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/old";
		std::vector<std::string> v;
		v.push_back("301");
		v.push_back("/new");
		block.directives["return"] = v;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_bool("builder(route): redirect flag set", cfg.has_redirect_, true);
		check_string("builder(route): redirect target", cfg.redirect_target_, "/new");
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/old";
		std::vector<std::string> v;
		v.push_back("404");
		v.push_back("/x");
		block.directives["return"] = v;
		CHECK_THROWS("builder(route): invalid redirect code throws", buildCfgRoute(block, root));
	}

	// 3. Маппинг URL->root: наследование и переопределение (пример /kapouet из сабжекта)
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/kapouet";
		CFG_Route cfg = buildCfgRoute(block, root);
		check_string("builder(route): inherits parent root", cfg.root_, root);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/kapouet";
		std::vector<std::string> v;
		v.push_back(altRoot);
		block.directives["root"] = v;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_string("builder(route): explicit root overrides parent", cfg.root_, altRoot);
	}

	// 4. Ни location, ни родитель не имеют root
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/kapouet";
		CHECK_THROWS("builder(route): no root anywhere throws", buildCfgRoute(block, ""));
	}

	// 5. autoindex (листинг директории)
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("on");
		block.directives["autoindex"] = v;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_bool("builder(route): autoindex on", cfg.autoindex_, true);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("off");
		block.directives["autoindex"] = v;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_bool("builder(route): autoindex off", cfg.autoindex_, false);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("maybe");
		block.directives["autoindex"] = v;
		CHECK_THROWS("builder(route): invalid autoindex value throws", buildCfgRoute(block, root));
	}

	// 6. index (файл по умолчанию для директории)
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("index.html");
		block.directives["index"] = v;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_string("builder(route): index file", cfg.index_file_, "index.html");
	}

	// 7. upload (разрешение загрузки + путь хранения)
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> enabled;
		enabled.push_back("on");
		std::vector<std::string> dir;
		dir.push_back(uploadDir);
		block.directives["upload_enabled"] = enabled;
		block.directives["upload_dir"] = dir;
		CFG_Route cfg = buildCfgRoute(block, root);
		check_bool("builder(route): upload enabled", cfg.upload_enabled_, true);
		check_string("builder(route): upload dir", cfg.upload_dir_, uploadDir);
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> enabled;
		enabled.push_back("on");
		std::vector<std::string> dir;
		dir.push_back(FIXTURE_MISSING);
		block.directives["upload_enabled"] = enabled;
		block.directives["upload_dir"] = dir;
		CHECK_THROWS("builder(route): upload_dir missing while enabled throws", buildCfgRoute(block, root));
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> dir;
		dir.push_back(FIXTURE_MISSING);
		block.directives["upload_dir"] = dir;
		CHECK_NOTHROW("builder(route): upload_dir missing while disabled does not throw",
					  buildCfgRoute(block, root));
	}
}

void test_config_invalid()
{
	const std::string root = FIXTURE_ROOT + "/server_root";

	// --- Уровень ConfigParser: синтаксически битые конфиги ---
	{
		std::vector<std::string> tokens = tokenize("server { listen 8080;");
		CHECK_THROWS("invalid-parser: unclosed block throws", ConfigParser(tokens).parse());
	}
	{
		std::vector<std::string> tokens = tokenize("server { listen 8080 }");
		CHECK_THROWS("invalid-parser: directive without semicolon throws", ConfigParser(tokens).parse());
	}
	{
		std::vector<std::string> tokens = tokenize("asgasgasfg afgafg");
		CHECK_THROWS("invalid-parser: top-level garbage without block throws",
					 ConfigParser(tokens).parse());
	}
	{
		std::vector<std::string> tokens = tokenize("");
		CHECK_NOTHROW("invalid-parser: empty config does not throw", ConfigParser(tokens).parse());
	}
	{
		std::vector<std::string> tokens = tokenize(
			"server { foo bar; listen 8080; root " + root + "; }");
		CHECK_NOTHROW("invalid-parser: unknown directive is ignored, not rejected",
					  ConfigParser(tokens).parse());
	}

	// --- ConfigBuilder: семантически некорректные значения (buildCfgServer) ---
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"][0] = "abc";
		CHECK_THROWS("invalid-builder: listen abc throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"][0] = "99999999";
		CHECK_THROWS("invalid-builder: listen 99999999 throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"][0] = "0";
		CHECK_THROWS("invalid-builder: listen 0 throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["listen"] = std::vector<std::string>();
		CHECK_THROWS("invalid-builder: listen with no value throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, root);
		block.directives["root"] = std::vector<std::string>();
		CHECK_THROWS("invalid-builder: root with no value throws", buildCfgServer(block));
	}
	{
		ConfigBlock block;
		block.name = "server";
		std::vector<std::string> v;
		v.push_back(root);
		block.directives["root"] = v;
		CHECK_THROWS("invalid-builder: missing listen directive throws", buildCfgServer(block));
	}
	{
		ConfigBlock block;
		block.name = "server";
		std::vector<std::string> v;
		v.push_back("8080");
		block.directives["listen"] = v;
		CHECK_THROWS("invalid-builder: missing root directive throws", buildCfgServer(block));
	}
	{
		ConfigBlock block = make_server_block(8080, FIXTURE_MISSING);
		CHECK_THROWS("invalid-builder: root points to missing directory throws", buildCfgServer(block));
	}

	// --- ConfigBuilder: семантически некорректные значения (buildCfgRoute) ---
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("PATCH");
		block.directives["allowed_methods"] = v;
		CHECK_THROWS("invalid-builder: allowed_methods PATCH throws", buildCfgRoute(block, root));
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("maybe");
		block.directives["autoindex"] = v;
		CHECK_THROWS("invalid-builder: autoindex maybe throws", buildCfgRoute(block, root));
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> v;
		v.push_back("maybe");
		block.directives["upload_enabled"] = v;
		CHECK_THROWS("invalid-builder: upload_enabled maybe throws", buildCfgRoute(block, root));
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/old";
		std::vector<std::string> v;
		v.push_back("404");
		v.push_back("/x");
		block.directives["return"] = v;
		CHECK_THROWS("invalid-builder: return 404 (not 301/302) throws", buildCfgRoute(block, root));
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/kapouet";
		CHECK_THROWS("invalid-builder: no root on location and no parent root throws",
					 buildCfgRoute(block, ""));
	}
	{
		ConfigBlock block;
		block.name = "location";
		block.arg = "/";
		std::vector<std::string> enabled;
		enabled.push_back("on");
		std::vector<std::string> dir;
		dir.push_back(FIXTURE_MISSING);
		block.directives["upload_enabled"] = enabled;
		block.directives["upload_dir"] = dir;
		CHECK_THROWS("invalid-builder: upload_dir missing while upload_enabled throws",
					 buildCfgRoute(block, root));
	}
}

void test_config_end_to_end()
{
	const std::string root = FIXTURE_ROOT + "/server_root";
	const std::string uploadDir = FIXTURE_ROOT + "/upload_dir";

	std::string text =
		"server {\n"
		"    listen 8080;\n"
		"    root " + root + ";\n"
		"    client_max_body_size 1048576;\n"
		"    error_page 404 /404.html;\n"
		"\n"
		"    location / {\n"
		"        allowed_methods GET POST DELETE;\n"
		"        autoindex on;\n"
		"        index index.html;\n"
		"        upload_enabled on;\n"
		"        upload_dir " + uploadDir + ";\n"
		"    }\n"
		"}\n";

	std::vector<std::string> tokens = tokenize(text);
	ConfigParser parser(tokens);
	std::vector<ConfigBlock> blocks = parser.parse();

	check_size("end-to-end: one server block", blocks.size(), (size_t)1);
	if (blocks.empty() || blocks[0].name != "server") return;

	CFG_Server cfg = buildCfgServer(blocks[0]);
	check_int("end-to-end: port", cfg.port_, 8080);
	check_ulong("end-to-end: max_body_size", cfg.max_body_size_, 1048576UL);

	std::map<int, std::string>::const_iterator errIt = cfg.error_pages_.find(404);
	check_bool("end-to-end: error_page present", errIt != cfg.error_pages_.end(), true);
	if (errIt != cfg.error_pages_.end())
		check_string("end-to-end: error_page path", errIt->second, "/404.html");

	check_size("end-to-end: one route", cfg.routes_.size(), (size_t)1);
	if (cfg.routes_.size() == 1)
	{
		const Route& r = cfg.routes_[0];
		check_string("end-to-end: route prefix", r.get_prefix(), "/");
		check_string("end-to-end: route root", r.get_root(), root);
		check_int("end-to-end: route methods",
				  r.get_allowed_methods(), Route::GET | Route::POST | Route::DELETE);
		check_bool("end-to-end: autoindex", r.get_autoindex(), true);
		check_string("end-to-end: index file", r.get_index_file(), "index.html");
		check_bool("end-to-end: upload enabled", r.get_upload_enabled(), true);
		check_string("end-to-end: upload dir", r.get_upload_dir(), uploadDir);
	}
}

int main()
{
	setup_config_fixtures();

	test_matching();
	test_resolve_path();
	test_config_parser();
	test_config_builder_server();
	test_config_builder_route();
	test_config_invalid();
	test_config_end_to_end();
	return 0;
}