#!/usr/bin/env bash
#
# test_basic.sh — базовые сценарии для webserv
#
# Что проверяет:
#   1. Обычный GET-запрос -> ожидаем 200
#   2. Запрос с кривым Content-Length -> ожидаем 400
#   3. Обрыв соединения на середине запроса -> сервер не должен упасть
#   4. POST с телом -> сервер должен принять и не зависнуть
#   5. Финальная проверка, что сервер всё ещё жив после всех тестов
#
# Использование:
#   ./test_basic.sh [host] [port]
# По умолчанию host=127.0.0.1, port=8080

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"

PASS=0
FAIL=0

check_pass() {
  echo "  [OK] $1"
  PASS=$((PASS + 1))
}

check_fail() {
  echo "  [FAIL] $1"
  FAIL=$((FAIL + 1))
}

echo "=== Тест 1: обычный GET ==="
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" "http://${HOST}:${PORT}/")
if [ "$RESPONSE" = "200" ]; then
  check_pass "GET / вернул 200"
else
  check_fail "GET / вернул '$RESPONSE', ожидался 200"
fi
echo ""

echo "=== Тест 2: кривой Content-Length -> ожидаем 400 ==="
# Отправляем запрос напрямую через nc, потому что curl сам не даст
# отправить заведомо невалидный Content-Length так просто.
RAW_RESPONSE=$(printf 'GET / HTTP/1.1\r\nHost: %s\r\nContent-Length: notanumber\r\n\r\n' "$HOST" |
  nc -w 2 "$HOST" "$PORT")
STATUS_LINE=$(echo "$RAW_RESPONSE" | head -n 1)
if echo "$STATUS_LINE" | grep -q "400"; then
  check_pass "Кривой Content-Length вернул 400 ($STATUS_LINE)"
else
  check_fail "Кривой Content-Length вернул '$STATUS_LINE', ожидался 400"
fi
echo ""

echo "=== Тест 3: обрыв соединения на середине запроса ==="
# Отправляем только часть заголовков и сразу закрываем сокет.
# Сервер должен просто удалить это соединение, не упав.
(
  printf 'GET / HTTP/1.1\r\nHost: incomplete'
  sleep 0.2
) | nc -w 1 "$HOST" "$PORT" >/dev/null 2>&1

# Проверяем, что сервер всё ещё принимает новые соединения после обрыва.
sleep 0.5
RESPONSE_AFTER=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "http://${HOST}:${PORT}/")
if [ "$RESPONSE_AFTER" = "200" ]; then
  check_pass "Сервер пережил обрыв соединения и всё ещё отвечает 200"
else
  check_fail "После обрыва соединения сервер вернул '$RESPONSE_AFTER' (или не ответил) — возможно, упал"
fi
echo ""

echo "=== Тест 4: POST с телом ==="
BODY="some=data&for=test"
RESPONSE_POST=$(curl -s -o /dev/null -w "%{http_code}" \
  -X POST \
  -d "$BODY" \
  "http://${HOST}:${PORT}/")
if [ "$RESPONSE_POST" = "200" ] || [ "$RESPONSE_POST" = "400" ]; then
  check_pass "POST с телом обработан, код: $RESPONSE_POST (роутинг ещё не реализован, поэтому любой валидный HTTP-ответ — уже хорошо)"
else
  check_fail "POST с телом вернул неожиданный код: '$RESPONSE_POST' (возможно, сервер завис или упал)"
fi
echo ""

echo "=== Тест 5: сервер всё ещё жив после всех тестов ==="
FINAL_CHECK=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "http://${HOST}:${PORT}/")
if [ "$FINAL_CHECK" = "200" ]; then
  check_pass "Сервер жив после всех тестов"
else
  check_fail "Сервер не отвечает после всех тестов ('$FINAL_CHECK') — возможно, упал где-то по пути"
fi
echo ""

echo "==================================="
echo "Итого: PASS=$PASS FAIL=$FAIL"
echo "==================================="

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
exit 0
