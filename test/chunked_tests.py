#!/usr/bin/env python3
"""
Chunked-запросы (RFC 9112 §7.1). Сервер должен быть запущен.
Запуск: python3 test/chunked_tests.py [port] [max_body_size]
Нужны: /cgi-bin/echo_body.py (CGI) и /uploads (upload_enabled, GET/POST/DELETE).
"""
import socket
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 1048576
results = []


def send(parts, delay=0.0):
    s = socket.create_connection(("localhost", PORT), timeout=10)
    data = b""
    try:
        for p in parts:
            s.sendall(p)
            if delay:
                time.sleep(delay)
    except (BrokenPipeError, ConnectionResetError):
        pass  # сервер ответил ошибкой и закрыл соединение, не дочитав тело
    try:
        while True:
            c = s.recv(65536)
            if not c:
                break
            data += c
    except (socket.timeout, ConnectionResetError):
        pass
    s.close()
    head, _, body = data.partition(b"\r\n\r\n")
    first = head.split(b"\r\n", 1)[0].split()
    return (int(first[1]) if len(first) > 1 else -1), body


def head(path, extra=b""):
    return (b"POST " + path.encode() + b" HTTP/1.1\r\nHost: x\r\n"
            b"Transfer-Encoding: chunked\r\nContent-Type: text/plain\r\n" + extra + b"\r\n")


def chunk(data, ext=b""):
    return b"%x" % len(data) + ext + b"\r\n" + data + b"\r\n"


def check(name, ok, note):
    results.append(ok)
    print("%s %-46s %s" % ("OK  " if ok else "FAIL", name, note))


# 1. Несколько чанков -> CGI получает склеенное тело
code, body = send([head("/cgi-bin/echo_body.py") + chunk(b"Hello, ") + chunk(b"chunked ") + chunk(b"world") + b"0\r\n\r\n"])
check("3 чанка -> CGI получает целое тело", code == 200 and body == b"Hello, chunked world", "код %d тело %r" % (code, body[:40]))

# 2. Расширения чанков и трейлеры игнорируются, HEX в верхнем регистре
data = b"Z" * 300
code, body = send([head("/cgi-bin/echo_body.py") + b"12C;name=val\r\n" + data + b"\r\n" + b"0\r\nX-Trailer: yes\r\n\r\n"])
check("chunk-ext, HEX 12C, трейлер", code == 200 and body == data, "код %d, %d байт" % (code, len(body)))

# 3. По одному байту за send — автомат должен собирать по кусочкам
raw = head("/cgi-bin/echo_body.py") + chunk(b"abc") + chunk(b"defgh") + b"0\r\n\r\n"
code, body = send([raw[i:i + 1] for i in range(len(raw))], delay=0.002)
check("запрос по 1 байту", code == 200 and body == b"abcdefgh", "код %d тело %r" % (code, body))

# 4. Большое тело (400 КБ, чанки по 16 КБ) -> CGI
big = b"Q" * 400000
parts = [head("/cgi-bin/echo_body.py")] + [chunk(big[i:i + 16384]) for i in range(0, len(big), 16384)] + [b"0\r\n\r\n"]
code, body = send(parts)
check("400 КБ чанками по 16 КБ", code == 200 and body == big, "код %d, %d байт" % (code, len(body)))

# 5. Chunked-загрузка файла и чтение обратно
code, _ = send([head("/uploads/chunked.txt") + chunk(b"uploaded ") + chunk(b"in chunks") + b"0\r\n\r\n"])
s = socket.create_connection(("localhost", PORT), timeout=5)
s.sendall(b"GET /uploads/chunked.txt HTTP/1.1\r\nHost: x\r\n\r\n")
got = b""
while True:
    c = s.recv(65536)
    if not c:
        break
    got += c
s.close()
check("chunked upload и GET обратно", code == 201 and got.endswith(b"uploaded in chunks"), "POST %d" % code)
send([b"DELETE /uploads/chunked.txt HTTP/1.1\r\nHost: x\r\n\r\n"])

# 6. Ошибки
code, _ = send([head("/cgi-bin/echo_body.py") + b"zz\r\nabc\r\n0\r\n\r\n"])
check("не HEX размер -> 400", code == 400, "код %d" % code)
code, _ = send([head("/cgi-bin/echo_body.py") + b"3\r\nabcdef\r\n0\r\n\r\n"])
check("данных больше размера -> 400", code == 400, "код %d" % code)
code, _ = send([b"POST /cgi-bin/echo_body.py HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n5\r\nhello\r\n0\r\n\r\n"])
check("TE + Content-Length -> 400", code == 400, "код %d (RFC 9112 §6.3 п.3)" % code)
code, _ = send([b"POST /cgi-bin/echo_body.py HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: gzip\r\n\r\n"])
check("TE: gzip -> 501", code == 501, "код %d (RFC 9112 §6.1)" % code)
over = b"A" * 4096
parts = [head("/cgi-bin/echo_body.py")] + [chunk(over)] * (LIMIT // 4096 + 2) + [b"0\r\n\r\n"]
code, _ = send(parts)
check("тело больше client_max_body_size -> 413", code == 413, "код %d" % code)

# 7. curl-стиль: один чанк + сервер жив
code, body = send([head("/cgi-bin/echo_body.py") + chunk(b"x") + b"0\r\n\r\n"])
check("сервер жив", code == 200, "код %d" % code)

print("\n%d/%d" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
