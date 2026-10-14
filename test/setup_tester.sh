#!/bin/bash
# Готовит всё для старого тестера 42 (./tester http://localhost:8080):
#   - каталог YoupiBanane с файлами из инструкции тестера
#   - conf/tester.conf с абсолютным путём к cgi_tester
#
# Запуск из корня проекта:
#   bash test/setup_tester.sh [путь/к/cgi_tester]
#   ./webserv conf/tester.conf
#   ./tester http://localhost:8080        (в другом терминале)
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

CGI="${1:-$ROOT/cgi_tester}"
case "$CGI" in /*) ;; *) CGI="$ROOT/${CGI#./}" ;; esac
if [ ! -f "$CGI" ]; then
    echo "Не найден cgi_tester: $CGI"
    echo "Скачай ubuntu_cgi_tester со страницы проекта и передай путь к нему."
    exit 1
fi
chmod +x "$CGI"

mkdir -p YoupiBanane/nop YoupiBanane/Yeah YoupiBanane/post_body
echo "youpi.bad_extension"   > YoupiBanane/youpi.bad_extension
echo "youpi.bla"             > YoupiBanane/youpi.bla
echo "nop/youpi.bad_extension" > YoupiBanane/nop/youpi.bad_extension
echo "nop/other.pouic"       > YoupiBanane/nop/other.pouic
echo "Yeah/not_happy.bad_extension" > YoupiBanane/Yeah/not_happy.bad_extension

cat > conf/tester.conf <<CONF
# Конфиг для старого тестера 42. Сгенерирован test/setup_tester.sh.
server {
	listen 8080;
	root ./www;
	client_max_body_size 200000000;    # тестер шлёт на .bla до 100 МБ

	# / - только GET (HEAD -> 405, POST -> 405)
	location / {
		allowed_methods GET;
		index index.html;
	}

	# /post_body - POST с лимитом 100 байт (101 -> 413)
	location /post_body {
		root ./YoupiBanane/post_body;
		allowed_methods POST;
		client_max_body_size 100;
		upload_enabled on;
		upload_dir ./YoupiBanane/post_body;
	}

	# /directory/ - GET из YoupiBanane, index youpi.bad_extension,
	# файлы .bla по POST идут в cgi_tester
	location /directory {
		root ./YoupiBanane;
		allowed_methods GET POST;
		index youpi.bad_extension;
		cgi .bla $CGI;
		cgi_methods POST;                  # GET на .bla - обычный файл
		cgi_require_file off;              # тестер шлёт POST и на несуществующий youpla.bla
		cgi_timeout 120;                   # 20 cgi_tester по 100 МБ параллельно думают долго
	}
}
CONF
echo "Готово: YoupiBanane/ и conf/tester.conf (cgi: $CGI)"
