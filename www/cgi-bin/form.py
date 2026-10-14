#!/usr/bin/env python3
# Форма с главной страницы. GET: данные в QUERY_STRING,
# POST: тело на stdin, длина в CONTENT_LENGTH (RFC 3875 §4.2).
import os
import sys
import html
from urllib.parse import parse_qs

method = os.environ.get("REQUEST_METHOD", "")
if method == "POST":
    length = int(os.environ.get("CONTENT_LENGTH", "0") or 0)
    raw = sys.stdin.read(length)
else:
    raw = os.environ.get("QUERY_STRING", "")

fields = parse_qs(raw)
name = fields.get("name", ["незнакомец"])[0]

body = ("<!DOCTYPE html><html lang=\"ru\"><head><meta charset=\"UTF-8\">"
        "<title>CGI form</title></head>"
        "<body style=\"font-family: monospace; background:#1e1e1e; color:#d4d4d4; padding:2rem\">"
        "<h1 style=\"color:#4ec9b0\">Привет, " + html.escape(name) + "!</h1>"
        "<p>Метод: " + html.escape(method) + "</p>"
        "<p>Сырые данные: <code>" + html.escape(raw) + "</code></p>"
        "<p><a style=\"color:#4ec9b0\" href=\"/\">назад</a></p></body></html>")

sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n\r\n")
sys.stdout.write(body)
