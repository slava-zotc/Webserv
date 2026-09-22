#include "ConfigParser.hpp"

std::vector<ConfigBlock> ConfigParser::parse() {
    std::vector<ConfigBlock> blocks;
    while (pos < tokens.size()) {
        blocks.push_back(parseBlock());
    }
    return blocks;
}

const std::string& ConfigParser::peek() { return tokens.at(pos); }

std::string ConfigParser::advance() { return tokens.at(pos++); }

ConfigBlock ConfigParser::parseBlock() {
    ConfigBlock block;
    block.name = advance();              // e.g. "server" or "location"

    // optional argument before '{' (e.g. location's path, or "/kapouet")
    while (peek() != "{") {
        block.arg += (block.arg.empty() ? "" : " ") + advance();
    }
    advance(); // consume '{'

    while (peek() != "}") {
        if (isBlockKeyword(peek())) {
            block.children.push_back(parseBlock()); // recurse for nested block
        } else {
            parseDirective(block);
        }
    }
        advance(); // consume '}'
        return block;
}

void ConfigParser::parseDirective(ConfigBlock& block) {
    std::string key = advance();
    std::vector<std::string> values;
    while (peek() != ";") {
        values.push_back(advance());
    }
    advance(); // consume ';'
    block.directives[key] = values;
}

bool ConfigParser::isBlockKeyword(const std::string& tok) {
    return tok == "server" || tok == "location";
}

ConfigParser::ConfigParser(const std::vector<std::string>& toks) : tokens(toks), pos(0) {}

// ConfigParser::ConfigParser() : tokens(static_cast<const std::vector<std::string>&>(new std::vector<std::string>())) {}

ConfigParser::ConfigParser(const ConfigParser& other) : tokens(other.tokens), pos(other.pos) {}

ConfigParser& ConfigParser::operator=(const ConfigParser& other) {
    (void)other;
	return *this;
}

ConfigParser::~ConfigParser() {}
