#!/usr/bin/env python3
"""
Пункт 4: таймаут клиента (408). Сервер должен быть запущен.
Запуск: python3 test/timeout_tests.py [port] [client_timeout_sec]
"""
import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 10

results = []


def report(ok, name, note):
    results.append(ok)
    print("%s %-40s %s" % ("OK  " if ok else "FAIL", name, note))


def wait_answer(sock, timeout):
    sock.settimeout(timeout)
    t0 = time.time()
    raw = b""
    try:
        while True:
            c = sock.recv(65536)
            if not c:
                break
            raw += c
    except socket.timeout:
        pass
    first = raw.split(b"\r\n", 1)[0].split()
    code = int(first[1]) if len(first) > 1 else -1
    return code, time.time() - t0, raw.split(b"\r\n", 1)[0]


def get(path):
    s = socket.create_connection(("localhost", PORT), timeout=5)
    s.sendall(("GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % path).encode())
    code, _, _ = wait_answer(s, 5)
    s.close()
    return code


res = {}


def silent():
    s = socket.create_connection(("localhost", PORT))
    res["silent"] = wait_answer(s, LIMIT + 8)
    s.close()


def partial():
    s = socket.create_connection(("localhost", PORT))
    s.sendall(b"GET / HTTP/1.1\r\nHost: loc")
    res["partial"] = wait_answer(s, LIMIT + 8)
    s.close()


th = [threading.Thread(target=silent), threading.Thread(target=partial)]
for t in th:
    t.start()
time.sleep(1)
tg = time.time()
code = get("/")
report(code == 200 and time.time() - tg < 1, "GET / пока висят молчуны", "код %d" % code)
for t in th:
    t.join()

lo, hi = LIMIT - 1, LIMIT + 3
c, dt, line = res["silent"]
report(c == 408 and lo <= dt <= hi, "подключился и молчит -> 408", "код %d через %.1f с (нужно 408 за %d..%d с)" % (c, dt, lo, hi))
report(line.endswith(b"Request Timeout"), "reason phrase у 408", repr(line))
c, dt, _ = res["partial"]
report(c == 408 and lo <= dt <= hi, "половина заголовков -> 408", "код %d через %.1f с (нужно 408 за %d..%d с)" % (c, dt, lo, hi))

s = socket.create_connection(("localhost", PORT))
for part in [b"GET / HTTP/1.1\r\n", b"Host: localhost\r\n", b"\r\n"]:
    s.sendall(part)
    time.sleep(min(2, LIMIT / 3.0))
c, _, _ = wait_answer(s, 5)
s.close()
report(c == 200, "запрос по кускам, паузы < таймаута", "код %d (нужно 200)" % c)

code = get("/")
report(code == 200, "сервер жив после всего", "код %d" % code)

print("\n%d/%d" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
