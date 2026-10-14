#ifndef MULTIPART_HPP
#define MULTIPART_HPP

#include <string>
#include <vector>

/**
 * @brief Одна часть тела multipart/form-data (RFC 7578)
 *
 * Поле формы (name="comment") или файл (name="file"; filename="cat.png").
 */
struct MultipartPart
{
	std::string name;			///< имя поля формы
	std::string filename;		///< имя файла от клиента; пусто, если не файл
	std::string content_type;	///< Content-Type части (может быть пустым)
	std::string data;			///< содержимое, байты как есть
};

bool get_multipart_boundary(const std::string& content_type,
							std::string& boundary);
bool parse_multipart(const std::string& body, const std::string& boundary,
					 std::vector<MultipartPart>& parts);
std::string sanitize_filename(const std::string& raw);

#endif	// MULTIPART_HPP
