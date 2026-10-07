#!/usr/bin/env python3
"""
Демонстрация: почему CR, LF и NUL запрещены в значениях заголовков
(RFC 9110 §5.5). Сервер не нужен — скрипт сравнивает, как разные
программы видят одни и те же байты.

Запуск: python3 test/demo_bad_header_chars.py
"""
import ctypes


def parse_strict(raw):
    """Как твой webserv: строки режутся только по \\r\\n."""
    head, _, body = raw.partition(b"\r\n\r\n")
    lines = head.split(b"\r\n")[1:]          # [0] — стартовая строка
    return [l.split(b":", 1) for l in lines], body


def parse_lenient(raw):
    """Как многие прокси: одиночный \\n тоже конец строки
    (RFC 9112 §2.2 это разрешает)."""
    head, _, body = raw.partition(b"\r\n\r\n")
    lines = [l.rstrip(b"\r") for l in head.split(b"\n")][1:]
    return [l.split(b":", 1) for l in lines], body


def show(title, headers, body):
    print(f"  {title}:")
    for kv in headers:
        name = kv[0].strip().decode()
        value = kv[1].strip() if len(kv) > 1 else b""
        print(f"    {name!r:18} = {value!r}")
    cl = next((int(v.strip()) for k, *v in [(h[0], *h[1:]) for h in headers]
               if k.strip().lower() == b"content-length" for v in v), 0)
    print(f"    -> тело запроса: {body[:cl]!r}")
    print(f"    -> остаток, который будет принят за СЛЕДУЮЩИЙ запрос: {body[cl:]!r}")


print("=" * 70)
print("1. LF внутри значения -> request smuggling (RFC 9112 §11.2)")
print("=" * 70)
raw = (b"POST /upload HTTP/1.1\r\n"
       b"Host: x\r\n"
       b"X-Note: hi\nContent-Length: 4\r\n"     # <-- голый \n в значении
       b"\r\n"
       b"AAAAGET /admin HTTP/1.1\r\nHost: x\r\n\r\n")
print("Отправлено:", raw, "\n")
show("Строгий парсер (webserv, режет по \\r\\n)", *parse_strict(raw))
print()
show("Мягкий парсер (прокси, режет и по \\n)", *parse_lenient(raw))
print("""
  Итог: прокси считает, что тело — 4 байта 'AAAA', а дальше идёт
  ВТОРОЙ запрос 'GET /admin'. Твой сервер Content-Length не видит,
  тела нет, а все байты — мусор. Две программы на одном соединении
  расходятся в том, где кончается запрос: так протаскивают запрос
  мимо проверок прокси. Ответ 400 на такую строку закрывает это.
""")

print("=" * 70)
print("2. CR LF в значении -> response splitting через CGI (RFC 9112 §11.1)")
print("=" * 70)
value = "ru\r\nSet-Cookie: session=attacker"
print("Значение заголовка Accept-Language:", repr(value))
print("Наивный CGI-скрипт делает: print('Content-Language: ' + os.environ['HTTP_ACCEPT_LANGUAGE'])")
cgi_out = "Content-Type: text/html\r\nContent-Language: " + value + "\r\n\r\n<h1>hi</h1>"
print("Вывод скрипта, построчно:")
for line in cgi_out.split("\r\n"):
    print("   ", repr(line))
print("""
  Итог: в ответе появился заголовок Set-Cookie, который никто не писал:
  атакующий выставил жертве свою сессию. Сервер, отклонивший \\r\\n
  в запросе, до скрипта такое значение просто не довёз бы.
""")

print("=" * 70)
print("3. NUL -> std::string и char* видят разные строки")
print("=" * 70)
value = b"user\x00admin"
print("std::string хранит  :", repr(value), f"(длина {len(value)})")
print("execve / getenv видят:", repr(ctypes.c_char_p(value).value),
      f"(длина {len(ctypes.c_char_p(value).value)})")
print("""
  Итог: сервер проверял одно значение, CGI получил другое (обрезанное
  на \\0). Любая проверка, сделанная по std::string, обходится.
""")
