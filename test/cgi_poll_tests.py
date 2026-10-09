#!/usr/bin/env python3
"""
Пункт 3: CGI не должен блокировать сервер. Сервер должен быть запущен.
Запуск: python3 test/cgi_poll_tests.py [port]
"""
import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080


def get(path, timeout=10):
    s = socket.create_connection(("localhost", PORT), timeout=timeout)
    s.sendall(("GET %s HTTP/1.1\r\nHost: localhost\r\n\r\n" % path).encode())
    raw = b""
    try:
        while True:
            c = s.recv(65536)
            if not c:
                break
            raw += c
    except socket.timeout:
        pass
    s.close()
    first = raw.split(b"\r\n", 1)[0].split()
    return int(first[1]) if len(first) > 1 else -1, raw


results = []


def report(ok, name, note):
    results.append(ok)
    print("%s %-38s %s" % ("OK  " if ok else "FAIL", name, note))


# 1. Пока slow.py спит 3 с, GET / должен ответить сразу.
slow = {}
t = threading.Thread(target=lambda: slow.update(r=get("/cgi-bin/slow.py")))
t.start()
time.sleep(0.3)
t0 = time.time()
code, _ = get("/", timeout=5)
dt = time.time() - t0
report(code == 200 and dt < 1.0, "GET / во время slow.py", "%.2f с (нужно < 1 с), код %d" % (dt, code))
t.join()
code, raw = slow["r"]
report(code == 200 and b"slow done" in raw, "slow.py сам ответил", "код %d" % code)

# 2. 5 параллельных slow.py: при poll() ~3 с, при блокирующем чтении ~15 с.
t0 = time.time()
res = []
ths = [threading.Thread(target=lambda: res.append(get("/cgi-bin/slow.py", timeout=20)[0])) for _ in range(5)]
for th in ths:
    th.start()
for th in ths:
    th.join()
dt = time.time() - t0
report(res.count(200) == 5 and dt < 6, "5 x slow.py параллельно", "%.1f с (нужно < 6 с), коды %s" % (dt, res))

# 3. Клиент отключился, пока скрипт работает: сервер жив, скрипт убит.
s = socket.create_connection(("localhost", PORT))
s.sendall(b"GET /cgi-bin/slow.py HTTP/1.1\r\nHost: localhost\r\n\r\n")
time.sleep(0.3)
s.close()
time.sleep(0.5)
code, _ = get("/", timeout=5)
report(code == 200, "клиент ушёл во время CGI", "сервер отвечает, код %d" % code)

# 5. Пункт 4: зависший скрипт (infinite.py) -> 504, сервер всё это время отвечает.
inf = {}
t = threading.Thread(target=lambda: inf.update(r=get("/cgi-bin/infinite.py", timeout=20), t=time.time()))
t0 = time.time()
t.start()
time.sleep(1)
code, _ = get("/", timeout=5)
report(code == 200, "GET / пока висит infinite.py", "код %d" % code)
t.join()
code, _ = inf["r"]
dt = inf["t"] - t0
report(code == 504 and 3 <= dt <= 10, "infinite.py -> 504 по таймауту", "код %d через %.1f с (нужно 504 за 3..10 с)" % (code, dt))

# 4. Обычные CGI по-прежнему работают
code, raw = get("/cgi-bin/hello.py")
report(code == 200 and b"Hello" in raw, "hello.py после всего", "код %d" % code)

print("\n%d/%d" % (sum(results), len(results)))
print("Проверь вручную: ps --ppid $(pgrep webserv) -o pid=,stat=,cmd=   -> пусто (нет скриптов, зомби и infinite.py)")
sys.exit(0 if all(results) else 1)
