#!/usr/bin/env python3
"""Тесты конфига и мелочей протокола (п. 6, 7, 8, 9, 11, 15 чеклиста).

Запуск из корня проекта:  python3 test/config_tests.py
Сам запускает ./webserv с временными конфигами, сервер на 8080 не нужен.
"""
import os, socket, subprocess, sys, tempfile, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BIN = os.path.join(ROOT, "webserv")
WWW = os.path.join(ROOT, "www")
results = []

def check(name, ok, info=""):
    results.append(bool(ok))
    print(("OK   " if ok else "FAIL ") + name + ("" if ok else "  -> " + str(info)))

def write_conf(text):
    f = tempfile.NamedTemporaryFile("w", suffix=".conf", delete=False)
    f.write(text.replace("@WWW@", WWW))
    f.close()
    return f.name

def start(text):
    path = write_conf(text)
    p = subprocess.Popen([BIN, path], stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, cwd=ROOT)
    time.sleep(0.4)
    return p, path

def stop(p):
    p.send_signal(2)
    try:
        p.wait(3)
    except subprocess.TimeoutExpired:
        p.kill(); p.wait()

def must_fail(name, text, expect_in_err):
    path = write_conf(text)
    try:
        r = subprocess.run([BIN, path], capture_output=True, text=True,
                           timeout=3, cwd=ROOT)
        ok = r.returncode != 0 and expect_in_err in r.stderr
        check(name, ok, "rc=%d stderr=%r" % (r.returncode, r.stderr))
    except subprocess.TimeoutExpired:
        check(name, False, "server started instead of failing")
    os.unlink(path)

def raw(host, port, data):
    s = socket.create_connection((host, port), timeout=3)
    s.sendall(data)
    out = b""
    while True:
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            break
        if not chunk:
            break
        out += chunk
    s.close()
    return out

def can_connect(host, port):
    try:
        socket.create_connection((host, port), timeout=1).close()
        return True
    except OSError:
        return False

def status(resp):
    try:
        return int(resp.split(b" ", 2)[1])
    except Exception:
        return None

def header(resp, name):
    head = resp.split(b"\r\n\r\n", 1)[0].decode("latin-1")
    vals = [l.split(":", 1)[1].strip() for l in head.split("\r\n")[1:]
            if l.split(":", 1)[0].lower() == name.lower()]
    return vals

# ---------- п. 6: listen host:port ----------
p, path = start("""
server { listen 127.0.0.1:8091; root @WWW@;
  location / { allowed_methods GET; } }
server { listen 8092; root @WWW@;
  location / { allowed_methods GET; index index.html; } }
""")
local_ip = None
try:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("10.255.255.255", 1)); local_ip = s.getsockname()[0]; s.close()
except OSError:
    pass
check("listen 127.0.0.1:8091 принимает на 127.0.0.1", can_connect("127.0.0.1", 8091))
if local_ip and local_ip != "127.0.0.1":
    check("listen 127.0.0.1:8091 НЕ принимает на %s" % local_ip,
          not can_connect(local_ip, 8091))
    check("listen 8092 (все интерфейсы) принимает на %s" % local_ip,
          can_connect(local_ip, 8092))
r = raw("127.0.0.1", 8092, b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
check("listen 8092 отвечает", status(r) in (200, 404), r[:80])
stop(p); os.unlink(path)

p, path = start("""
server { listen localhost:8093; root @WWW@;
  location / { allowed_methods GET; } }
""")
check("listen localhost:8093 (имя через getaddrinfo)", can_connect("127.0.0.1", 8093))
stop(p); os.unlink(path)

# ---------- п. 15: понятные ошибки запуска ----------
must_fail("одинаковый host:port в двух server -> ошибка",
          """server { listen 8094; root @WWW@; location / { } }
             server { listen 8094; root @WWW@; location / { } }""",
          "Duplicate listen")
must_fail("неизвестный хост -> ошибка резолва",
          "server { listen no.such.host.invalid:8095; root @WWW@; location / { } }",
          "Cannot resolve")
must_fail("чужой адрес -> Cannot listen ... Cannot assign",
          "server { listen 10.254.254.254:8096; root @WWW@; location / { } }",
          "Cannot listen on 10.254.254.254:8096")
must_fail("плохой порт -> ошибка",
          "server { listen 127.0.0.1:99999; root @WWW@; location / { } }",
          "Invalid port")
must_fail("повтор директивы -> ошибка",
          "server { listen 8097; root @WWW@; location / { autoindex on; autoindex off; } }",
          "Duplicate directive 'autoindex'")
must_fail("error_page без пути -> ошибка",
          "server { listen 8097; root @WWW@; error_page 404; location / { } }",
          "error_page")
for name, text, err in (
    ("директива вне server", "listen 8080;", "at top level"),
    ("лишняя }", "}", "at top level"),
    ("location в location", "server { listen 8097; root @WWW@; location / { location /x { } } }", "not allowed inside location"),
    ("location без пути", "server { listen 8097; root @WWW@; location { } }", "location needs one path"),
    ("неизвестная директива", "server { listen 8097; root @WWW@; location / { autoindx on; } }", "Unknown directive 'autoindx'"),
    ("забыли ; перед }", "server { listen 8097; root @WWW@; location / { autoindex on } }", "Missing ';'"),
    ("return без цели", "server { listen 8097; root @WWW@; location / { return 302; } }", "return: expected 2"),
    ("пустой конфиг", "", "No server blocks"),
):
    must_fail(name + " -> ошибка", text, err)

blocker = socket.socket(); blocker.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
blocker.bind(("0.0.0.0", 8098)); blocker.listen(1)
must_fail("порт занят другим процессом -> Address already in use",
          "server { listen 8098; root @WWW@; location / { } }",
          "Address already in use")
blocker.close()

# ---------- п. 7: несколько error_page ----------
tmpdir = tempfile.mkdtemp()
for name, txt in (("404.html", "custom-404"), ("405.html", "custom-405"),
                  ("5xx.html", "custom-5xx")):
    open(os.path.join(tmpdir, name), "w").write(txt)
p, path = start("""
server { listen 8090; root @WWW@;
  error_page 404 %s/404.html;
  error_page 405 %s/405.html;
  error_page 500 501 %s/5xx.html;
  location / { allowed_methods GET; }
  location /ro { allowed_methods GET DELETE; }
  location /none { allowed_methods; } }
""" % (tmpdir, tmpdir, tmpdir))
r = raw("127.0.0.1", 8090, b"GET /no_such_file HTTP/1.1\r\nHost: x\r\n\r\n")
check("error_page 404 -> своя страница", status(r) == 404 and b"custom-404" in r, r[:120])
r = raw("127.0.0.1", 8090, b"POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n")
check("error_page 405 -> своя страница", status(r) == 405 and b"custom-405" in r, r[:120])

# ---------- п. 8: Allow в 405 ----------
check("405 содержит Allow: GET", header(r, "Allow") == ["GET"], header(r, "Allow"))
r = raw("127.0.0.1", 8090, b"POST /ro HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n")
check("Allow: GET, DELETE", status(r) == 405 and header(r, "Allow") == ["GET, DELETE"],
      header(r, "Allow"))

# ---------- п. 9: неизвестный метод -> 501 ----------
r = raw("127.0.0.1", 8090, b"PUT / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n")
check("PUT -> 501", status(r) == 501, r[:80])
check("501 тоже через error_page (500 501 одной строкой)", b"custom-5xx" in r, r[:200])
r = raw("127.0.0.1", 8090, b"BREW /nowhere HTTP/1.1\r\nHost: x\r\n\r\n")
check("неизвестный метод на несуществующий путь -> 501, не 404", status(r) == 501, r[:80])

# ---------- п. 11: Connection: close ----------
r = raw("127.0.0.1", 8090, b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
check("Connection: close ровно один раз", header(r, "Connection") == ["close"],
      header(r, "Connection"))
check("Content-Length ровно один раз", len(header(r, "Content-Length")) == 1)
check("сервер жив после всех запросов", p.poll() is None)
stop(p); os.unlink(path)

print("\n%d/%d" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
