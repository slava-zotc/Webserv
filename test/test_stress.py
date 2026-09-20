#!/usr/bin/env python3
"""
test_stress.py — конкурентность и стресс-тест для webserv

Что проверяет:
    1. Много одновременных соединений (по умолчанию 50) — сервер должен
       корректно ответить каждому клиенту, poll() не должен "захлебнуться".
    2. Медленный клиент — отправляет запрос по одному байту с небольшой
       паузой между ними. Это специально проверяет, что parse() внутри
       ClientSocket корректно продолжает разбор запроса между несколькими
       recv()-вызовами (несколько итераций poll()), а не теряет данные
       и не ломается на частичном запросе.
    3. Финальная проверка "жив ли сервер" — после всей нагрузки открывается
       ещё одно простое соединение.

Использование:
    python3 test_stress.py [host] [port] [num_concurrent_clients]
По умолчанию host=127.0.0.1, port=8080, num_concurrent_clients=50
"""

import socket
import sys
import time
import threading

HOST = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
NUM_CLIENTS = int(sys.argv[3]) if len(sys.argv) > 3 else 500000

REQUEST = (
    "GET / HTTP/1.1\r\n"
    "Host: {host}\r\n"
    "Connection: close\r\n"
    "\r\n"
).format(host=HOST)

results_lock = threading.Lock()
results = {"ok": 0, "fail": 0, "errors": []}


def record(ok, message=""):
    with results_lock:
        if ok:
            results["ok"] += 1
        else:
            results["fail"] += 1
            if message:
                results["errors"].append(message)


def simple_client(client_id):
    """Один клиент: подключается, шлёт запрос целиком, ждёт ответ."""
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as sock:
            sock.sendall(REQUEST.encode())
            response = b""
            sock.settimeout(5)
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            if response.startswith(b"HTTP/1.1 200"):
                record(True)
            else:
                record(False, f"client {client_id}: неожиданный ответ: {response[:50]!r}")
    except Exception as e:
        record(False, f"client {client_id}: исключение {e}")


def test_concurrent_clients(n):
    print(f"=== Тест 1: {n} одновременных клиентов ===")
    threads = []
    for i in range(n):
        t = threading.Thread(target=simple_client, args=(i,))
        threads.append(t)
        t.start()
    for t in threads:
        t.join()

    with results_lock:
        print(f"  Успешно: {results['ok']} / {n}")
        if results["fail"] > 0:
            print(f"  Провалено: {results['fail']}")
            for err in results["errors"][:10]:
                print(f"    - {err}")
    print("")


def test_slow_client():
    """
    Отправляет запрос по одному байту с паузой между ними.
    Проверяет, что сервер (poll + non-blocking recv) корректно
    накапливает данные между несколькими итерациями event loop,
    а не теряет их и не падает на частичных данных.
    """
    print("=== Тест 2: медленный клиент (байт за байтом) ===")
    request_bytes = REQUEST.encode()
    try:
        with socket.create_connection((HOST, PORT), timeout=10) as sock:
            for b in request_bytes:
                sock.sendall(bytes([b]))
                time.sleep(0.01)  # намеренная задержка между байтами

            sock.settimeout(5)
            response = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk

            if response.startswith(b"HTTP/1.1 200"):
                print("  [OK] Медленный клиент получил корректный ответ 200")
            else:
                print(f"  [FAIL] Медленный клиент получил неожиданный ответ: {response[:80]!r}")
    except Exception as e:
        print(f"  [FAIL] Исключение при работе медленного клиента: {e}")
    print("")


def test_server_alive_after_stress():
    print("=== Тест 3: сервер жив после стресса ===")
    try:
        with socket.create_connection((HOST, PORT), timeout=5) as sock:
            sock.sendall(REQUEST.encode())
            sock.settimeout(5)
            response = sock.recv(4096)
            if response.startswith(b"HTTP/1.1 200"):
                print("  [OK] Сервер отвечает после стресс-теста")
            else:
                print(f"  [FAIL] Неожиданный ответ после стресса: {response[:80]!r}")
    except Exception as e:
        print(f"  [FAIL] Сервер не отвечает после стресса: {e}")
    print("")


if __name__ == "__main__":
    print(f"Тестируем сервер {HOST}:{PORT}\n")
    test_concurrent_clients(NUM_CLIENTS)
    test_slow_client()
    test_server_alive_after_stress()

    with results_lock:
        print("===================================")
        print(f"Итого по конкурентному тесту: OK={results['ok']} FAIL={results['fail']}")
        print("===================================")
        sys.exit(1 if results["fail"] > 0 else 0)
