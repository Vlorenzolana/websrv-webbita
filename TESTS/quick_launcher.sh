#!/bin/bash

# ============================================
# WebServ Quick Launcher
# Acceso rápido a pruebas y GUI
# ============================================

CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

cd "$(dirname "$0")"

echo -e "${CYAN}"
cat << "EOF"
╔════════════════════════════════════════════╗
║   WebServ - Quick Launcher                 ║
║   Pre-Evaluation Review Tool               ║
╚════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo "¿Qué quieres hacer?"
echo ""
echo "1. 🧪 Ejecutar Tests Automáticos (rápido)"
echo "2. 🎮 Abrir Evaluation GUI (interactivo)"
echo "3. 📋 Ver Cheat Sheet (referencia)"
echo "4. 📚 Ver Documentación Completa"
echo "5. ▶️  Iniciar Servidor"
echo "6. ⏹️  Detener Servidor"
echo "0. ❌ Salir"
echo ""
read -p "Selecciona (0-6): " choice

case $choice in
    1)
        echo -e "${YELLOW}[*] Ejecutando tests automáticos...${NC}\n"
        bash test_multivhost.sh
        ;;
    2)
        echo -e "${YELLOW}[*] Lanzando Evaluation GUI...${NC}\n"
        bash evaluation_gui.sh
        ;;
    3)
        echo -e "${YELLOW}[*] Mostrando Cheat Sheet...${NC}\n"
        less CHEATSHEET.md
        ;;
    4)
        echo -e "${YELLOW}[*] Documentación disponible:${NC}\n"
        echo "1. README.md                      - Overview"
        echo "2. TESTING_GUIDE.md               - Guía básica" 
        echo "3. TESTING_COMPLETE_GUIDE.md      - Guía exhaustiva"
        echo "4. EVALUATION_GUI_README.md       - Cómo usar GUI"
        echo "5. CHEATSHEET.md                  - Quick reference"
        echo ""
        read -p "¿Cuál quieres leer? (1-5): " doc
        case $doc in
            1) less README.md ;;
            2) less TESTING_GUIDE.md ;;
            3) less TESTING_COMPLETE_GUIDE.md ;;
            4) less EVALUATION_GUI_README.md ;;
            5) less CHEATSHEET.md ;;
            *) echo "Opción inválida" ;;
        esac
        ;;
    5)
        echo -e "${YELLOW}[*] Iniciando servidor...${NC}"
        cd ..
        ./webserv config/multivhost.conf
        ;;
    6)
        echo -e "${YELLOW}[*] Deteniendo servidor...${NC}"
        killall webserv 2>/dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}[✓] Servidor detenido${NC}"
        else
            echo -e "${YELLOW}[!] No hay servidor ejecutándose${NC}"
        fi
        ;;
    0)
        echo -e "${CYAN}¡Bye! Buena suerte.${NC}"
        ;;
    *)
        echo "Opción inválida"
        ;;
esac
