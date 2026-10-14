#!/usr/bin/env python3
"""Тесты загрузки multipart/form-data (п. 12 чеклиста).

Запуск из корня проекта:  python3 test/multipart_tests.py
Сам запускает ./webserv conf/default.conf.
"""
import os, random, socket, subprocess, sys, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UPLOADS = os.path.join(ROOT, "www", "uploads")
results = []

def check(name, ok, info=""):
    results.append(bool(ok))
    print(("OK   " if ok else "FAIL ") + name + ("" if ok else "  -> " + str(info)[:200]))

def raw(data, port=8080):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    try:
        s.sendall(data)
    except (BrokenPipeError, ConnectionResetError):
        pass
    out = b""
    while True:
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

def post(body, ctype, path="/uploads/", port=8080):
    return raw(b"POST " + path.encode() + b" HTTP/1.1\r\nHost: x\r\nContent-Type: "
               + ctype.encode() + b"\r\nContent-Length: " + str(len(body)).encode()
               + b"\r\n\r\n" + body, port)

def part(name, data, filename=None, ctype=None, disp=b"Content-Disposition"):
    h = disp + b': form-data; name="' + name.encode() + b'"'
    if filename is not None:
        h += b'; filename="' + filename.encode() + b'"'
    h += b"\r\n"
    if ctype:
        h += b"Content-Type: " + ctype.encode() + b"\r\n"
    return h + b"\r\n" + data

def multipart(parts, boundary="XyZ-boundary-42"):
    b = boundary.encode()
    out = b""
    for p in parts:
        out += b"--" + b + b"\r\n" + p + b"\r\n"
    return out + b"--" + b + b"--\r\n"

def saved(name):
    path = os.path.join(UPLOADS, name)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return f.read()

before = set(os.listdir(UPLOADS))
srv = subprocess.Popen([os.path.join(ROOT, "webserv")], cwd=ROOT,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(0.5)
CT = "multipart/form-data; boundary=XyZ-boundary-42"
try:
    # 1. бинарный файл: \0, \r\n и "--" внутри должны сохраниться байт в байт
    random.seed(42)
    blob = bytes(random.randrange(256) for _ in range(200000)) + b"\r\n--XyZ\r\n\0\0"
    r = post(multipart([part("file", blob, "blob.bin", "application/octet-stream")]), CT)
    check("бинарный файл -> 201", status(r) == 201, r[:100])
    check("Location: /uploads/blob.bin", header(r, "Location") == "/uploads/blob.bin", header(r, "Location"))
    check("содержимое совпадает байт в байт", saved("blob.bin") == blob,
          len(saved("blob.bin") or b""))

    # 2. curl -F, как на защите: два файла и текстовое поле
    tmp = os.path.join(ROOT, "test", "_mp_a.txt")
    with open(tmp, "wb") as f:
        f.write(b"first file\n")
    out = subprocess.run(["curl", "-s", "-i", "-F", "comment=hello",
                          "-F", "file=@" + tmp, "-F", "file=@" + os.path.join(ROOT, "Makefile"),
                          "http://127.0.0.1:8080/uploads/"], capture_output=True).stdout
    os.unlink(tmp)
    check("curl -F два файла -> 201", status(out) == 201, out[:100])
    check("первый файл сохранён", saved("_mp_a.txt") == b"first file\n")
    with open(os.path.join(ROOT, "Makefile"), "rb") as f:
        check("второй файл (Makefile) сохранён", saved("Makefile") == f.read())
    check("текстовое поле comment не стало файлом", saved("comment") is None)
    check("в теле ответа ссылки на оба файла",
          b"_mp_a.txt" in out and b"/uploads/Makefile" in out, out[-300:])

    # 3. path traversal в filename
    r = post(multipart([part("f", b"evil", "../../../evil.txt")]), CT)
    check("filename ../../../evil.txt -> сохранён как evil.txt", status(r) == 201 and saved("evil.txt") == b"evil", r[:80])
    check("за пределами uploads ничего не появилось",
          not os.path.exists(os.path.join(ROOT, "evil.txt")) and
          not os.path.exists(os.path.join(ROOT, "www", "evil.txt")))
    r = post(multipart([part("f", b"win", "C:\\Users\\me\\win.txt")]), CT)
    check("filename C:\\Users\\me\\win.txt -> win.txt", status(r) == 201 and saved("win.txt") == b"win", r[:80])
    r = post(multipart([part("f", b"x", "..")]), CT)
    check('filename ".." -> 400', status(r) == 400, r[:60])
    r = post(multipart([part("f", b"x", "dir/")]), CT)
    check('filename "dir/" (пустое имя) -> 400', status(r) == 400, r[:60])

    # 4. ошибки формата
    body = multipart([part("f", b"data", "a.txt")])
    r = post(body[:-len(b"--XyZ-boundary-42--\r\n") - 2], CT)
    check("нет закрывающего разделителя -> 400", status(r) == 400, r[:60])
    r = post(body, "multipart/form-data")
    check("нет boundary в Content-Type -> 400", status(r) == 400, r[:60])
    r = post(b"--XyZ-boundary-42\r\nContent-Type: text/plain\r\n\r\nx\r\n--XyZ-boundary-42--\r\n", CT)
    check("часть без Content-Disposition -> 400", status(r) == 400, r[:60])
    r = post(multipart([part("comment", b"just text")]), CT)
    check("только текстовые поля, файлов нет -> 400", status(r) == 400, r[:60])
    r = post(multipart([part("file", b"", "")]), CT)
    check('пустой <input type="file"> (filename="") -> 400', status(r) == 400, r[:60])

    # 5. варианты, которые должны работать
    body = multipart([part("f", b"quoted", "q.txt")], "a b;c")
    r = post(body, 'multipart/form-data; boundary="a b;c"')
    check("boundary в кавычках с ; и пробелом", status(r) == 201 and saved("q.txt") == b"quoted", r[:80])
    body = b"preamble text\r\n" + multipart([part("f", b"pre", "pre.txt")]) + b"epilogue"
    r = post(body, CT)
    check("преамбула и эпилог игнорируются", status(r) == 201 and saved("pre.txt") == b"pre", r[:80])
    r = post(multipart([part("f", b"UP", "upper.txt", disp=b"CONTENT-DISPOSITION")]),
             "Multipart/Form-Data; BOUNDARY=XyZ-boundary-42")
    check("регистр в именах заголовков и типе не важен", status(r) == 201 and saved("upper.txt") == b"UP", r[:80])
    r = post(multipart([part("f", b"semi", "a;b.txt")]), CT)
    check('filename="a;b.txt" (; в кавычках)', status(r) == 201 and saved("a;b.txt") == b"semi", r[:80])
    r = post(multipart([part("f", b"", "empty.txt")]), CT)
    check("пустой файл с именем -> 201, файл 0 байт", status(r) == 201 and saved("empty.txt") == b"", r[:80])

    # 6. chunked + multipart: сначала HTTP снимает chunked, потом разбор
    body = multipart([part("f", b"chunked body", "ch.txt")])
    chunks = b"".join(b"%x\r\n%s\r\n" % (len(body[i:i+10]), body[i:i+10]) for i in range(0, len(body), 10))
    r = raw(b"POST /uploads/ HTTP/1.1\r\nHost: x\r\nContent-Type: " + CT.encode()
            + b"\r\nTransfer-Encoding: chunked\r\n\r\n" + chunks + b"0\r\n\r\n")
    check("chunked + multipart", status(r) == 201 and saved("ch.txt") == b"chunked body", r[:80])

    # 7. маршрут и лимиты
    r = post(multipart([part("f", b"x", "a.txt")]), CT, "/uploads/no_dir/")
    check("multipart в несуществующий каталог -> 404", status(r) == 404, r[:60])
    r = post(multipart([part("f", b"x" * 500, "big.txt")]), CT, "/", 8081)
    check("лимит тела действует на multipart (8081, 100 байт) -> 413", status(r) == 413, r[:60])
    r = post(multipart([part("f", b"x", "a.txt")]), CT, "/")
    check("POST на location без upload -> 405", status(r) == 405, r[:60])
    r = raw(b"POST /uploads/raw.txt HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\n\r\nraw")
    check("не-multipart POST работает как раньше", status(r) == 201 and saved("raw.txt") == b"raw", r[:60])

    check("сервер жив", srv.poll() is None)
finally:
    srv.send_signal(2)
    try:
        srv.wait(3)
    except subprocess.TimeoutExpired:
        srv.kill()
    for name in set(os.listdir(UPLOADS)) - before:
        os.unlink(os.path.join(UPLOADS, name))

print("\n%d/%d" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
