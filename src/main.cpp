#include <cctype>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ConfigBuilder.hpp"
#include "ConfigParser.hpp"
#include "Core.hpp"
#include "Route.hpp"
#include "Server.hpp"

static std::string readFile(const char* path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		throw std::runtime_error(std::string("Cannot open config file: ")
								 + path);
	}
	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

static std::vector<std::string> tokenize(const std::string& content)
{
	std::vector<std::string> tokens;
	std::string current;

	for (size_t i = 0; i < content.size(); ++i)
	{
		char c = content[i];

		if (c == '#')
		{  // comment: skip to end of line
			while (i < content.size() && content[i] != '\n') ++i;
			continue;
		}
		if (c == '{' || c == '}' || c == ';')
		{
			if (!current.empty())
			{
				tokens.push_back(current);
				current.clear();
			}
			tokens.push_back(std::string(1, c));
		}
		else if (std::isspace(static_cast<unsigned char>(c)))
		{
			if (!current.empty())
			{
				tokens.push_back(current);
				current.clear();
			}
		}
		else
		{
			current += c;
		}
	}
	if (!current.empty()) tokens.push_back(current);
	return tokens;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config-file>" << std::endl;
		return 1;
	}

	std::vector<Server*> servers;
	try
	{
		std::string content = readFile(argv[1]);
		std::vector<std::string> tokens = tokenize(content);

		ConfigParser parser(tokens);
		std::vector<ConfigBlock> blocks = parser.parse();

		for (size_t i = 0; i < blocks.size(); ++i)
		{
			if (blocks[i].name == "server")
			{
				CFG_Server cfg_server = buildCfgServer(blocks[i]);
				servers.push_back(new Server(cfg_server));
			}
		}

		if (servers.empty())
		{
			throw std::runtime_error("No server blocks found in config file: "
									 + std::string(argv[1]));
		}

		for (size_t i = 0; i < servers[0]->get_route().size(); i++)
		{
			std::cout << servers[0]->get_route()[i] << std::endl;
		}

		Core core(servers);
		core.core_loop();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		for (size_t i = 0; i < servers.size(); ++i) delete servers[i];
		return 1;
	}

	for (size_t i = 0; i < servers.size(); ++i) delete servers[i];
	return 0;
}
