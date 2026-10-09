#!/usr/bin/env python3
# Два поля Status (RFC 3875 §6.3: CGI-поле не больше одного раза).
# Ожидается: 502.
import sys
sys.stdout.write("Status: 200 OK\nStatus: 404 Not Found\nContent-Type: text/plain\n\nbody\n")
