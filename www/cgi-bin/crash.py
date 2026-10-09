#!/usr/bin/env python3
# Скрипт успел вывести корректные заголовки, но завершился с кодом 1.
# Ожидается: 502 (ненулевой код выхода), а не 200 с этим телом.
import sys
sys.stdout.write("Content-Type: text/plain\n\nthis output must NOT reach the client\n")
sys.stdout.flush()
sys.exit(1)
