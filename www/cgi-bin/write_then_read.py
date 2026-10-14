#!/usr/bin/env python3
# Сначала пишет 200 КБ вывода, и только потом читает stdin.
# Если сервер не читает вывод и не пишет тело ОДНОВРЕМЕННО (через один poll),
# оба pipe заполнятся и всё зависнет (взаимная блокировка) -> 504 по таймауту.
# Ожидается: 200, в конце тела строка "got=<длина тела запроса>".
import os, sys
sys.stdout.write("Content-Type: text/plain\n\n")
sys.stdout.write("x" * 200000 + "\n")
sys.stdout.flush()
n = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
data = sys.stdin.buffer.read(n) if n > 0 else b""
sys.stdout.write("got=%d\n" % len(data))
