#include "ConfigParser.hpp"

#include <stdexcept>

static bool isSpecial(const std::string& tok) {
    return tok == "{" || tok == "}" || tok == ";";
}

// Какие директивы разрешены в каком блоке. Неизвестная директива - ошибка:
// раньше опечатка вроде "autoindx on;" молча игнорировалась.
static bool isKnownDirective(const std::string& block, const std::string& key) {
    if (block == "server")
        return key == "listen" || key == "root" || key == "error_page"
            || key == "client_max_body_size";
    return key == "root" || key == "allowed_methods" || key == "autoindex"
        || key == "index" || key == "upload_enabled" || key == "upload_dir"
        || key == "return" || key == "cgi";
}

// На верхнем уровне конфига допустимы только блоки server.
std::vector<ConfigBlock> ConfigParser::parse() {
    std::vector<ConfigBlock> blocks;
    while (pos < tokens.size()) {
        if (peek() != "server")
            throw std::runtime_error("Unexpected '" + peek()
                                     + "' at top level (expected 'server')");
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
    block.name = advance();              // "server" или "location"

    // Аргумент перед '{': у location ровно один путь, у server - ничего.
    while (peek() != "{") {
        if (isSpecial(peek()))
            throw std::runtime_error("Unexpected '" + peek() + "' after '"
                                     + block.name + "' (expected '{')");
        block.arg += (block.arg.empty() ? "" : " ") + advance();
    }
    advance(); // consume '{'
    if (block.name == "server" && !block.arg.empty())
        throw std::runtime_error("server block takes no arguments: " + block.arg);
    if (block.name == "location"
        && (block.arg.empty() || block.arg[0] != '/'
            || block.arg.find(' ') != std::string::npos))
        throw std::runtime_error("location needs one path starting with '/': '"
                                 + block.arg + "'");

    while (peek() != "}") {
        if (isBlockKeyword(peek())) {
            // Вложенность только server -> location (location в location
            // и server в server раньше молча игнорировались).
            if (block.name != "server" || peek() != "location")
                throw std::runtime_error("'" + peek()
                                         + "' block is not allowed inside "
                                         + block.name);
            block.children.push_back(parseBlock());
        } else {
            parseDirective(block);
        }
    }
        advance(); // consume '}'
        return block;
}

void ConfigParser::parseDirective(ConfigBlock& block) {
    std::string key = advance();
    if (isSpecial(key))
        throw std::runtime_error("Unexpected '" + key + "' in " + block.name);
    if (!isKnownDirective(block.name, key))
        throw std::runtime_error("Unknown directive '" + key + "' in "
                                 + block.name
                                 + (block.arg.empty() ? "" : " " + block.arg));
    std::vector<std::string> values;
    while (peek() != ";") {
        // "listen 8080 }" - забыли ';', иначе директива съела бы конец блока
        if (isSpecial(peek()))
            throw std::runtime_error("Missing ';' after directive '" + key + "'");
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
