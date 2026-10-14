#!/usr/bin/env python3
"""Тесты п. 13 (POST в каталог), п. 17 (окружение CGI), п. 18 (414/431).

Запуск из корня проекта:  python3 test/misc_tests.py
Сам запускает ./webserv conf/default.conf (порты 8080-8082 должны быть свободны).
"""
import os, socket, subprocess, sys, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
results = []

def check(name, ok, info=""):
    results.append(bool(ok))
    print(("OK   " if ok else "FAIL ") + name + ("" if ok else "  -> " + str(info)))

def raw(data, port=8080, read=True):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    try:
        s.sendall(data)
    except (BrokenPipeError, ConnectionResetError):
        pass
    out = b""
    while read:
        try:
            chunk = s.recv(65536)
        except (socket.timeout, ConnectionResetError):
            break
        if not chunk:
            break
        out += chunk
    s.close()
    return out

def status(resp):
    try:
        return int(resp.split(b" ", 2)[1])
    except Exception:
        return None

def header(resp, name):
    head = resp.split(b"\r\n\r\n", 1)[0].decode("latin-1")
    for l in head.split("\r\n")[1:]:
        k, _, v = l.partition(":")
        if k.lower() == name.lower():
            return v.strip()
    return None

def body(resp):
    return resp.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in resp else b""

srv = subprocess.Popen([os.path.join(ROOT, "webserv")], cwd=ROOT,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(0.5)
uploads = os.path.join(ROOT, "www", "uploads")
created = []
try:
    # ---------- п. 18: лимиты ----------
    r = raw(b"GET /" + b"a" * 9000 + b" HTTP/1.1\r\nHost: x\r\n\r\n")
    check("request-line 9000 байт -> 414", status(r) == 414, r[:60])
    r = raw(b"GET /" + b"a" * 9000)           # без \r\n вообще
    check("недописанная длинная строка -> 414 (буфер не растёт)", status(r) == 414, r[:60])
    r = raw(b"GET / HTTP/1.1\r\nHost: x\r\nX-Big: " + b"b" * 20000 + b"\r\n\r\n")
    check("заголовок 20 KiB -> 431", status(r) == 431, r[:60])
    hdrs = b"".join(b"X-H%d: %s\r\n" % (i, b"v" * 100) for i in range(200))
    r = raw(b"GET / HTTP/1.1\r\nHost: x\r\n" + hdrs + b"\r\n")
    check("200 заголовков по 100 байт -> 431", status(r) == 431, r[:60])
    hdrs = b"".join(b"X-H%d: %s\r\n" % (i, b"v" * 100) for i in range(50))
    r = raw(b"GET / HTTP/1.1\r\nHost: x\r\n" + hdrs + b"\r\n")
    check("50 заголовков по 100 байт -> 200", status(r) == 200, r[:60])
    r = raw(b"GET /" + b"a" * 7000 + b" HTTP/1.1\r\nHost: x\r\n\r\n")
    check("request-line 7000 байт -> 404 (не 414)", status(r) == 404, r[:60])

    # ---------- п. 13: POST в каталог ----------
    r = raw(b"POST /uploads/ HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nhello")
    loc = header(r, "Location")
    check("POST /uploads/ -> 201", status(r) == 201, r[:80])
    check("Location: /uploads/upload_...", bool(loc) and loc.startswith("/uploads/upload_"), loc)
    if loc:
        created.append(loc.split("/")[-1])
        g = raw(b"GET " + loc.encode() + b" HTTP/1.1\r\nHost: x\r\n\r\n")
        check("GET по Location отдаёт то же тело", status(g) == 200 and body(g) == b"hello", g[:80])
    r2 = raw(b"POST /uploads HTTP/1.1\r\nHost: x\r\nContent-Length: 2\r\n\r\nhi")
    loc2 = header(r2, "Location")
    check("POST /uploads (без /) -> 201, другое имя", status(r2) == 201 and loc2 and loc2 != loc, (loc, loc2))
    if loc2:
        created.append(loc2.split("/")[-1])
    r = raw(b"POST /uploads/named.txt HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\n\r\nabc")
    check("POST /uploads/named.txt -> 201, Location /uploads/named.txt",
          status(r) == 201 and header(r, "Location") == "/uploads/named.txt", r[:120])
    created.append("named.txt")
    r = raw(b"POST /uploads/no_dir/x.txt HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\n\r\nx")
    check("POST в несуществующий каталог -> 404", status(r) == 404, r[:60])
    r = raw(b"POST /uploads/no_dir/ HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\n\r\nx")
    check("POST /uploads/no_dir/ -> 404", status(r) == 404, r[:60])

    # ---------- п. 17: окружение CGI ----------
    r = raw(b"GET /cgi-bin/env.py/extra/path?q=1 HTTP/1.1\r\nHost: demo.local:8080\r\n\r\n")
    b = body(r).decode()
    env = dict(l.split("=", 1) for l in b.splitlines() if "=" in l and not l.startswith(("cwd", "argv")))
    check("env.py/extra/path -> 200", status(r) == 200, r[:80])
    check("SCRIPT_NAME=/cgi-bin/env.py", env.get("SCRIPT_NAME") == "/cgi-bin/env.py", env.get("SCRIPT_NAME"))
    check("PATH_INFO=/extra/path", env.get("PATH_INFO") == "/extra/path", env.get("PATH_INFO"))
    check("QUERY_STRING=q=1", env.get("QUERY_STRING") == "q=1", env.get("QUERY_STRING"))
    check("REMOTE_ADDR=127.0.0.1", env.get("REMOTE_ADDR") == "127.0.0.1", env.get("REMOTE_ADDR"))
    check("SERVER_SOFTWARE задан", env.get("SERVER_SOFTWARE", "").startswith("webserv"), env.get("SERVER_SOFTWARE"))
    check("SERVER_NAME из Host без порта", env.get("SERVER_NAME") == "demo.local", env.get("SERVER_NAME"))
    r = raw(b"GET /cgi-bin/env.py HTTP/1.1\r\nHost: x\r\n\r\n")
    env = dict(l.split("=", 1) for l in body(r).decode().splitlines() if "=" in l)
    check("без хвоста PATH_INFO = полный путь (нужно cgi_tester)", env.get("PATH_INFO") == "/cgi-bin/env.py", env.get("PATH_INFO"))
    r = raw(b"GET /cgi-bin/env.pyc HTTP/1.1\r\nHost: x\r\n\r\n")
    check("/cgi-bin/env.pyc - не CGI (404 как статика)", status(r) == 404, r[:60])
    r = raw(b"GET /cgi-bin/nope.py/x HTTP/1.1\r\nHost: x\r\n\r\n")
    check("несуществующий скрипт с PATH_INFO -> 404", status(r) == 404, r[:60])

    check("сервер жив", srv.poll() is None)
finally:
    srv.send_signal(2)
    try:
        srv.wait(3)
    except subprocess.TimeoutExpired:
        srv.kill()
    for name in created:
        try:
            os.unlink(os.path.join(uploads, name))
        except OSError:
            pass

print("\n%d/%d" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
