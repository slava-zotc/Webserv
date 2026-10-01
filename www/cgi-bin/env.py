#!/usr/bin/env python3
# Показывает, что сервер передал скрипту: переменные окружения,
# argv и рабочую директорию (проверка chdir).
# Пример: curl "http://localhost:8080/cgi-bin/env.py?name=slava&x=1"
import os
import sys

lines = []
lines.append("cwd  = " + os.getcwd())
lines.append("argv = " + repr(sys.argv))
lines.append("")
for key in sorted(os.environ):
    lines.append(key + "=" + os.environ[key])

sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("\n".join(lines) + "\n")