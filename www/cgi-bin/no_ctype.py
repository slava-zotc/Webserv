#!/usr/bin/env python3
# Нет Content-Type (RFC 3875 §6.2.1: MUST). Ожидается: 502.
import sys
sys.stdout.write("X-Custom: 1\n\nbody without content type\n")
