#!/usr/bin/env python3
# Возвращает тело запроса как есть. Для POST в CGI (пункт 5).
# Ожидается: тело ответа == телу запроса, Content-Type как у запроса.
import os, sys
n = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
data = sys.stdin.buffer.read(n) if n > 0 else b""
ctype = os.environ.get("CONTENT_TYPE", "application/octet-stream")
sys.stdout.buffer.write(("Content-Type: " + ctype + "\r\n\r\n").encode())
sys.stdout.buffer.write(data)
