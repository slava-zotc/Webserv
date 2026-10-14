#include "ConfigParser.hpp"

#include <stdexcept>

std::vector<ConfigBlock> ConfigParser::parse() {
    std::vector<ConfigBlock> blocks;
    while (pos < tokens.size()) {
        blocks.push_back(parseBlock());
    }
    return blocks;
}

// Конфиг оборвался посреди блока или директивы (нет '}' или ';').
// Без этой проверки tokens.at() бросал бы непонятное vector::_M_range_check.
const std::string& ConfigParser::peek() {
    if (pos >= tokens.size())
        throw std::runtime_error("Unexpected end of config file "
                                 "(missing '}' or ';'?)");
    return tokens[pos];
}

std::string ConfigParser::advance() {
    std::string tok = peek();
    ++pos;
    return tok;
}

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

    // error_page - единственная директива, которую можно повторять:
    // по строке на каждый код (или группу кодов).
    if (key == "error_page") {
        block.error_pages.push_back(values);
        return;
    }
    // Любой другой повтор - ошибка конфига. Раньше второе вхождение молча
    // перезаписывало первое, и такую опечатку было трудно заметить.
    if (block.directives.count(key)) {
        throw std::runtime_error("Duplicate directive '" + key + "' in "
                                 + block.name
                                 + (block.arg.empty() ? "" : " " + block.arg));
    }
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
