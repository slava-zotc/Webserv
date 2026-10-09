#!/usr/bin/env python3
# Заголовки без пустой строки: нет границы заголовков и тела (RFC 3875 §6.2).
# Ожидается: 502.
import sys
sys.stdout.write("Content-Type: text/plain\n")
