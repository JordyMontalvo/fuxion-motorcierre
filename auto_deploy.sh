#!/bin/bash
# Auto-deploy script: revisa GitHub y actualiza si hay cambios

REPO_DIR="$HOME/fuxion-repo"
LOG="$HOME/auto_deploy.log"

cd "$REPO_DIR" || exit 1

# Obtener el hash actual y el remoto
LOCAL=$(git rev-parse HEAD)
git fetch origin master >> "$LOG" 2>&1
REMOTE=$(git rev-parse origin/master)

if [ "$LOCAL" != "$REMOTE" ]; then
    echo "$(date): 🔄 Cambios detectados en GitHub. Actualizando..." >> "$LOG"
    git pull origin master >> "$LOG" 2>&1
    npm install --silent >> "$LOG" 2>&1
    
    # Recompilar solo si los archivos C++ cambiaron
    if git diff HEAD@{1} --name-only | grep -q "\.cpp$"; then
        echo "$(date): ⚙️ Cambios en C++. Recompilando motor..." >> "$LOG"
        g++ -std=c++17 cierre_fuxion.cpp -o cierre_fuxion $(pkg-config --cflags --libs libmongocxx libbsoncxx) >> "$LOG" 2>&1
        g++ -std=c++17 mongo_benchmark.cpp -o mongo_benchmark $(pkg-config --cflags --libs libmongocxx libbsoncxx) >> "$LOG" 2>&1
    fi

    pm2 restart fuxion-dashboard >> "$LOG" 2>&1
    echo "$(date): ✅ Deploy completado." >> "$LOG"
else
    echo "$(date): ✔ Sin cambios." >> "$LOG"
fi
