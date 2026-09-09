#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-18080}"
SECOND_PORT=$((PORT + 1))
TMP_CONFIG="$(mktemp)"
SERVER_LOG="$(mktemp)"
TMP_DIR="$(mktemp -d)"
SERVER_PID=""
LEAK_LOG=""
VHOST_ALPHA_ROOT="$TMP_DIR/vhost-alpha"
VHOST_BETA_ROOT="$TMP_DIR/vhost-beta"

if [[ "${LEAK_CHECK:-0}" == "1" ]]; then
    command -v valgrind >/dev/null 2>&1 || {
        echo "Leak test requires valgrind" >&2
        exit 2
    }
    LEAK_LOG="$(mktemp)"
fi

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$TMP_CONFIG" "$SERVER_LOG" "$LEAK_LOG"
    rm -rf "$TMP_DIR"
    rm -f "$ROOT_DIR/www/uploads/raw.bin" \
          "$ROOT_DIR/www/uploads/form.txt" \
          "$ROOT_DIR/www/uploads/chunk.bin" \
            "$ROOT_DIR/www/uploads/read-only.txt" \
            "$ROOT_DIR/www/uploads/delete-link" \
          "$ROOT_DIR/www/cgi-bin/fail.py" \
          "$ROOT_DIR/www/cgi-bin/slow.py"
}
trap cleanup EXIT

cd "$ROOT_DIR"
make >/dev/null
mkdir -p "$VHOST_ALPHA_ROOT" "$VHOST_BETA_ROOT"
printf 'alpha virtual host\n' > "$VHOST_ALPHA_ROOT/index.html"
printf 'beta virtual host\n' > "$VHOST_BETA_ROOT/index.html"
sed "s/127.0.0.1:8081/127.0.0.1:${PORT}/" config/webserv.conf > "$TMP_CONFIG"
cat >> "$TMP_CONFIG" <<EOF

server {
    listen 127.0.0.1:${PORT};
    server_name alpha.localhost;
    root ${VHOST_ALPHA_ROOT};

    location / {
        allowed_methods GET;
        root ${VHOST_ALPHA_ROOT};
        index index.html;
        autoindex off;
    }
}

server {
    listen 127.0.0.1:${PORT};
    server_name beta.localhost;
    root ${VHOST_BETA_ROOT};

    location / {
        allowed_methods GET;
        root ${VHOST_BETA_ROOT};
        index index.html;
        autoindex off;
    }
}

server {
    listen 127.0.0.1:${PORT};
    server_name limited.localhost;
    root ./www;
    client_max_body_size 1K;
    error_page 413 /errors/413.html;

    location / {
        allowed_methods GET POST;
        root ./www;
        index index.html;
        autoindex off;
    }
}

server {
    listen 127.0.0.1:${SECOND_PORT};
    server_name secondary.localhost;
    root ./www;

    location / {
        allowed_methods GET;
        root ./www;
        index index.html;
        autoindex off;
    }
}
EOF

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

SERVER_COMMAND=(./webserv "$TMP_CONFIG")
if [[ "${LEAK_CHECK:-0}" == "1" ]]; then
    SERVER_COMMAND=(valgrind --leak-check=full --show-leak-kinds=all \
        --errors-for-leak-kinds=definite,indirect --error-exitcode=42 \
        --log-file="$LEAK_LOG" ./webserv "$TMP_CONFIG")
fi
"${SERVER_COMMAND[@]}" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 0.3

status="$(curl -sS -o "$TMP_DIR/index" -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]

grep -q "webserv is running" "$TMP_DIR/index"

status="$(curl -sS -H 'Host: alpha.localhost' -o "$TMP_DIR/alpha" \
    -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
grep -q "alpha virtual host" "$TMP_DIR/alpha"

status="$(curl -sS -H 'Host: beta.localhost' -o "$TMP_DIR/beta" \
    -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
grep -q "beta virtual host" "$TMP_DIR/beta"

status="$(curl -sS -H 'Host: unknown.localhost' -o "$TMP_DIR/fallback" \
    -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
grep -q "webserv is running" "$TMP_DIR/fallback"

head -c 2048 /dev/zero > "$TMP_DIR/too-large.bin"
status="$(curl -sS -H 'Host: limited.localhost' -o "$TMP_DIR/too-large" \
    -w '%{http_code}' -X POST --data-binary @"$TMP_DIR/too-large.bin" \
    "http://127.0.0.1:${PORT}/")"
[[ "$status" == "413" ]]
grep -q "413" "$TMP_DIR/too-large"

status="$(curl -sS -H 'Host: limited.localhost' -o "$TMP_DIR/too-large-chunked" \
    -w '%{http_code}' -X POST -H 'Transfer-Encoding: chunked' \
    --data-binary @"$TMP_DIR/too-large.bin" \
    "http://127.0.0.1:${PORT}/")"
[[ "$status" == "413" ]]
grep -q "413" "$TMP_DIR/too-large-chunked"

status="$(curl -sS -H 'Host: localhost' -o "$TMP_DIR/method-not-allowed" \
    -w '%{http_code}' -X POST --data 'small body' \
    "http://127.0.0.1:${PORT}/")"
[[ "$status" == "405" ]]
grep -q "405" "$TMP_DIR/method-not-allowed"

status="$(curl -sS -o "$TMP_DIR/secondary-index" -w '%{http_code}' \
    "http://127.0.0.1:${SECOND_PORT}/")"
[[ "$status" == "200" ]]
grep -q "webserv is running" "$TMP_DIR/secondary-index"

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

printf 'read-only file\n' > www/uploads/read-only.txt
chmod 444 www/uploads/read-only.txt
read_only_mode="$(stat -c '%a' www/uploads/read-only.txt)"
if [[ "$read_only_mode" == "444" ]]; then
    status="$(curl -sS -o "$TMP_DIR/read-only-delete" -w '%{http_code}' \
        -X DELETE "http://127.0.0.1:${PORT}/uploads/read-only.txt")"
    [[ "$status" == "403" ]]
    grep -q "403" "$TMP_DIR/read-only-delete"
else
    echo "Skipping read-only DELETE check: filesystem mode is ${read_only_mode}"
fi

ln -s index.html www/uploads/delete-link
status="$(curl -sS -o "$TMP_DIR/symlink-delete" -w '%{http_code}' \
    -X DELETE "http://127.0.0.1:${PORT}/uploads/delete-link")"
[[ "$status" == "403" ]]
grep -q "403" "$TMP_DIR/symlink-delete"

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

if [[ "${LEAK_CHECK:-0}" == "1" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    grep -q "definitely lost: 0 bytes" "$LEAK_LOG"
    grep -q "indirectly lost: 0 bytes" "$LEAK_LOG"
    grep -q "ERROR SUMMARY: 0 errors" "$LEAK_LOG"
fi

echo "All smoke tests passed on port ${PORT}."
