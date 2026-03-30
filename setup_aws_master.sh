#!/bin/bash
set -e

echo "🚀 Iniciando configuración maestra de Fuxion en AWS (PostgreSQL + Ltree Edition)..."

# 1. Actualización y Dependencias base (Incluye Postgres Client y LibPQ para C++)
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake git libssl-dev pkg-config curl \
    postgresql postgresql-contrib libpq-dev

# 2. Instalación de Node.js 20 si no existe
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y nodejs
fi

# 3. Configuración de PostgreSQL Local (Opcional si usas RDS, se instala por si acaso)
sudo systemctl start postgresql
sudo systemctl enable postgresql

# 4. Preparación de la Base de Datos y Schema
echo "🐘 Configurando Base de Datos fuxion_db..."
sudo -u postgres psql -c "CREATE DATABASE fuxion_db;" || true
sudo -u postgres psql -d fuxion_db -c "CREATE EXTENSION IF NOT EXISTS ltree;"

# 5. Compilación del Motor Fuxion Pro-Lev X (v2)
echo "🛠️ Compilando Motor de Cierre C++ (Postgres Native)..."
cd ~/fuxion-motorcierre || cd .
g++ -std=c++17 cierre_postgres.cpp -o cierre_postgres -I/usr/include/postgresql -lpq

# 6. Aplicar Schema y Poblar 1 Millón de Usuarios (Balanceado para 8GB EBS)
echo "🔥 Cargando 1,000,000 de usuarios con ltree..."
psql -d fuxion_db -f schema_fuxion.sql
psql -d fuxion_db -f populate_1m.sql

# 7. Configuración de PM2 para Dashboard
echo "🛰️ Lanzando Dashboard con PM2..."
sudo npm install -g pm2
npm install --silent
pm2 delete fuxion-dashboard 2>/dev/null || true
pm2 start server.js --name fuxion-dashboard
pm2 save

echo "✅ CONFIGURACIÓN AWS COMPLETADA CON ÉXITO."
echo "🔗 Dashboard FuXion: http://$(curl -s ifconfig.me):3000"
echo "⚙️ Motor de Cierre: ~/fuxion-motorcierre/cierre_postgres"
