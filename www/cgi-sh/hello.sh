#!/bin/bash
# CGI на bash: показывает, что интерпретатор выбирается по расширению.
# curl "http://localhost:8080/cgi-sh/hello.sh?who=evaluator"
printf 'Content-Type: text/plain\r\n\r\n'
echo "Hello from bash CGI"
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "QUERY_STRING=$QUERY_STRING"
echo "cwd=$(pwd)"
