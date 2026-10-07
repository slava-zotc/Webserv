#include "HttpUtils.hpp"

#include <algorithm>




bool has_forbidden_chars(const std::string& value)
{
	return value.find('\r') != std::string::npos
		   || value.find('\n') != std::string::npos
		   || value.find('\0') != std::string::npos;
}

bool is_tchar(char c)
{
	return (c >= 33 && c <= 126)
		   && (c != '(' && c != ')' && c != '<' && c != '>' && c != '@'
			   && c != ',' && c != ';' && c != ':' && c != '\\' && c != '"'
			   && c != '/' && c != '[' && c != ']' && c != '{' && c != '}'
			   && c != '=' && c != '?');
}

bool is_token(const std::string& str)
{
	if (str.empty())
	{
		return false;
	}
	for (size_t i = 0; i < str.size(); ++i)
	{
		char c = str[i];
		if (!is_tchar(c))
		{
			return false;
		}
	}
	return true;
}

bool parse_header_line(const std::string& line, std::string& key,
					   std::string& value)
{
	size_t pos = line.find(':');
	
    if (pos == std::string::npos) return false;

	key = line.substr(0, pos);
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);
	value = line.substr(pos + 1);
	size_t first_not_space = value.find_first_not_of("\t ");
	size_t last_not_space = value.find_last_not_of("\t ");
	
    if (first_not_space == std::string::npos)
	{
		value = "";
	}
	else
	{
		value.erase(last_not_space + 1);
		value = value.substr(first_not_space);
	}
	
    if (!is_token(key)) return false;

	return true;
}