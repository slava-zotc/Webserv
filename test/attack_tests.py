#!/usr/bin/env python3
"""Попытки уронить webserv: обрывы соединений, мусор, граничные значения,
slowloris, флуд, CGI-флуд. После каждой атаки проверяем, что сервер жив.

Запуск из корня проекта:  python3 test/attack_tests.py [./webserv]
Сам запускает сервер с conf/default.conf. Под valgrind:
  printf '#!/bin/sh\nexec valgrind --leak-check=full --track-fds=yes ./webserv\n' > /tmp/vg.sh
  chmod +x /tmp/vg.sh && python3 test/attack_tests.py /tmp/vg.sh
"""
import os, socket, subprocess, sys, time, threading, random

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BIN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "webserv")
HOST, PORT = "127.0.0.1", 8080
log = open("/tmp/attack_srv.log", "w")
srv = subprocess.Popen([BIN], stdout=log, stderr=log, cwd=ROOT)
time.sleep(1.0 if "vg" not in BIN else 4.0)
results = []

def alive():
    try:
        s = socket.create_connection((HOST, PORT), timeout=5)
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
        d = s.recv(64); s.close()
        return d.startswith(b"HTTP/1.1 200") and srv.poll() is None
    except Exception as e:
        return False

def send(data, read=True, timeout=5, close_early=False):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    try:
        s.sendall(data)
    except OSError:
        pass
    if close_early:
        s.close(); return b""
    out = b""
    while read:
        try:
            c = s.recv(65536)
        except (socket.timeout, OSError):
            break
        if not c: break
        out += c
    s.close()
    return out

def attack(name, fn):
    t = time.time()
    try:
        info = fn()
    except Exception as e:
        info = "exc: %r" % e
    ok = alive()
    results.append(ok)
    print("%-4s %-55s %5.1fs %s" % ("OK" if ok else "DEAD", name, time.time() - t, (info or "")[:70]), flush=True)
    if not ok:
        print("SERVER DIED, rc=%s" % srv.poll()); sys.exit(1)

st = lambda r: r.split(b"\r\n", 1)[0].decode("latin-1", "replace") if r else "<no response>"

# --- разрывы соединения ---
attack("закрыть сразу после connect", lambda: send(b"", close_early=True))
attack("закрыть посреди заголовков", lambda: send(b"GET / HTTP/1.1\r\nHo", close_early=True))
attack("закрыть посреди тела", lambda: send(b"POST /uploads/x HTTP/1.1\r\nHost: x\r\nContent-Length: 1000\r\n\r\nabc", close_early=True))
def big_get_close():
    s = socket.create_connection((HOST, PORT))
    s.sendall(b"GET /cgi-bin/big.py HTTP/1.1\r\nHost: x\r\n\r\n"); s.recv(10); s.close()
    for _ in range(20):
        s = socket.create_connection((HOST, PORT))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        s.sendall(b"GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n"); s.close()
attack("закрыть до ответа (SIGPIPE на send)", big_get_close)
def rst_close():
    import struct
    for _ in range(20):
        s = socket.create_connection((HOST, PORT))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n"); s.close()  # RST
attack("RST вместо FIN (SO_LINGER 0)", rst_close)
def cgi_abandon():
    for path in (b"/cgi-bin/slow.py", b"/cgi-bin/infinite.py", b"/cgi-bin/big.py"):
        s = socket.create_connection((HOST, PORT))
        s.sendall(b"GET " + path + b" HTTP/1.1\r\nHost: x\r\n\r\n"); time.sleep(0.3); s.close()
    time.sleep(6)
attack("бросить клиента во время CGI (slow, infinite, big)", cgi_abandon)
def cgi_post_abandon():
    body = b"x" * 5_000_000
    s = socket.create_connection((HOST, 8081 if False else PORT))
    s.sendall(b"POST /cgi-bin/hello.py HTTP/1.1\r\nHost: x\r\nContent-Length: %d\r\n\r\n" % len(body))
    s.sendall(body[:100000]); s.close()
attack("бросить POST в CGI на середине тела", cgi_post_abandon)
def cgi_ignores_stdin():
    body = b"x" * 900_000   # hello.py не читает stdin -> write в pipe получит EPIPE
    return st(send(b"POST /cgi-bin/hello.py HTTP/1.1\r\nHost: x\r\nContent-Length: %d\r\n\r\n" % len(body) + body))
attack("CGI не читает stdin, а тело большое (EPIPE)", cgi_ignores_stdin)

# --- мусор и граничные значения ---
attack("пустые строки и мусор", lambda: st(send(b"\r\n\r\n\r\n" + bytes(random.randrange(256) for _ in range(5000)))))
attack("NUL-байты в стартовой строке", lambda: st(send(b"GET /\x00\x00 HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("только LF вместо CRLF", lambda: st(send(b"GET / HTTP/1.1\nHost: x\n\n", timeout=3)))
attack("CL: -1", lambda: st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nContent-Length: -1\r\n\r\n")))
attack("CL: 99999999999999999999999", lambda: st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nContent-Length: 99999999999999999999999\r\n\r\n")))
attack("CL: 18446744073709551615", lambda: st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nContent-Length: 18446744073709551615\r\n\r\n")))
attack("CL: abc / пустой / '+5' / '5 5'", lambda: " ".join(st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nContent-Length: " + v + b"\r\n\r\nhello")) for v in (b"abc", b"", b"+5", b"5 5")))
attack("два разных Content-Length", lambda: st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello!")))
attack("chunk size ffffffffffffffff", lambda: st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\nffffffffffffffff\r\nabc\r\n")))
attack("chunk size 7fffffffffffffff", lambda: st(send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n7fffffffffffffff\r\nabc\r\n", timeout=3)))
attack("чанк без \\r\\n и закрытие", lambda: send(b"POST /uploads/a HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nab", close_early=True))
attack("метод из 10000 букв", lambda: st(send(b"A" * 10000 + b" / HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("путь '*' и absolute-form", lambda: st(send(b"GET * HTTP/1.1\r\nHost: x\r\n\r\n")) + " | " + st(send(b"GET http://x/ HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("путь без / в начале, пустой путь", lambda: st(send(b"GET index.html HTTP/1.1\r\nHost: x\r\n\r\n")) + " | " + st(send(b"GET  HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("%-мусор в пути: %, %zz, %2", lambda: " ".join(st(send(b"GET /" + p + b" HTTP/1.1\r\nHost: x\r\n\r\n")) for p in (b"%", b"%zz", b"%2", b"%ff%fe")))
attack("1000 '../' и %2e%2e%2f", lambda: st(send(b"GET /" + b"../" * 1000 + b"etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n")) + " | " + st(send(b"GET /files/%2e%2e/%2e%2e/%2e%2e/etc/passwd HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("заголовок без ':' / пустое имя / obs-fold", lambda: " ".join(st(send(b"GET / HTTP/1.1\r\nHost: x\r\n" + h + b"\r\n\r\n")) for h in (b"NoColon", b": v", b"X: a\r\n  folded")))
attack("Host повторён дважды", lambda: st(send(b"GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n")))
attack("HTTP/1.0 без Host, HTTP/0.9, HTTP/1.10", lambda: " ".join(st(send(b"GET / " + v + b"\r\n\r\n")) for v in (b"HTTP/1.0", b"HTTP/0.9", b"HTTP/1.10", b"HTTP/1.1x")))
attack("pipelining: 2 запроса в одном пакете", lambda: st(send(b"GET / HTTP/1.1\r\nHost: x\r\n\r\nGET /about/ HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("GET с телом", lambda: st(send(b"GET / HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\n\r\nhello")))
attack("DELETE каталога / корня / '..'", lambda: " ".join(st(send(b"DELETE " + p + b" HTTP/1.1\r\nHost: x\r\n\r\n")) for p in (b"/files/sub", b"/files/", b"/files")))
attack("CGI: каталог с расширением, несуществующий", lambda: st(send(b"GET /cgi-bin/nope.py HTTP/1.1\r\nHost: x\r\n\r\n")) + " | " + st(send(b"GET /cgi-bin/.py HTTP/1.1\r\nHost: x\r\n\r\n")))
attack("CGI с огромным query и заголовками в env", lambda: st(send(b"GET /cgi-bin/env.py?" + b"q=" * 3000 + b" HTTP/1.1\r\nHost: x\r\n" + b"".join(b"X-%d: %s\r\n" % (i, b"v"*50) for i in range(150)) + b"\r\n")))
attack("multipart с огромным числом пустых частей", lambda: st(send((lambda b: b"POST /uploads/ HTTP/1.1\r\nHost: x\r\nContent-Type: multipart/form-data; boundary=B\r\nContent-Length: %d\r\n\r\n" % len(b) + b)(b"--B\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\n\r\n" * 20000 + b"--B--\r\n"))))

# --- нагрузка и ресурсы ---
def slowloris():
    socks = []
    for i in range(300):
        s = socket.create_connection((HOST, PORT)); s.sendall(b"GET / HTTP/1.1\r\n"); socks.append(s)
    ok = alive()
    for s in socks: s.close()
    return "300 полуоткрытых, сервер отвечает другим: %s" % ok
attack("slowloris: 300 медленных клиентов", slowloris)
def slow_drip():
    s = socket.create_connection((HOST, PORT), timeout=20)
    req = b"GET / HTTP/1.1\r\nHost: x\r\n\r\n"
    for c in req[:-4]:
        s.send(bytes([c])); time.sleep(0.4)
    try:
        d = s.recv(100)
    except Exception:
        d = b""
    return "по байту раз в 0.4 с: " + st(d)
attack("по байту (таймаут считается от активности?)", slow_drip)
def flood():
    errs = [0]; codes = {}
    def worker():
        for _ in range(200):
            try:
                r = send(b"GET /files/hello.txt HTTP/1.1\r\nHost: x\r\n\r\n")
                codes[st(r)[:12]] = codes.get(st(r)[:12], 0) + 1
            except Exception:
                errs[0] += 1
    th = [threading.Thread(target=worker) for _ in range(50)]
    [t.start() for t in th]; [t.join() for t in th]
    return "10000 запросов в 50 потоков, ошибок %d, %s" % (errs[0], codes)
attack("флуд 50x200", flood)
def cgi_flood():
    def w():
        for _ in range(10):
            try: send(b"GET /cgi-bin/hello.py HTTP/1.1\r\nHost: x\r\n\r\n")
            except Exception: pass
    th = [threading.Thread(target=w) for _ in range(30)]
    [t.start() for t in th]; [t.join() for t in th]
    z = subprocess.run("ps --ppid %d -o stat= | grep -c Z" % srv.pid, shell=True, capture_output=True, text=True).stdout.strip()
    return "300 CGI параллельно, зомби: %s" % z
attack("CGI флуд 30x10", cgi_flood)

fds = len(os.listdir("/proc/%d/fd" % srv.pid))
kids = subprocess.run("ps --ppid %d -o pid= | wc -l" % srv.pid, shell=True, capture_output=True, text=True).stdout.strip()
print("\nfd у сервера: %d, детей: %s" % (fds, kids))
srv.send_signal(2); srv.wait(30)
for name in ("a", "x", "cl"):
    try:
        os.unlink(os.path.join(ROOT, "www", "uploads", name))
    except OSError:
        pass
print("выход по Ctrl+C, rc=%s" % srv.returncode)
print("\n%d/%d" % (sum(results), len(results)))
