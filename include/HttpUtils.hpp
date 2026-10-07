#ifndef HTTP_UTILS_HPP
#define HTTP_UTILS_HPP

#include <string>

bool parse_header_line(const std::string& line, std::string& key, std::string& value);
bool is_tchar(char c);
bool is_token(const std::string& str);
bool has_forbidden_chars(const std::string& value);
#endif // HTTP_UTILS_HPP