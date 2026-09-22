#!/usr/bin/env python3
import os
import sys
body = sys.stdin.buffer.read()
print("Content-Type: text/plain")
print()
print("method=" + os.environ.get("REQUEST_METHOD", ""))
print("query=" + os.environ.get("QUERY_STRING", ""))
sys.stdout.flush()
sys.stdout.buffer.write(body)
