#!/usr/bin/env python3
"""
Безопасность пути и разбор стартовой строки. Сервер должен быть запущен.
Запуск: python3 test/security_tests.py [port]
Конфиг: location /uploads с upload_enabled on и разрешёнными POST/DELETE
(как в conf/default.conf); в корне сайта есть index.html.
"""
import os
import socket
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
results = []


def raw(req):
    s = socket.create_connection(("localhost", PORT), timeout=5)
    s.sendall(req)
    data = b""
    try:
        while True:
            c = s.recv(65536)
            if not c:
                break
            data += c
    except socket.timeout:
        pass
    s.close()
    head, _, body = data.partition(b"\r\n\r\n")
    first = head.split(b"\r\n", 1)[0].split()
    return (int(first[1]) if len(first) > 1 else -1), body


def req(method, path, body=b""):
    r = ("%s %s HTTP/1.1\r\nHost: x\r\n" % (method, path)).encode()
    if body:
        r += b"Content-Length: %d\r\n" % len(body)
    return raw(r + b"\r\n" + body)


def check(name, ok, note):
    results.append(ok)
    print("%s %-44s %s" % ("OK  " if ok else "FAIL", name, note))


UP = "../" * 15
victim = "/tmp/webserv_victim.txt"
open(victim, "w").write("victim")

code, body = req("GET", "/" + UP + "etc/passwd")
check("GET ../../etc/passwd", code == 400 and b"root:" not in body, "код %d" % code)
code, body = req("GET", "/uploads/" + UP + "etc/passwd")
check("GET /uploads/../../etc/passwd", code == 400 and b"root:" not in body, "код %d" % code)
code, body = req("GET", "/%2e%2e/%2e%2e/%2e%2e/etc/passwd")
check("GET %2e%2e (закодированные ..)", code == 400 and b"root:" not in body, "код %d" % code)
code, _ = req("DELETE", "/uploads/" + UP + "tmp/webserv_victim.txt")
check("DELETE через .. не удаляет файл", os.path.exists(victim), "код %d" % code)
code, _ = req("POST", "/uploads/" + UP + "tmp/webserv_pwned.txt", b"pwn")
check("POST через .. не создаёт файл", not os.path.exists("/tmp/webserv_pwned.txt"), "код %d" % code)
code, _ = req("GET", "/uploads/../index.html")
check(".. внутри сайта нормализуется", code == 200, "код %d (/uploads/../index.html -> /index.html)" % code)
code, _ = req("GET", "/./index.html")
check("/./index.html", code == 200, "код %d" % code)
code, _ = req("GET", "/index%2Ehtml")
check("%2E декодируется", code == 200, "код %d" % code)
code, _ = req("GET", "/bad%zz")
check("битый %zz -> 400", code == 400, "код %d" % code)
code, _ = req("GET", "/a%00b")
check("%00 -> 400", code == 400, "код %d" % code)
code, _ = req("GET", "noslash")
check("путь без / -> 400", code == 400, "код %d" % code)
code, _ = raw(b"GET / HTTP/2.0\r\nHost: x\r\n\r\n")
check("HTTP/2.0 -> 505", code == 505, "код %d" % code)
code, _ = raw(b"GET / FOO/1.1\r\nHost: x\r\n\r\n")
check("мусор вместо версии -> 400", code == 400, "код %d" % code)
code, _ = raw(b"GET / HTTP/1.0\r\n\r\n")
check("HTTP/1.0 без Host -> 200", code == 200, "код %d" % code)

for p in (victim, "/tmp/webserv_pwned.txt"):
    if os.path.exists(p):
        os.remove(p)
code, _ = req("GET", "/")
check("сервер жив", code == 200, "код %d" % code)
print("\n%d/%d" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
