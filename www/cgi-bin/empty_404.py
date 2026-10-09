#!/usr/bin/env python3
# Скрипт вернул 404 БЕЗ тела. Сервер вправе подставить свою страницу ошибки,
# но в ответе должен остаться ровно ОДИН Content-Type (а не content-type от
# скрипта + Content-Type от страницы ошибки).
import sys
sys.stdout.write("Status: 404 Not Found\nContent-Type: text/plain\n\n")
