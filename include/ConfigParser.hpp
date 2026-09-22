#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <map>
#include <vector>
#include <string>

struct ConfigBlock {
	std::string name;                        // e.g. "server", "location"
	std::string arg;                         // e.g. "/kapouet" for location
	std::map<std::string, std::vector<std::string> > directives; // key -> values
	std::vector<ConfigBlock> children;       // nested blocks (server -> location)
};

class ConfigParser {
public:
	ConfigParser(const std::vector<std::string>& toks);
	~ConfigParser();

	std::vector<ConfigBlock> parse();

private:
	const std::vector<std::string>& tokens;
	size_t pos;

	const std::string& peek();
	std::string advance();
	ConfigBlock parseBlock();
	void parseDirective(ConfigBlock& block);
	bool isBlockKeyword(const std::string& tok);

	ConfigParser();
	ConfigParser(const ConfigParser & other);
	ConfigParser& operator=(const ConfigParser& other);
};

#endif