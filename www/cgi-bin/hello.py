#!/usr/bin/env python3
# Шаг 1: самый простой CGI. Ожидается 200 и HTML-страница.
import sys
 
body = "<html><body><h1>Hello from CGI</h1></body></html>"
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)
 
