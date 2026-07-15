# Script de pruebas para CGI Routing y Multiple Virtual Hosts
# Uso: powershell -ExecutionPolicy Bypass -File test_multivhost.ps1

$PORT1 = 8080
$PORT2 = 8081
$PORT3 = 8082

function Test-Server {
    param(
        [string]$Name,
        [string]$Url,
        [string]$Port
    )
    
    try {
        $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -ErrorAction SilentlyContinue
        Write-Host "[✓] $Name (Port $Port): HTTP $($response.StatusCode)" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Host "[✗] $Name (Port $Port): Connection failed" -ForegroundColor Red
        return $false
    }
}

function Show-Response {
    param(
        [string]$Title,
        [string]$Url,
        [int]$Lines = 5
    )
    
    Write-Host "`n$Title" -ForegroundColor Yellow
    try {
        $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -ErrorAction SilentlyContinue
        $content = $response.Content -split "`n" | Select-Object -First $Lines
        Write-Host $content
    }
    catch {
        Write-Host "Error: $_" -ForegroundColor Red
    }
}

Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Testing Multiple Virtual Hosts + CGI" -ForegroundColor Yellow
Write-Host "========================================`n" -ForegroundColor Yellow

# Test 1: Virtual Host 1
Write-Host "[TEST 1] Virtual Host 1 (Port $PORT1)" -ForegroundColor Cyan
Test-Server "Main Site" "http://localhost:$PORT1/" $PORT1

# Test 2: Virtual Host 2
Write-Host "`n[TEST 2] Virtual Host 2 (Port $PORT2)" -ForegroundColor Cyan
Test-Server "API Site" "http://localhost:$PORT2/" $PORT2

# Test 3: Virtual Host 3
Write-Host "`n[TEST 3] Virtual Host 3 (Port $PORT3)" -ForegroundColor Cyan
Test-Server "Admin Site" "http://localhost:$PORT3/" $PORT3

# Test 4: CGI - Python
Show-Response "[TEST 4] CGI - Python Script (Port $PORT1)" "http://localhost:$PORT1/cgi/test.py"

# Test 5: CGI - Bash
Show-Response "[TEST 5] CGI - Bash Script (Port $PORT1)" "http://localhost:$PORT1/cgi/test.sh"

# Test 6: Autoindex
Show-Response "[TEST 6] Directory Listing (Port $PORT1)" "http://localhost:$PORT1/" 10

# Test 7: Query String
Show-Response "[TEST 7] GET with Query String" "http://localhost:$PORT1/cgi/test.py?param1=value1&param2=value2"

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "Tests completed!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green

Write-Host "Notes:" -ForegroundColor Yellow
Write-Host "- Port $PORT1: Virtual Host 1 (Main Site)"  -ForegroundColor White
Write-Host "- Port $PORT2: Virtual Host 2 (API Site)"  -ForegroundColor White
Write-Host "- Port $PORT3: Virtual Host 3 (Admin Site)"  -ForegroundColor White
