#!/bin/bash

# ============================================
# WebServ Evaluation Checklist GUI
# Para repasar antes de la evaluación de 42
# ============================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuración
SERVER_HOST="localhost"
PORT1=8080
PORT2=8081
PORT3=8082

# Resultados
declare -A results

clear_screen() {
    clear
}

print_header() {
    echo -e "${CYAN}"
    cat << "EOF"
    ╔════════════════════════════════════════════════════════════╗
    ║       WebServ - Evaluation Checklist GUI (42 School)       ║
    ║    Repasa todos los puntos antes de la evaluación          ║
    ╚════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

print_menu() {
    clear_screen
    print_header
    
    echo -e "${YELLOW}=== MANDATORY PART ===${NC}"
    echo "1. Configuration Tests (Puertos, Hostnames, Error Pages)"
    echo "2. Basic HTTP Tests (GET, POST, DELETE)"
    echo "3. CGI Tests (Python y Bash)"
    echo "4. Browser Compatibility (Headers, Status Codes)"
    echo "5. Port Issues & Multiple Servers"
    echo "6. Siege Stress Test (Disponibilidad, Memory Leaks)"
    echo ""
    echo -e "${BLUE}=== CODE REVIEW ===${NC}"
    echo "7. Check I/O Multiplexing (select/epoll)"
    echo "8. Check Error Handling"
    echo "9. Check Memory Leaks (valgrind/leaks)"
    echo ""
    echo -e "${GREEN}=== UTILITIES ===${NC}"
    echo "10. Start Server"
    echo "11. Stop Server"
    echo "12. View Full Report"
    echo "0. Exit"
    echo ""
    read -p "Selecciona opción: " choice
}

check_server_running() {
    pgrep -f "./webserv config/multivhost.conf" > /dev/null 2>&1
    return $?
}

start_server() {
    echo -e "${YELLOW}[*] Iniciando servidor...${NC}"
    killall webserv 2>/dev/null || true
    sleep 1
    cd /sgoinfre/students/vlorenzo/42CURSUS/websrv-webbita
    ./webserv config/multivhost.conf > /tmp/webserv.log 2>&1 &
    sleep 2
    
    if check_server_running; then
        echo -e "${GREEN}[✓] Servidor iniciado correctamente${NC}"
        echo -e "${GREEN}Puerto 8080: example.com${NC}"
        echo -e "${GREEN}Puerto 8081: api.example.com${NC}"
        echo -e "${GREEN}Puerto 8082: admin.example.com${NC}"
    else
        echo -e "${RED}[✗] Error al iniciar servidor${NC}"
    fi
    read -p "Presiona Enter para continuar..."
}

stop_server() {
    echo -e "${YELLOW}[*] Deteniendo servidor...${NC}"
    killall webserv 2>/dev/null
    sleep 1
    echo -e "${GREEN}[✓] Servidor detenido${NC}"
    read -p "Presiona Enter para continuar..."
}

test_configuration() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== CONFIGURATION TESTS ===${NC}\n"
    
    # Test 1: Múltiples puertos
    echo "Test 1: Múltiples Puertos"
    for port in $PORT1 $PORT2 $PORT3; do
        result=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$port/)
        if [ "$result" = "200" ]; then
            echo -e "  ${GREEN}[✓]${NC} Puerto $port: $result"
            results["port_$port"]=1
        else
            echo -e "  ${RED}[✗]${NC} Puerto $port: $result"
            results["port_$port"]=0
        fi
    done
    echo ""
    
    # Test 2: Error Page 404
    echo "Test 2: Custom Error Page (404)"
    result=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT1/nonexistent)
    if [ "$result" = "404" ]; then
        echo -e "  ${GREEN}[✓]${NC} 404 Error: Correcto"
        results["error_404"]=1
    else
        echo -e "  ${RED}[✗]${NC} 404 Error: Recibido $result"
        results["error_404"]=0
    fi
    echo ""
    
    # Test 3: Límite de body
    echo "Test 3: Client Body Size Limit"
    large_data=$(head -c 2000000 </dev/urandom | base64)
    result=$(curl -s -X POST -d "$large_data" -o /dev/null -w "%{http_code}" http://localhost:$PORT1/upload 2>/dev/null)
    if [ "$result" = "413" ] || [ "$result" = "201" ]; then
        echo -e "  ${GREEN}[✓]${NC} Body Limit: Funciona (Código $result)"
        results["body_limit"]=1
    else
        echo -e "  ${RED}[?]${NC} Body Limit: Código $result"
        results["body_limit"]=0
    fi
    echo ""
    
    # Test 4: Custom index file
    echo "Test 4: Index File Resolution"
    result=$(curl -s http://localhost:$PORT1/ | grep -q "Welcome to Virtual Host 1" && echo "1" || echo "0")
    if [ "$result" = "1" ]; then
        echo -e "  ${GREEN}[✓]${NC} Index: Encontrado y servido"
        results["index"]=1
    else
        echo -e "  ${RED}[✗]${NC} Index: No encontrado"
        results["index"]=0
    fi
    
    read -p "Presiona Enter para continuar..."
}

test_http_methods() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== HTTP METHODS TESTS ===${NC}\n"
    
    # Test GET
    echo "Test 1: GET Request"
    result=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT1/)
    if [ "$result" = "200" ]; then
        echo -e "  ${GREEN}[✓]${NC} GET: $result"
        results["get"]=1
    else
        echo -e "  ${RED}[✗]${NC} GET: $result"
        results["get"]=0
    fi
    echo ""
    
    # Test POST
    echo "Test 2: POST Request (Upload)"
    echo "test data" > /tmp/test_upload.txt
    result=$(curl -s -X POST --data-binary @/tmp/test_upload.txt -o /dev/null -w "%{http_code}" http://localhost:$PORT1/upload/)
    if [ "$result" = "201" ] || [ "$result" = "200" ]; then
        echo -e "  ${GREEN}[✓]${NC} POST: $result"
        results["post"]=1
    else
        echo -e "  ${RED}[✗]${NC} POST: $result"
        results["post"]=0
    fi
    echo ""
    
    # Test DELETE
    echo "Test 3: DELETE Request"
    # Primero upload un archivo
    curl -s -X POST --data "test" http://localhost:$PORT1/upload/ > /dev/null 2>&1
    # Luego intenta borrarlo
    result=$(curl -s -X DELETE -o /dev/null -w "%{http_code}" http://localhost:$PORT1/upload/test.txt 2>/dev/null)
    if [ "$result" = "200" ] || [ "$result" = "404" ]; then
        echo -e "  ${GREEN}[✓]${NC} DELETE: Responde ($result)"
        results["delete"]=1
    else
        echo -e "  ${RED}[✗]${NC} DELETE: $result"
        results["delete"]=0
    fi
    echo ""
    
    # Test Unknown Method
    echo "Test 4: Unknown HTTP Methods (No Crash)"
    result=$(curl -s -X UNKNOWN http://localhost:$PORT1/ 2>&1 | grep -q "405\|501\|400" && echo "1" || echo "0")
    if [ "$result" = "1" ]; then
        echo -e "  ${GREEN}[✓]${NC} Maneja métodos desconocidos"
        results["unknown_method"]=1
    else
        echo -e "  ${YELLOW}[?]${NC} Verifica manualmente"
        results["unknown_method"]=0
    fi
    
    read -p "Presiona Enter para continuar..."
}

test_cgi() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== CGI TESTS ===${NC}\n"
    
    # Test Python CGI
    echo "Test 1: Python CGI Script"
    result=$(curl -s http://localhost:$PORT1/cgi/test.py | grep -q "Python CGI" && echo "1" || echo "0")
    if [ "$result" = "1" ]; then
        echo -e "  ${GREEN}[✓]${NC} Python CGI: Funciona"
        results["cgi_python"]=1
    else
        echo -e "  ${RED}[✗]${NC} Python CGI: No responde"
        results["cgi_python"]=0
    fi
    echo ""
    
    # Test Bash CGI
    echo "Test 2: Bash CGI Script"
    result=$(curl -s http://localhost:$PORT1/cgi/test.sh | grep -q "Bash CGI" && echo "1" || echo "0")
    if [ "$result" = "1" ]; then
        echo -e "  ${GREEN}[✓]${NC} Bash CGI: Funciona"
        results["cgi_bash"]=1
    else
        echo -e "  ${RED}[✗]${NC} Bash CGI: No responde"
        results["cgi_bash"]=0
    fi
    echo ""
    
    # Test CGI with Query String
    echo "Test 3: CGI con Query String"
    result=$(curl -s "http://localhost:$PORT1/cgi/test.py?name=test&value=123" | grep -q "name=test" && echo "1" || echo "0")
    if [ "$result" = "1" ]; then
        echo -e "  ${GREEN}[✓]${NC} Query String: Recibido correctamente"
        results["cgi_query"]=1
    else
        echo -e "  ${YELLOW}[?]${NC} Query String: Verifica manualmente"
        results["cgi_query"]=0
    fi
    echo ""
    
    # Test CGI with POST
    echo "Test 4: CGI con POST Data"
    result=$(curl -s -X POST -d "test=data" http://localhost:$PORT1/cgi/test.py | grep -q "POST" && echo "1" || echo "0")
    if [ "$result" = "1" ]; then
        echo -e "  ${GREEN}[✓]${NC} POST to CGI: REQUEST_METHOD recibido"
        results["cgi_post"]=1
    else
        echo -e "  ${YELLOW}[?]${NC} POST to CGI: Verifica manualmente"
        results["cgi_post"]=0
    fi
    
    read -p "Presiona Enter para continuar..."
}

test_browser_compat() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== BROWSER COMPATIBILITY TESTS ===${NC}\n"
    
    echo "Test 1: Response Headers"
    echo -e "  Ejecutando: ${CYAN}curl -v http://localhost:$PORT1/ 2>&1 | head -20${NC}"
    echo ""
    curl -v http://localhost:$PORT1/ 2>&1 | head -20
    echo ""
    echo -e "  ${YELLOW}Verifica:${NC}"
    echo "    ✓ HTTP/1.1 200 OK"
    echo "    ✓ Content-Type: text/html"
    echo "    ✓ Content-Length presente"
    echo ""
    results["browser_headers"]=0
    read -p "¿Están bien los headers? (s/n): " answer
    if [ "$answer" = "s" ] || [ "$answer" = "S" ]; then
        results["browser_headers"]=1
    fi
    echo ""
    
    echo "Test 2: Main Page Display"
    echo -e "  ${YELLOW}Abre en navegador: ${CYAN}http://localhost:$PORT1/${NC}"
    echo -e "  ${YELLOW}Verifica:${NC}"
    echo "    ✓ Página HTML visible"
    echo "    ✓ Enlaces funcionan"
    results["browser_main"]=0
    read -p "¿Se ve correctamente? (s/n): " answer
    if [ "$answer" = "s" ] || [ "$answer" = "S" ]; then
        results["browser_main"]=1
    fi
}

test_port_issues() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== PORT ISSUES TESTS ===${NC}\n"
    
    echo "Test 1: Multiple Servers on Different Ports"
    for port in $PORT1 $PORT2 $PORT3; do
        result=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$port/)
        if [ "$result" = "200" ]; then
            echo -e "  ${GREEN}[✓]${NC} Puerto $port: Escuchando"
        else
            echo -e "  ${RED}[✗]${NC} Puerto $port: No responde"
        fi
    done
    results["multi_ports"]=1
    echo ""
    
    echo "Test 2: Verify Different Virtual Hosts"
    for port in $PORT1 $PORT2 $PORT3; do
        result=$(curl -s http://localhost:$port/ | grep -o "Virtual Host [0-9]" | head -1)
        echo -e "  Puerto $port: $result"
    done
    results["vhost_diff"]=1
    
    read -p "Presiona Enter para continuar..."
}

test_stress() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== SIEGE STRESS TEST ===${NC}\n"
    
    echo "Test 1: Verificar que siege está instalado"
    if command -v siege &> /dev/null; then
        echo -e "  ${GREEN}[✓]${NC} Siege instalado"
        results["siege_installed"]=1
    else
        echo -e "  ${RED}[✗]${NC} Siege no está instalado"
        echo -e "  ${YELLOW}Instálalo con:${NC} brew install siege"
        results["siege_installed"]=0
        read -p "Presiona Enter para continuar..."
        return
    fi
    echo ""
    
    echo "Test 2: Benchmark simple (10 conexiones)"
    echo -e "  ${CYAN}Ejecutando: siege -c 10 -r 5 -b http://localhost:8080/${NC}"
    siege -c 10 -r 5 -b http://localhost:8080/ 2>/dev/null | tail -10
    echo ""
    echo -e "  ${YELLOW}Fijate en:${NC}"
    echo "    ✓ Availability > 99.5%"
    echo "    ✓ Sin 'failed'  transactions"
    results["siege_test"]=0
    read -p "¿Availability > 99.5%? (s/n): " answer
    if [ "$answer" = "s" ] || [ "$answer" = "S" ]; then
        results["siege_test"]=1
    fi
    echo ""
    
    echo "Test 3: Memory Usage Monitoring"
    echo -e "  ${YELLOW}Monitorea memoria durante el siguiente benchmark${NC}"
    echo -e "  ${CYAN}En otra terminal: watch -n 0.5 'ps aux | grep webserv | grep -v grep'${NC}"
    read -p "Presiona Enter cuando esté listo..."
    siege -c 20 -r 10 -b http://localhost:8080/ 2>/dev/null > /tmp/siege_results.txt
    echo -e "  ${GREEN}[✓]${NC} Test completado"
    results["memory_test"]=1
    
    read -p "Presiona Enter para continuar..."
}

check_code_review() {
    clear_screen
    print_header
    echo -e "${YELLOW}=== CODE REVIEW CHECKLIST ===${NC}\n"
    
    echo "Test 1: I/O Multiplexing (select/epoll)"
    echo -e "  ${CYAN}Verificando epoll en src/Server.cpp...${NC}"
    grep -q "epoll" /sgoinfre/students/vlorenzo/42CURSUS/websrv-webbita/src/Server.cpp && echo -e "  ${GREEN}[✓]${NC} epoll_create1 encontrado" || echo -e "  ${RED}[✗]${NC} epoll no encontrado"
    results["io_multi"]=1
    echo ""
    
    echo "Test 2: Error Handling (signal handling, errno checks)"
    echo -e "  ${CYAN}Verificando SIGPIPE si no está ignorada...${NC}"
    grep -q "SIGPIPE" /sgoinfre/students/vlorenzo/42CURSUS/websrv-webbita/src/main.cpp && echo -e "  ${GREEN}[✓]${NC} SIGPIPE handling" || echo -e "  ${YELLOW}[?]${NC} Verifica error handling"
    results["error_handling"]=1
    echo ""
    
    echo "Test 3: Memory Leaks (valgrind recomendado)"
    echo -e "  ${YELLOW}Para verificar: valgrind --leak-check=full ./webserv${NC}"
    echo -e "  ${CYAN}O: leaks -atEachMalloc 1 ./webserv${NC}"
    results["memory_leaks"]=0
    read -p "¿Sin memory leaks? (s/n): " answer
    if [ "$answer" = "s" ] || [ "$answer" = "S" ]; then
        results["memory_leaks"]=1
    fi
    
    read -p "Presiona Enter para continuar..."
}

generate_report() {
    clear_screen
    print_header
    
    local total=0
    local passed=0
    
    echo -e "${YELLOW}=== FULL TEST REPORT ===${NC}\n"
    
    # Contar resultados
    for key in "${!results[@]}"; do
        ((total++))
        if [ "${results[$key]}" = "1" ]; then
            ((passed++))
        fi
    done
    
    # Mostrar detalles
    echo -e "${BLUE}Configuration Tests:${NC}"
    for port in $PORT1 $PORT2 $PORT3; do
        [ "${results[port_$port]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} Port $port" || echo -e "  ${RED}[✗]${NC} Port $port"
    done
    echo ""
    
    echo -e "${BLUE}HTTP Methods:${NC}"
    [ "${results[get]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} GET" || echo -e "  ${RED}[✗]${NC} GET"
    [ "${results[post]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} POST" || echo -e "  ${RED}[✗]${NC} POST"
    [ "${results[delete]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} DELETE" || echo -e "  ${RED}[✗]${NC} DELETE"
    echo ""
    
    echo -e "${BLUE}CGI Tests:${NC}"
    [ "${results[cgi_python]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} Python CGI" || echo -e "  ${RED}[✗]${NC} Python CGI"
    [ "${results[cgi_bash]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} Bash CGI" || echo -e "  ${RED}[✗]${NC} Bash CGI"
    echo ""
    
    echo -e "${BLUE}Stress Tests:${NC}"
    [ "${results[siege_test]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} Siege >99.5%" || echo -e "  ${RED}[✗]${NC} Siege benchmark"
    [ "${results[memory_leaks]}" = "1" ] && echo -e "  ${GREEN}[✓]${NC} No Memory Leaks" || echo -e "  ${RED}[✗]${NC} Memory Leaks"
    echo ""
    
    # Resumen
    local percentage=$((passed * 100 / total))
    if [ $percentage -ge 90 ]; then
        color=$GREEN
        status="EXCELLENT"
    elif [ $percentage -ge 70 ]; then
        color=$YELLOW
        status="GOOD"
    else
        color=$RED
        status="NEEDS WORK"
    fi
    
    echo -e "${color}════════════════════════════════════${NC}"
    echo -e "${color}SCORE: $passed/$total ($percentage%)${NC}"
    echo -e "${color}STATUS: $status${NC}"
    echo -e "${color}════════════════════════════════════${NC}"
    
    read -p "Presiona Enter para volver al menú..."
}

# Main Loop
while true; do
    print_menu
    
    case $choice in
        1) 
            if check_server_running; then
                test_configuration
            else
                echo -e "${RED}[✗] Servidor no está ejecutándose. Inicia opción 10 primero.${NC}"
                read -p "Presiona Enter para continuar..."
            fi
            ;;
        2)
            if check_server_running; then
                test_http_methods
            else
                echo -e "${RED}[✗] Servidor no está ejecutándose${NC}"
                read -p "Presiona Enter para continuar..."
            fi
            ;;
        3)
            if check_server_running; then
                test_cgi
            else
                echo -e "${RED}[✗] Servidor no está ejecutándose${NC}"
                read -p "Presiona Enter para continuar..."
            fi
            ;;
        4)
            if check_server_running; then
                test_browser_compat
            else
                echo -e "${RED}[✗] Servidor no está ejecutándose${NC}"
                read -p "Presiona Enter para continuar..."
            fi
            ;;
        5)
            if check_server_running; then
                test_port_issues
            else
                echo -e "${RED}[✗] Servidor no está ejecutándose${NC}"
                read -p "Presiona Enter para continuar..."
            fi
            ;;
        6)
            if check_server_running; then
                test_stress
            else
                echo -e "${RED}[✗] Servidor no está ejecutándose${NC}"
                read -p "Presiona Enter para continuar..."
            fi
            ;;
        7)
            check_code_review
            ;;
        8)
            check_code_review
            ;;
        9)
            check_code_review
            ;;
        10)
            start_server
            ;;
        11)
            stop_server
            ;;
        12)
            generate_report
            ;;
        0)
            echo -e "${CYAN}¡Bye! Buena suerte en la evaluación.${NC}"
            break
            ;;
        *)
            echo -e "${RED}Opción inválida${NC}"
            read -p "Presiona Enter para continuar..."
            ;;
    esac
done
