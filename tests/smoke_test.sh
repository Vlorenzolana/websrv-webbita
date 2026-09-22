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
              "$ROOT_DIR/www2/uploads/delete-link" \
          "$ROOT_DIR/www/cgi-bin/fail.py" \
          "$ROOT_DIR/www/cgi-bin/slow.py"
}
trap cleanup EXIT

cd "$ROOT_DIR"
# Build the server and prepare temporary virtual-host document roots.
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

# Start the server, optionally under Valgrind for leak detection.
SERVER_COMMAND=(./webserv "$TMP_CONFIG")
if [[ "${LEAK_CHECK:-0}" == "1" ]]; then
    SERVER_COMMAND=(valgrind --leak-check=full --show-leak-kinds=all \
        --errors-for-leak-kinds=definite,indirect --error-exitcode=42 \
        --log-file="$LEAK_LOG" ./webserv "$TMP_CONFIG")
fi
"${SERVER_COMMAND[@]}" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 0.3

# Serve the default virtual host's index page.
status="$(curl -sS -o "$TMP_DIR/index" -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]

grep -q "Webserv Evaluation Tester - 42 Urduliz" "$TMP_DIR/index"

# Route requests to the correct virtual host by Host header.
status="$(curl -sS -H 'Host: alpha.localhost' -o "$TMP_DIR/alpha" \
    -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
grep -q "alpha virtual host" "$TMP_DIR/alpha"

status="$(curl -sS -H 'Host: beta.localhost' -o "$TMP_DIR/beta" \
    -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
grep -q "beta virtual host" "$TMP_DIR/beta"

# Fall back to the default server for an unknown virtual host.
status="$(curl -sS -H 'Host: unknown.localhost' -o "$TMP_DIR/fallback" \
    -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
grep -q "Webserv Evaluation Tester - 42 Urduliz" "$TMP_DIR/fallback"

# Reject oversized fixed-length and chunked request bodies.
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

# Reject a method that is not allowed by the location configuration.
status="$(curl -sS -H 'Host: localhost' -o "$TMP_DIR/method-not-allowed" \
    -w '%{http_code}' -X POST --data 'small body' \
    "http://127.0.0.1:${PORT}/")"
[[ "$status" == "405" ]]
grep -q "405" "$TMP_DIR/method-not-allowed"

# Serve a virtual host listening on a second port.
status="$(curl -sS -o "$TMP_DIR/secondary-index" -w '%{http_code}' \
    "http://127.0.0.1:${SECOND_PORT}/")"
[[ "$status" == "200" ]]
grep -q "Webserv Evaluation Tester - 42 Urduliz" "$TMP_DIR/secondary-index"

# Return the configured custom error page for a missing resource.
status="$(curl -sS -o "$TMP_DIR/missing" -w '%{http_code}' "http://127.0.0.1:${PORT}/missing")"
[[ "$status" == "404" ]]
grep -q "Custom 404" "$TMP_DIR/missing"

# Preserve binary data during a regular POST upload.
printf 'ABC\0XYZ' > "$TMP_DIR/raw.bin"
status="$(curl -sS -o /dev/null -w '%{http_code}' -X POST \
    --data-binary @"$TMP_DIR/raw.bin" \
    "http://127.0.0.1:${PORT}/uploads/raw.bin")"
[[ "$status" == "201" ]]
cmp "$TMP_DIR/raw.bin" www/uploads/raw.bin

# Accept multipart form uploads and store the uploaded file.
printf 'multipart-data' > "$TMP_DIR/form.txt"
status="$(curl -sS -o /dev/null -w '%{http_code}' \
    -F "file=@$TMP_DIR/form.txt" \
    "http://127.0.0.1:${PORT}/uploads/")"
[[ "$status" == "201" ]]
cmp "$TMP_DIR/form.txt" www/uploads/form.txt

# Reject DELETE requests that target a symbolic link.
ln -s index.html www2/uploads/delete-link
status="$(curl -sS -o "$TMP_DIR/symlink-delete" -w '%{http_code}' \
    -H 'Host: localhost2' -X DELETE \
    "http://127.0.0.1:${PORT}/uploads/delete-link")"
[[ "$status" == "403" ]]
grep -q "403" "$TMP_DIR/symlink-delete"

# Pass a large POST body through a CGI script without truncation.
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

# Convert a CGI script failure into an HTTP 500 response.
status="$(curl -sS -o /dev/null -w '%{http_code}' \
    "http://127.0.0.1:${PORT}/cgi-bin/fail.py")"
[[ "$status" == "500" ]]

# Decode a slowly sent chunked request body and save its contents.
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

# Keep serving other requests while a CGI request exceeds its timeout.
curl -sS -o "$TMP_DIR/slow" -w '%{http_code}' \
    "http://127.0.0.1:${PORT}/cgi-bin/slow.py" > "$TMP_DIR/slow-code" &
SLOW_CURL_PID=$!
sleep 0.5
status="$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:${PORT}/")"
[[ "$status" == "200" ]]
wait "$SLOW_CURL_PID"
[[ "$(cat "$TMP_DIR/slow-code")" == "504" ]]

# Handle multiple simultaneous requests successfully.
seq 1 50 | xargs -P10 -I{} sh -c \
    "test \"\$(curl -sS -o /dev/null -w '%{http_code}' http://127.0.0.1:${PORT}/)\" = 200"

if ps -o stat= --ppid "$SERVER_PID" | grep -q Z; then
    echo "Zombie CGI process detected" >&2
    exit 1
fi

# Confirm that CGI cleanup and allocations are clean under Valgrind.
if [[ "${LEAK_CHECK:-0}" == "1" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    grep -q "definitely lost: 0 bytes" "$LEAK_LOG"
    grep -q "indirectly lost: 0 bytes" "$LEAK_LOG"
    grep -q "ERROR SUMMARY: 0 errors" "$LEAK_LOG"
fi

echo "All smoke tests passed on port ${PORT}."