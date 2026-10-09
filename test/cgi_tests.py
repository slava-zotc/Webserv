#!/usr/bin/env python3
"""
Проверка разбора вывода CGI (пункт 2). Сервер должен быть запущен.
Запуск: python3 test/cgi_tests.py [port]      (по умолчанию 8080)
infinite.py здесь не вызывается — он для пункта 4 (таймауты).
"""
import socket
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080


def request(path, body=None):
    method = "POST" if body is not None else "GET"
    req = "%s %s HTTP/1.1\r\nHost: localhost\r\n" % (method, path)
    if body is not None:
        req += "Content-Type: text/plain\r\nContent-Length: %d\r\n" % len(body)
    req = req.encode() + b"\r\n" + (body or b"")
    s = socket.create_connection(("localhost", PORT), timeout=5)
    s.sendall(req)
    raw = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            raw += chunk
    except socket.timeout:
        pass
    s.close()
    head, _, payload = raw.partition(b"\r\n\r\n")
    lines = head.decode("latin-1").split("\r\n")
    code = int(lines[0].split()[1]) if lines and len(lines[0].split()) > 1 else -1
    headers = []
    for l in lines[1:]:
        k, _, v = l.partition(":")
        headers.append((k.strip().lower(), v.strip()))
    return code, headers, payload


def hdr(headers, name):
    return [v for k, v in headers if k == name]


# (скрипт, ожидаемый код, доп. проверка или None, описание)
CASES = [
    ("hello.py",         200, lambda h, b: hdr(h, "content-type") and b"Hello" in b, "200 + Content-Type от скрипта"),
    ("status404.py",     404, lambda h, b: not hdr(h, "status"),                    "Status -> код ответа, не заголовок"),
    ("custom_header.py", 201, lambda h, b: hdr(h, "x-custom") == ["yes"] and not hdr(h, "status"), "свой заголовок дошёл"),
    ("bad_cl.py",        200, lambda h, b: hdr(h, "content-length") == ["5"] and b == b"short", "один Content-Length = 5"),
    ("big.py",           200, lambda h, b: len(b) >= 200000 and hdr(h, "content-length") == [str(len(b))], "200 КБ, длина совпадает"),
    ("crash.py",         502, lambda h, b: b"must NOT" not in b,                     "exit 1 -> 502"),
    ("killed.py",        502, None,                                                   "SIGKILL -> 502"),
    ("no_blank.py",      502, None,                                                   "нет пустой строки -> 502"),
    ("dup_status.py",    502, None,                                                   "два Status -> 502"),
    ("no_ctype.py",      502, None,                                                   "нет Content-Type -> 502"),
    ("bad_status.py",    502, None,                                                   "Status 700 -> 502"),
    ("nonexistent.py",   404, None,                                                   "нет скрипта -> 404"),
    ("empty_404.py",     404, lambda h, b: len(hdr(h, "content-type")) == 1,       "404 без тела: один Content-Type"),
    ("leak_502.py",      502, lambda h, b: not hdr(h, "x-leak") and len(hdr(h, "content-type")) <= 1, "502 без заголовков скрипта"),
]

passed = 0
for script, want, check, desc in CASES:
    try:
        code, headers, body = request("/cgi-bin/" + script)
        ok = bool(code == want and (check is None or check(headers, body))
                  and len(hdr(headers, "content-type")) <= 1
                  and len(hdr(headers, "content-length")) == 1)
        note = "" if ok else "  (получено %d, заголовки %s)" % (code, headers)
    except Exception as e:  # noqa: BLE001
        ok, note = False, "  (%s)" % e
    passed += ok
    print("%s %-18s %-40s%s" % ("OK  " if ok else "FAIL", script, desc, note))

# POST: тело доходит до скрипта (заработает после пункта 5)
try:
    code, headers, body = request("/cgi-bin/echo_body.py", b"hello post")
    ok = code == 200 and body == b"hello post"
    print("%s %-18s %-40s%s" % ("OK  " if ok else "TODO", "echo_body.py", "POST-тело вернулось (пункт 5)",
                                "" if ok else "  (получено %d, тело %r)" % (code, body[:40])))
except Exception as e:  # noqa: BLE001
    print("TODO echo_body.py       POST (пункт 5)  (%s)" % e)

# Сервер жив после всех тестов? (POST в CGI без stdin может повесить весь сервер)
try:
    code, _, _ = request("/")
    alive = code > 0
except Exception:  # noqa: BLE001
    alive = False
print("%s %-18s %s" % ("OK  " if alive else "FAIL", "GET /", "сервер отвечает после всех тестов"))
if not alive:
    passed -= 1

print("\n%d/%d" % (passed, len(CASES)))
sys.exit(0 if passed == len(CASES) else 1)
