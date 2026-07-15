#!/bin/bash

# Script de pruebas para CGI Routing y Multiple Virtual Hosts
# Uso: bash test_multivhost.sh

BASE_URL="http://localhost"
PORT1=8080
PORT2=8081
PORT3=8082

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Testing Multiple Virtual Hosts + CGI${NC}"
echo -e "${YELLOW}========================================${NC}\n"

# Test 1: Virtual Host 1 - Main site
echo -e "${YELLOW}[TEST 1] Virtual Host 1 (Port $PORT1) - GET /index.html${NC}"
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" "http://localhost:$PORT1/"
echo ""

# Test 2: Virtual Host 2 - API site
echo -e "${YELLOW}[TEST 2] Virtual Host 2 (Port $PORT2) - GET /index.html${NC}"
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" "http://localhost:$PORT2/"
echo ""

# Test 3: Virtual Host 3 - Admin site
echo -e "${YELLOW}[TEST 3] Virtual Host 3 (Port $PORT3) - GET /index.html${NC}"
curl -s -o /dev/null -w "HTTP Status: %{http_code}\n" "http://localhost:$PORT3/"
echo ""

# Test 4: CGI - Python script
echo -e "${YELLOW}[TEST 4] CGI - Python Script (Port $PORT1)${NC}"
echo "Response:"
curl -s "http://localhost:$PORT1/cgi/test.py" | head -5
echo ""

# Test 5: CGI - Bash script
echo -e "${YELLOW}[TEST 5] CGI - Bash Script (Port $PORT1)${NC}"
echo "Response:"
curl -s "http://localhost:$PORT1/cgi/test.sh" | head -5
echo ""

# Test 6: Autoindex
echo -e "${YELLOW}[TEST 6] Autoindex (Port $PORT1)${NC}"
echo "Response (first 10 lines):"
curl -s "http://localhost:$PORT1/" | head -10
echo ""

# Test 7: Upload file
echo -e "${YELLOW}[TEST 7] File Upload (Port $PORT1)${NC}"
echo "test content" > /tmp/test_upload.txt
curl -s -X POST --data-binary @/tmp/test_upload.txt "http://localhost:$PORT1/upload/"
echo ""

# Test 8: POST to CGI
echo -e "${YELLOW}[TEST 8] POST to CGI Script${NC}"
curl -s -X POST -d "name=test" "http://localhost:$PORT1/cgi/test.py" | head -5
echo ""

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Tests completed!${NC}"
echo -e "${GREEN}========================================${NC}"
