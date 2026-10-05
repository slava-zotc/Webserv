#!/usr/bin/env python3
import sys

sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("x" * 200000)  # 200 KB, больше буфера pipe (~64 KB)
sys.stdout.flush()