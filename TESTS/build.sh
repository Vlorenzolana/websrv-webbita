#!/bin/bash
# Build script - delegates to Makefile
# Usage: bash build.sh [make_target]
# Default target: all (compile everything)

set -e

MAKE_TARGET=${1:-all}

echo "=========================================="
echo "WebServ Build System"
echo "=========================================="
echo ""
echo "Make target: $MAKE_TARGET"
echo ""

# Detectar compilador disponible
if command -v g++ &> /dev/null; then
    echo "[✓] Compiler: g++"
    export CXX=g++
elif command -v clang++ &> /dev/null; then
    echo "[✓] Compiler: clang++"
    export CXX=clang++
else
    echo "[✗] No C++ compiler found!"
    echo "Install: g++ (apt install g++) or clang++ (apt install clang)"
    exit 1
fi

echo ""

# Deleguar a Makefile
if [ -f "Makefile" ]; then
    make $MAKE_TARGET
else
    echo "[✗] Makefile not found!"
    exit 1
fi

echo ""
echo "=========================================="
if [ "$MAKE_TARGET" = "all" ] || [ "$MAKE_TARGET" = "" ]; then
    echo "Build Targets Available:"
    echo "  make              - compile (default)"
    echo "  make clean        - remove object files"
    echo "  make fclean       - remove all (objects + binary)"
    echo "  make re           - full rebuild"
    echo ""
    if [ -f "webserv" ]; then
        echo "Usage:"
        echo "  ./webserv config/multivhost.conf"
        echo ""
        echo "Testing:"
        echo "  bash test_multivhost.sh"
    fi
fi
echo "=========================================="
