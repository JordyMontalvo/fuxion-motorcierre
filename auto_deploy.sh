#!/bin/bash
# Auto-deploy script: PostgreSQL + Ltree Edition
# Este script es ejecutado por el cron o el webhook y actualiza el motor.

REPO_DIR="$HOME/fuxion-motorcierre"
LOG="$HOME/auto_deploy.log"

cd "$REPO_DIR" || exit 1

# 1. Obtener Cambios
LOCAL=$(git rev-parse HEAD)
git fetch origin master >> "$LOG" 2>&1
REMOTE=$(git rev-parse origin/master)

if [ "$LOCAL" != "$REMOTE" ]; then
    echo "$(date): 🔄 Cambios detectados. Actualizando repositorio..." >> "$LOG"
    git pull origin master >> "$LOG" 2>&1
    
    # 2. Reinstalar dependencias de Node si cambiaron (incluyendo 'pg')
    npm install --silent >> "$LOG" 2>&1
    
    # 3. Compilar Motor Postgres C++
    echo "$(date): ⚙️ Compilando nuevo Motor Postgres..." >> "$LOG"
    g++ -std=c++17 cierre_postgres.cpp -o cierre_postgres -I/usr/include/postgresql -lpq >> "$LOG" 2>&1
    
    # 4. Aplicar Cambios de Schema si existen (opcional)
    if [ -f "schema_fuxion.sql" ]; then
        echo "$(date): 🐘 Aplicando migraciones de Postgres..." >> "$LOG"
        psql -d fuxion_db -f schema_fuxion.sql >> "$LOG" 2>&1
    fi

    # 5. Reiniciar con PM2
    pm2 restart fuxion-dashboard >> "$LOG" 2>&1
    echo "$(date): ✅ Deploy completado exitosamente." >> "$LOG"
else
    echo "$(date): ✔ Sin cambios en el repositorio." >> "$LOG"
fi
