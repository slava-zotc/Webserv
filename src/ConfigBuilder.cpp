#include "ConfigBuilder.hpp"

#include <sys/stat.h>

#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

static int parsePort(const std::string& raw)
{
	std::stringstream ss(raw);
	int port;
	ss >> port;
	if (ss.fail() || !ss.eof() || port < 1 || port > 65535)
	{
		throw std::runtime_error("Invalid port in listen directive: " + raw);
	}
	return port;
}

/*
 * listen принимает "порт" или "хост:порт" (subject: "interface:port").
 * Без хоста слушаем все интерфейсы - 0.0.0.0, как nginx.
 * Сам хост здесь не резолвится: это делает ListeningSocket через
 * getaddrinfo, там же будет понятная ошибка, если имя неизвестно.
 */
static void parseListen(const std::string& raw, std::string& host, int& port)
{
	std::string::size_type colon = raw.rfind(':');
	if (colon == std::string::npos)
	{
		host = "0.0.0.0";
		port = parsePort(raw);
		return;
	}
	host = raw.substr(0, colon);
	if (host.empty())
	{
		throw std::runtime_error("Empty host in listen directive: " + raw);
	}
	port = parsePort(raw.substr(colon + 1));
}

static int parseErrorCode(const std::string& raw)
{
	std::stringstream ss(raw);
	int code;
	ss >> code;
	if (ss.fail() || !ss.eof() || code < 400 || code > 599)
	{
		throw std::runtime_error("Invalid error_page code: " + raw);
	}
	return code;
}

static void validateRoot(const std::string& root)
{
	struct stat st;
	if (stat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
	{
		throw std::runtime_error("Invalid root directive: " + root
								 + " does not exist or is not a directory");
	}
}

static bool parseBool(const std::string& raw)
{
	if (raw == "on" || raw == "true") return true;
	if (raw == "off" || raw == "false") return false;
	throw std::runtime_error("Invalid boolean value: " + raw);
}

static int parseAllowedMethods(const std::vector<std::string>& values)
{
	int mask = 0;
	for (size_t i = 0; i < values.size(); ++i)
	{
		if (values[i] == "GET")
			mask |= Route::GET;
		else if (values[i] == "POST")
			mask |= Route::POST;
		else if (values[i] == "DELETE")
			mask |= Route::DELETE;
		else
			throw std::runtime_error("Invalid method in allowed_methods: "
									 + values[i]);
	}
	return mask;
}

CFG_Route buildCfgRoute(const ConfigBlock& block, const std::string& parentRoot)
{
	CFG_Route cfg = CFG_Route();
	cfg.prefix_ = block.arg;

	std::map<std::string, std::vector<std::string> >::const_iterator it;

	it = block.directives.find("root");
	if (it != block.directives.end() && !it->second.empty())
	{
		cfg.root_ = it->second[0];
	}
	else
	{
		cfg.root_ = parentRoot;
	}
	if (cfg.root_.empty())
	{
		throw std::runtime_error("No root defined for location " + cfg.prefix_
								 + " and no root inherited from server");
	}
	validateRoot(cfg.root_);

	it = block.directives.find("allowed_methods");
	if (it != block.directives.end())
	{
		cfg.allowed_methods_ = parseAllowedMethods(it->second);
	}

	it = block.directives.find("autoindex");
	if (it != block.directives.end() && !it->second.empty())
	{
		cfg.autoindex_ = parseBool(it->second[0]);
	}

	it = block.directives.find("index");
	if (it != block.directives.end() && !it->second.empty())
	{
		cfg.index_file_ = it->second[0];
	}

	it = block.directives.find("upload_enabled");
	if (it != block.directives.end() && !it->second.empty())
	{
		cfg.upload_enabled_ = parseBool(it->second[0]);
	}
	it = block.directives.find("upload_dir");
	if (it != block.directives.end() && !it->second.empty())
	{
		cfg.upload_dir_ = it->second[0];
		if (cfg.upload_enabled_)
		{
			validateRoot(cfg.upload_dir_);
		}
	}

	it = block.directives.find("return");
	if (it != block.directives.end() && it->second.size() != 2)
	{
		throw std::runtime_error("location " + cfg.prefix_
			+ ": return: expected 2 arguments (code and target)");
	}
	if (it != block.directives.end())
	{
		std::stringstream ss(it->second[0]);
		int code;
		ss >> code;
		if (ss.fail() || !ss.eof() || (code != 301 && code != 302))
		{
			throw std::runtime_error(
				"Invalid redirect code in return directive: " + it->second[0]);
		}
		cfg.has_redirect_ = true;
		cfg.redirect_target_ = it->second[1];
		cfg.redirect_code_ = code;
	}

	it = block.directives.find("cgi");
	if (it != block.directives.end())
	{
		if (it->second.size() != 2)
		{
			throw std::runtime_error("location " + cfg.prefix_
            + ": cgi: expected 2 arguments (extension and interpreter path)");
		}

		std::string extension = it->second[0];
		std::string path_interpreter = it->second[1];
		if (extension.length() < 2|| extension[0] != '.')
		{
			throw std::runtime_error("location " + cfg.prefix_
            + ": cgi: invalid extension '" + extension
            + "' (must start with '.' and have at least one character after it)");
		}
		if (access(path_interpreter.c_str(), X_OK) == -1)
		{
			throw std::runtime_error("location " + cfg.prefix_
            + ": cgi: interpreter '" + path_interpreter
            + "' does not exist or is not executable");
		}
		cfg.extensions_ = extension;
		cfg.path_interpreter_ = path_interpreter;
	}

	return cfg;
}

CFG_Server buildCfgServer(const ConfigBlock& block)
{
	CFG_Server cfg = CFG_Server();

	if (!block.directives.count("listen"))
		throw std::runtime_error("server: missing listen directive");
	if (!block.directives.count("root"))
		throw std::runtime_error("server: missing root directive");

	const std::vector<std::string>& listenValues =
		block.directives.at("listen");
	if (listenValues.empty())
	{
		throw std::runtime_error("Empty listen directive");
	}
	if (listenValues.size() != 1)
	{
		throw std::runtime_error(
			"listen: expected exactly one argument (port or host:port)");
	}
	parseListen(listenValues[0], cfg.host_, cfg.port_);

	const std::vector<std::string>& rootValues = block.directives.at("root");
	if (rootValues.empty())
	{
		throw std::runtime_error("Empty root directive");
	}
	std::string root = rootValues[0];
	validateRoot(root);

	cfg.max_body_size_ = 1048576;

	std::map<std::string, std::vector<std::string> >::const_iterator it;

	it = block.directives.find("client_max_body_size");
	if (it != block.directives.end() && !it->second.empty())
	{
		std::stringstream ss(it->second[0]);
		long size;
		ss >> size;
		if (ss.fail() || !ss.eof() || size < 0)
		{
			throw std::runtime_error("Invalid client_max_body_size: "
									 + it->second[0]);
		}
		cfg.max_body_size_ = size;
	}

	// error_page <код> [<код> ...] <путь>;  можно повторять.
	// Если один код указан дважды, побеждает последняя строка (как в nginx).
	for (size_t i = 0; i < block.error_pages.size(); ++i)
	{
		const std::vector<std::string>& values = block.error_pages[i];
		if (values.size() < 2)
		{
			throw std::runtime_error(
				"error_page: expected at least one code and a path");
		}
		const std::string& path = values[values.size() - 1];
		for (size_t j = 0; j + 1 < values.size(); ++j)
		{
			cfg.error_pages_[parseErrorCode(values[j])] = path;
		}
	}

	for (size_t i = 0; i < block.children.size(); ++i)
	{
		if (block.children[i].name == "location")
		{
			cfg.routes_.push_back(buildCfgRoute(block.children[i], root));
		}
	}

	return cfg;
}

/*
 * Два server-блока на одном host:port - ошибка конфига: виртуальных хостов
 * (выбора блока по заголовку Host) у нас нет. Проверяем до создания сокетов,
 * чтобы ошибка в конфиге не успела открыть ни одного.
 *
 * map: ключ host:port -> номер блока, где он встретился впервые.
 * insert не перезаписывает: если ключ уже есть, .second == false,
 * а .first указывает на старую запись (откуда берём номер первого блока).
 */
void checkDuplicateListens(const std::vector<Server*>& servers)
{
	typedef std::pair<std::string, int> Endpoint;
	std::map<Endpoint, size_t> seen;

	for (size_t i = 0; i < servers.size(); ++i)
	{
		Endpoint key(servers[i]->get_host(), servers[i]->get_port());
		std::pair<std::map<Endpoint, size_t>::iterator, bool> res =
			seen.insert(std::make_pair(key, i));
		if (!res.second)
		{
			std::stringstream ss;
			ss << "Duplicate listen " << key.first << ":" << key.second
			   << " in server blocks " << res.first->second + 1 << " and "
			   << i + 1;
			throw std::runtime_error(ss.str());
		}
	}
}
