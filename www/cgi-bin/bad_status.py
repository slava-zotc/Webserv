#!/usr/bin/env python3
# Код вне диапазона 100..599 (RFC 9110 §15). Ожидается: 502.
import sys
sys.stdout.write("Status: 700 Weird\nContent-Type: text/plain\n\nbody\n")
