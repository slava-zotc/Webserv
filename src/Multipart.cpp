#include "Multipart.hpp"

#include <cctype>
#include <map>

#include "HttpUtils.hpp"

/*
 * Разбор multipart/form-data.
 *
 * Тело выглядит так (boundary берётся из заголовка Content-Type):
 *
 *   --BOUNDARY\r\n
 *   Content-Disposition: form-data; name="file"; filename="cat.png"\r\n
 *   Content-Type: image/png\r\n
 *   \r\n
 *   <байты файла>\r\n
 *   --BOUNDARY--\r\n
 *
 * RFC 2046 §5.1.1: "\r\n" перед "--BOUNDARY" принадлежит разделителю,
 * а не данным, поэтому данные части кончаются прямо перед "\r\n--BOUNDARY".
 * RFC 7578 §4.2: в каждой части обязателен Content-Disposition: form-data.
 *
 * Данные бинарные (в них бывают '\0'), поэтому только std::string::find
 * и substr: они работают с байтами и длиной, а не с C-строками.
 */

static std::string to_lower(std::string s)
{
	for (std::string::size_type i = 0; i < s.size(); ++i)
		s[i] = std::tolower(static_cast<unsigned char>(s[i]));
	return s;
}

static std::string trim(const std::string& s)
{
	std::string::size_type b = s.find_first_not_of(" \t");
	if (b == std::string::npos) return "";
	std::string::size_type e = s.find_last_not_of(" \t");
	return s.substr(b, e - b + 1);
}

/**
 * @brief Снимает кавычки: "cat.png" -> cat.png
 *
 * Обратную косую черту НЕ считаем экранированием, хотя в общем HTTP
 * quoted-string это так (RFC 9110 §5.6.4). Браузеры в filename её не
 * экранируют: Windows шлёт filename="C:\dir\a.txt" как есть, а '"'
 * кодирует как %22 (RFC 7578 §4.2, HTML living standard). Boundary
 * обратной косой содержать не может (RFC 2046 §5.1.1).
 */
static std::string unquote(const std::string& v)
{
	if (v.size() < 2 || v[0] != '"' || v[v.size() - 1] != '"') return v;
	return v.substr(1, v.size() - 2);
}

/**
 * @brief Делит "значение; k1=v1; k2="v;2"" на значение и параметры
 * @param s      Значение заголовка
 * @param first  [out] то, что до первой ';' (тип или form-data)
 * @param params [out] параметры: имя в нижнем регистре -> значение
 *
 * ';' внутри кавычек - часть значения, а не разделитель:
 * filename="a;b.txt" должен остаться одним параметром.
 */
static void split_params(const std::string& s, std::string& first,
						 std::map<std::string, std::string>& params)
{
	std::vector<std::string> segments;
	std::string current;
	bool in_quotes = false;

	for (std::string::size_type i = 0; i < s.size(); ++i)
	{
		char c = s[i];
		if (c == '"') in_quotes = !in_quotes;
		if (c == ';' && !in_quotes)
		{
			segments.push_back(current);
			current.clear();
			continue;
		}
		current += c;
	}
	segments.push_back(current);

	first = trim(segments[0]);
	for (std::vector<std::string>::size_type i = 1; i < segments.size(); ++i)
	{
		std::string seg = trim(segments[i]);
		std::string::size_type eq = seg.find('=');
		if (eq == std::string::npos) continue;
		std::string key = to_lower(trim(seg.substr(0, eq)));
		params[key] = unquote(trim(seg.substr(eq + 1)));
	}
}

/**
 * @brief Достаёт boundary из Content-Type: multipart/form-data
 * @return false, если это не multipart/form-data или boundary некорректен
 *
 * Тип и имя параметра сравниваются без учёта регистра (RFC 9110 §8.3.1).
 * Длина boundary 1..70 символов (RFC 2046 §5.1.1).
 */
bool get_multipart_boundary(const std::string& content_type,
							std::string& boundary)
{
	std::string media_type;
	std::map<std::string, std::string> params;
	split_params(content_type, media_type, params);

	if (to_lower(media_type) != "multipart/form-data") return false;
	std::map<std::string, std::string>::const_iterator it =
		params.find("boundary");
	if (it == params.end() || it->second.empty() || it->second.size() > 70)
		return false;
	boundary = it->second;
	return true;
}

/**
 * @brief Разбирает заголовки одной части (блок до пустой строки)
 * @return false, если строка заголовка битая или нет
 *         Content-Disposition: form-data; name="..."
 */
static bool parse_part_headers(const std::string& block, MultipartPart& part)
{
	bool has_disposition = false;
	std::string::size_type pos = 0;

	while (pos <= block.size())
	{
		std::string::size_type eol = block.find("\r\n", pos);
		if (eol == std::string::npos) eol = block.size();
		std::string line = block.substr(pos, eol - pos);
		pos = eol + 2;

		std::string key;
		std::string value;
		if (!parse_header_line(line, key, value)) return false;

		if (key == "content-disposition")
		{
			std::string kind;
			std::map<std::string, std::string> params;
			split_params(value, kind, params);
			if (to_lower(kind) != "form-data" || !params.count("name"))
				return false;
			part.name = params["name"];
			if (params.count("filename")) part.filename = params["filename"];
			has_disposition = true;
		}
		else if (key == "content-type")
		{
			part.content_type = value;
		}
	}
	return has_disposition;
}

/**
 * @brief Разбирает тело multipart/form-data на части
 * @param body     Тело запроса целиком (уже без chunked)
 * @param boundary Значение из Content-Type
 * @param parts    [out] части в порядке следования
 * @return false, если формат нарушен (сервер ответит 400)
 */
bool parse_multipart(const std::string& body, const std::string& boundary,
					 std::vector<MultipartPart>& parts)
{
	const std::string delim = "--" + boundary;
	const std::string next_delim = "\r\n" + delim;

	// Первый разделитель: в самом начале тела или после преамбулы
	// (текст до него по RFC 2046 §5.1.1 игнорируется).
	std::string::size_type pos;
	if (body.compare(0, delim.size(), delim) == 0)
		pos = delim.size();
	else
	{
		pos = body.find(next_delim);
		if (pos == std::string::npos) return false;
		pos += next_delim.size();
	}

	while (true)
	{
		// После разделителя допускаются пробелы (transport padding).
		while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t'))
			++pos;
		// "--BOUNDARY--" - конец; всё после него (эпилог) игнорируем.
		if (body.compare(pos, 2, "--") == 0) return true;
		if (body.compare(pos, 2, "\r\n") != 0) return false;
		pos += 2;

		// Заголовки части - до пустой строки. Если пустая строка сразу,
		// заголовков нет, а без Content-Disposition часть некорректна.
		std::string::size_type head_end = body.find("\r\n\r\n", pos);
		if (head_end == std::string::npos || head_end == pos) return false;

		MultipartPart part;
		if (!parse_part_headers(body.substr(pos, head_end - pos), part))
			return false;

		// Данные - до следующего "\r\n--BOUNDARY".
		std::string::size_type data_start = head_end + 4;
		std::string::size_type data_end = body.find(next_delim, data_start);
		if (data_end == std::string::npos) return false;  // нет конца части

		part.data = body.substr(data_start, data_end - data_start);
		parts.push_back(part);
		pos = data_end + next_delim.size();
	}
}

/**
 * @brief Безопасное имя файла из filename клиента
 * @return Имя без каталогов или "", если имя использовать нельзя
 *
 * RFC 7578 §4.2: filename нельзя использовать вслепую. "../../etc/passwd"
 * или "/home/x/.bashrc" - классический path traversal. Оставляем только
 * последний компонент (и '/', и '\\' - Windows-браузеры шлют C:\dir\a.txt),
 * а пустое имя, "." и ".." и управляющие символы отвергаем.
 */
std::string sanitize_filename(const std::string& raw)
{
	std::string::size_type slash = raw.find_last_of("/\\");
	std::string name = (slash == std::string::npos) ? raw
													: raw.substr(slash + 1);
	if (name.empty() || name == "." || name == "..") return "";
	for (std::string::size_type i = 0; i < name.size(); ++i)
	{
		unsigned char c = static_cast<unsigned char>(name[i]);
		if (c < 0x20 || c == 0x7f) return "";
	}
	return name;
}
