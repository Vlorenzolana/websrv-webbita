#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-18080}"
TMP_CONFIG="$(mktemp)"
SERVER_LOG="$(mktemp)"
TMP_DIR="$(mktemp -d)"
SERVER_PID=""

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$TMP_CONFIG" "$SERVER_LOG"
    rm -rf "$TMP_DIR"
    rm -f "$ROOT_DIR/www/uploads/raw.bin" \
          "$ROOT_DIR/www/uploads/form.txt" \
          "$ROOT_DIR/www/uploads/chunk.bin" \
          "$ROOT_DIR/www/cgi-bin/fail.py" \
          "$ROOT_DIR/www/cgi-bin/slow.py"
}
trap cleanup EXIT

cd "$ROOT_DIR"
make >/dev/null
sed "s/127.0.0.1:8080/127.0.0.1:${PORT}/" config/webserv.conf > "$TMP_CONFIG"

cat > www/cgi-bin/fail.py <<'PY'
#!/usr/bin/env python3
raise RuntimeError("intentional test failure")
PY
cat > www/cgi-bin/slow.py <<'PY'
#!/usr/bin/env python3
import time
time.sleep(30)
print("Content-Type: text/plain")
print()
print("late response")
PY
chmod +x www/cgi-bin/fail.py www/cgi-bin/slow.py

./webserv "$TMP_CONFIG" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 0.3

status="$(curl -sS -o "$TMP_DIR/index" -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]

grep -q "webserv is running" "$TMP_DIR/index"

status="$(curl -sS -o "$TMP_DIR/missing" -w '%{http_code}' "http://127.0.0.1:${PORT}/missing")"
[[ "$status" == "404" ]]
grep -q "Custom 404" "$TMP_DIR/missing"

printf 'ABC\0XYZ' > "$TMP_DIR/raw.bin"
status="$(curl -sS -o /dev/null -w '%{http_code}' -X POST \
    --data-binary @"$TMP_DIR/raw.bin" \
    "http://127.0.0.1:${PORT}/uploads/raw.bin")"
[[ "$status" == "201" ]]
cmp "$TMP_DIR/raw.bin" www/uploads/raw.bin

printf 'multipart-data' > "$TMP_DIR/form.txt"
status="$(curl -sS -o /dev/null -w '%{http_code}' \
    -F "file=@$TMP_DIR/form.txt" \
    "http://127.0.0.1:${PORT}/uploads/")"
[[ "$status" == "201" ]]
cmp "$TMP_DIR/form.txt" www/uploads/form.txt

head -c 1048576 /dev/urandom > "$TMP_DIR/one-megabyte.bin"
status="$(curl -sS -o "$TMP_DIR/cgi-output" -w '%{http_code}' -X POST \
    --data-binary @"$TMP_DIR/one-megabyte.bin" \
    "http://127.0.0.1:${PORT}/cgi-bin/echo.py")"
[[ "$status" == "200" ]]
python3 - "$TMP_DIR/one-megabyte.bin" "$TMP_DIR/cgi-output" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_bytes()
response = Path(sys.argv[2]).read_bytes()
assert response.endswith(source)
PY

status="$(curl -sS -o /dev/null -w '%{http_code}' \
    "http://127.0.0.1:${PORT}/cgi-bin/fail.py")"
[[ "$status" == "500" ]]

python3 - "$PORT" <<'PY'
import socket
import sys
import time
port = int(sys.argv[1])
request = (
    b"POST /uploads/chunk.bin HTTP/1.1\r\n"
    b"host: localhost\r\n"
    b"transfer-encoding: chunked\r\n"
    b"content-type: application/octet-stream\r\n\r\n"
    b"4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n"
)
with socket.create_connection(("127.0.0.1", port)) as sock:
    for byte in request:
        sock.sendall(bytes((byte,)))
    response = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        response += chunk
assert response.startswith(b"HTTP/1.1 201")
PY
[[ "$(cat www/uploads/chunk.bin)" == "Wikipedia" ]]

curl -sS -o "$TMP_DIR/slow" -w '%{http_code}' \
    "http://127.0.0.1:${PORT}/cgi-bin/slow.py" > "$TMP_DIR/slow-code" &
SLOW_CURL_PID=$!
sleep 0.5
status="$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
wait "$SLOW_CURL_PID"
[[ "$(cat "$TMP_DIR/slow-code")" == "504" ]]

seq 1 50 | xargs -P10 -I{} sh -c \
    "test \"\$(curl -sS -o /dev/null -w '%{http_code}' http://127.0.0.1:${PORT}/)\" = 200"

if ps -o stat= --ppid "$SERVER_PID" | grep -q Z; then
    echo "Zombie CGI process detected" >&2
    exit 1
fi

echo "All smoke tests passed on port ${PORT}."
