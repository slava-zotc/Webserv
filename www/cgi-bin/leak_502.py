#!/usr/bin/env python3
# Скрипт успевает выдать нормальные заголовки, а потом ломает ответ (два Status).
# Ожидается: 502, и НИ ОДНОГО заголовка от скрипта (X-Leak) в ответе:
# 502 — это ответ сервера, а не сломанного скрипта.
import sys
sys.stdout.write("X-Leak: yes\nContent-Type: text/plain\nStatus: 200 OK\nStatus: 404 Not Found\n\nbody\n")
