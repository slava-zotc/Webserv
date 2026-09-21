#!/usr/bin/env bash
#
# test_basic.sh — базовые сценарии для webserv
#
# Что проверяет:
#   1. Обычный GET-запрос -> ожидаем 200
#   2. Запрос с кривым Content-Length -> ожидаем 400
#   3. Обрыв соединения на середине запроса -> сервер не должен упасть
#   4. POST создаёт файл -> ожидаем 201, содержимое сверяется через GET
#   5. DELETE существующего файла -> ожидаем 204
#   6. DELETE уже удалённого файла -> ожидаем 404
#   7. DELETE директории -> ожидаем 409 (не должны позволять удалить root)
#   8. Финальная проверка, что сервер всё ещё жив после всех тестов
#
# Использование:
#   ./test_basic.sh [host] [port]
# По умолчанию host=127.0.0.1, port=8080

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"

PASS=0
FAIL=0

# Цвета для терминала — если вывод не в терминал (например, перенаправлен в файл
# или в CI без TTY), коды отключаются, чтобы не засорять лог escape-последовательностями.
if [ -t 1 ]; then
  C_RED='\033[0;31m'
  C_GREEN='\033[0;32m'
  C_YELLOW='\033[1;33m'
  C_BOLD='\033[1m'
  C_RESET='\033[0m'
else
  C_RED=''
  C_GREEN=''
  C_YELLOW=''
  C_BOLD=''
  C_RESET=''
fi

section() {
  printf "${C_BOLD}${C_YELLOW}=== %s ===${C_RESET}\n" "$1"
}

check_pass() {
  printf "  ${C_GREEN}[OK]${C_RESET} %s\n" "$1"
  PASS=$((PASS + 1))
}

check_fail() {
  printf "  ${C_RED}[FAIL]${C_RESET} %s\n" "$1"
  FAIL=$((FAIL + 1))
}

section "Тест 1: обычный GET"
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" "http://${HOST}:${PORT}/")
if [ "$RESPONSE" = "200" ]; then
  check_pass "GET / вернул 200"
else
  check_fail "GET / вернул '$RESPONSE', ожидался 200"
fi
echo ""

section "Тест 2: кривой Content-Length -> ожидаем 400"
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

section "Тест 3: обрыв соединения на середине запроса"
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

section "Тест 4: POST создаёт файл, содержимое сверяется через GET"
# ВАЖНО: POST резолвится относительно upload_dir_ (сейчас это www/uploads),
# а GET/DELETE резолвятся относительно root_ (www). Раз upload_dir_ физически
# лежит внутри root_ как подпапка "uploads", читать и удалять загруженный
# файл нужно по пути с префиксом /uploads/, а не по тому же пути, что при POST.
UPLOAD_PATH="/test_upload_$$.txt"
RETRIEVE_PATH="/uploads${UPLOAD_PATH}"
UPLOAD_BODY="hello from test_basic $$"
RESPONSE_POST=$(curl -s -o /dev/null -w "%{http_code}" \
  -X POST \
  -d "$UPLOAD_BODY" \
  "http://${HOST}:${PORT}${UPLOAD_PATH}")
if [ "$RESPONSE_POST" = "201" ]; then
  check_pass "POST $UPLOAD_PATH вернул 201"
else
  check_fail "POST $UPLOAD_PATH вернул '$RESPONSE_POST', ожидался 201"
fi

# Сверяем, что файл реально лёг на диск с тем же содержимым — читаем его назад через GET,
# не трогая файловую систему сервера напрямую.
FETCHED_BODY=$(curl -s "http://${HOST}:${PORT}${RETRIEVE_PATH}")
if [ "$FETCHED_BODY" = "$UPLOAD_BODY" ]; then
  check_pass "Содержимое загруженного файла совпадает с отправленным"
else
  check_fail "Содержимое не совпадает: отправили '$UPLOAD_BODY', получили '$FETCHED_BODY'"
fi
echo ""

section "Тест 5: DELETE существующего файла -> ожидаем 204"
RESPONSE_DELETE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "http://${HOST}:${PORT}${RETRIEVE_PATH}")
if [ "$RESPONSE_DELETE" = "204" ]; then
  check_pass "DELETE $RETRIEVE_PATH вернул 204"
else
  check_fail "DELETE $RETRIEVE_PATH вернул '$RESPONSE_DELETE', ожидался 204"
fi
echo ""

section "Тест 6: DELETE уже удалённого файла -> ожидаем 404"
RESPONSE_DELETE_AGAIN=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "http://${HOST}:${PORT}${RETRIEVE_PATH}")
if [ "$RESPONSE_DELETE_AGAIN" = "404" ]; then
  check_pass "Повторный DELETE $RETRIEVE_PATH вернул 404"
else
  check_fail "Повторный DELETE $RETRIEVE_PATH вернул '$RESPONSE_DELETE_AGAIN', ожидался 404"
fi
echo ""

section "Тест 7: DELETE директории (root) -> ожидаем 409"
RESPONSE_DELETE_DIR=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "http://${HOST}:${PORT}/")
if [ "$RESPONSE_DELETE_DIR" = "409" ]; then
  check_pass "DELETE / (директория) вернул 409"
else
  check_fail "DELETE / (директория) вернул '$RESPONSE_DELETE_DIR', ожидался 409"
fi
echo ""

section "Тест 8: Content-Length больше лимита (10 MiB) -> ожидаем 400, сервер не падает"
# Регрессионный тест на баг, где отсутствие потолка для Content-Length
# в HttpRequest могло привести к неограниченному росту буфера тела запроса.
# Заголовок с Content-Length больше лимита должен быть отклонён сразу после
# разбора заголовков (до чтения тела) — тело реально отправлять не нужно.
RAW_RESPONSE_BIG_CL=$(printf 'GET / HTTP/1.1\r\nHost: %s\r\nContent-Length: 999999999\r\n\r\n' "$HOST" |
  nc -w 2 "$HOST" "$PORT")
STATUS_LINE_BIG_CL=$(echo "$RAW_RESPONSE_BIG_CL" | head -n 1)
if echo "$STATUS_LINE_BIG_CL" | grep -q "400"; then
  check_pass "Content-Length сверх лимита вернул 400 ($STATUS_LINE_BIG_CL)"
else
  check_fail "Content-Length сверх лимита вернул '$STATUS_LINE_BIG_CL', ожидался 400"
fi

sleep 0.3
RESPONSE_AFTER_BIG_CL=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "http://${HOST}:${PORT}/")
if [ -n "$RESPONSE_AFTER_BIG_CL" ]; then
  check_pass "Сервер пережил запрос со слишком большим Content-Length и отвечает ($RESPONSE_AFTER_BIG_CL)"
else
  check_fail "После запроса со слишком большим Content-Length сервер не отвечает — возможно, упал"
fi
echo ""

section "Тест 9: сервер всё ещё жив после всех тестов"
FINAL_CHECK=$(curl -s -o /dev/null -w "%{http_code}" --max-time 2 "http://${HOST}:${PORT}/")
if [ "$FINAL_CHECK" = "200" ]; then
  check_pass "Сервер жив после всех тестов"
else
  check_fail "Сервер не отвечает после всех тестов ('$FINAL_CHECK') — возможно, упал где-то по пути"
fi
echo ""

echo "==================================="
if [ "$FAIL" -gt 0 ]; then
  printf "${C_BOLD}Итого: ${C_GREEN}PASS=%s${C_RESET} ${C_BOLD}${C_RED}FAIL=%s${C_RESET}\n" "$PASS" "$FAIL"
else
  printf "${C_BOLD}Итого: ${C_GREEN}PASS=%s FAIL=%s${C_RESET}\n" "$PASS" "$FAIL"
fi
echo "==================================="

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
exit 0