#include "HttpUtils.hpp"

#include <vector>

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

static int hex_value(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/**
 * @brief Декодирует %XX в пути запроса (RFC 3986 §2.1).
 * @return false, если после '%' не две hex-цифры или получился NUL.
 *
 * NUL запрещаем: дальше путь уходит в stat()/open() как char*, и всё
 * после \0 было бы молча отброшено (тот же приём, что и с заголовками).
 */
bool percent_decode(const std::string& in, std::string& out)
{
	out.clear();
	for (std::string::size_type i = 0; i < in.size(); ++i)
	{
		if (in[i] != '%')
		{
			out += in[i];
			continue;
		}
		if (i + 2 >= in.size()) return false;
		int hi = hex_value(in[i + 1]);
		int lo = hex_value(in[i + 2]);
		if (hi < 0 || lo < 0) return false;
		char c = static_cast<char>(hi * 16 + lo);
		if (c == '\0') return false;
		out += c;
		i += 2;
	}
	return true;
}

/**
 * @brief Декодирует и нормализует путь запроса: убирает "." и "..".
 * @param raw Путь из стартовой строки (без query string), например
 *            "/list/../a%20b.txt"
 * @param out Результат, например "/a b.txt"
 * @return false — путь некорректный или пытается выйти выше корня "/".
 *
 * Сначала декодируем, потом разбираем сегменты: иначе "%2e%2e" прошёл бы
 * проверку как обычное имя и превратился бы в ".." уже после неё.
 * Алгоритм — remove_dot_segments из RFC 3986 §5.2.4, но выход выше корня
 * не «обрезаем», а отклоняем (400): такой запрос — почти всегда атака.
 */
bool normalize_path(const std::string& raw, std::string& out)
{
	std::string decoded;
	if (raw.empty() || raw[0] != '/') return false;
	if (!percent_decode(raw, decoded)) return false;

	std::vector<std::string> segments;
	std::string::size_type start = 1;
	while (start <= decoded.size())
	{
		std::string::size_type slash = decoded.find('/', start);
		if (slash == std::string::npos) slash = decoded.size();
		std::string seg = decoded.substr(start, slash - start);
		if (seg == "..")
		{
			if (segments.empty()) return false;	 // выше корня
			segments.pop_back();
		}
		else if (!seg.empty() && seg != ".")
		{
			segments.push_back(seg);
		}
		start = slash + 1;
	}

	out = "/";
	for (std::vector<std::string>::size_type i = 0; i < segments.size(); ++i)
	{
		if (i > 0) out += "/";
		out += segments[i];
	}
	// Сохраняем завершающий "/": "/list/" и "/list" — разные запросы
	// (директория и файл), это нужно для autoindex и index.
	bool trailing = decoded[decoded.size() - 1] == '/'
					|| (decoded.size() >= 2
						&& decoded.compare(decoded.size() - 2, 2, "/.") == 0)
					|| (decoded.size() >= 3
						&& decoded.compare(decoded.size() - 3, 3, "/..") == 0);
	if (trailing && out != "/") out += "/";
	return true;
}
