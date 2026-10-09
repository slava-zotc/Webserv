#!/usr/bin/env python3
# Скрипт врёт про длину. Ожидается: 200, ровно ОДИН Content-Length = 5.
import sys
sys.stdout.write("Content-Type: text/plain\nContent-Length: 999\n\nshort")
