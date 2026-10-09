#!/usr/bin/env python3
# Скрипт убивает сам себя сигналом SIGKILL после вывода заголовков.
# WIFEXITED(status) == false. Ожидается: 502.
import os, signal, sys
sys.stdout.write("Content-Type: text/plain\n\npartial")
sys.stdout.flush()
os.kill(os.getpid(), signal.SIGKILL)
