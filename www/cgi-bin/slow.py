#!/usr/bin/env python3
# Отвечает через 3 секунды. Пока он работает, сервер должен обслуживать других.
import sys, time
time.sleep(3)
sys.stdout.write("Content-Type: text/plain\n\nslow done\n")
