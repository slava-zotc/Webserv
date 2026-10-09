#!/usr/bin/env python3
# Свой заголовок должен дойти до клиента, Status — нет.
# Ожидается: 201, заголовок X-Custom: yes, без заголовка Status.
import sys
sys.stdout.write("Status: 201 Created\r\nContent-Type: text/plain\r\nX-Custom: yes\r\n\r\ncreated\n")
