#!/usr/bin/env python3
# Скрипт сам задаёт код ответа через CGI-заголовок Status.
# Ожидается: клиент получает "HTTP/1.1 404 Not Found", а не 200.
import sys

sys.stdout.write("Status: 404 Not Found\r\n")
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("custom 404 from CGI\n")