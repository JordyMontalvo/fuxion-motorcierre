#!/bin/bash
set -e

echo "🚀 CONFIGURACIÓN FINAL FUXION AWS (POSTGRESQL + LTREE + C++)..."

# 1. Actualización de Repositorios y Utilerías
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake git libssl-dev pkg-config curl libpq-dev

# 2. Instalación de PostgreSQL 16
if ! command -v psql &> /dev/null; then
    echo "🐘 Instalando PostgreSQL 16..."
    sudo apt-get install -y postgresql-16
fi

# 3. Configuración de la Base de Datos Fuxion
echo "🛠️ Configurando Database y Extensiones..."
sudo -u postgres psql -c "CREATE DATABASE fuxion_db;" || true
sudo -u postgres psql -d fuxion_db -c "CREATE EXTENSION IF NOT EXISTS ltree;"

# 4. Creación de Usuario y Permisos para el Motor C++
# (Asumiendo que corre como el usuario actual)
CURRENT_USER=$(whoami)
sudo -u postgres psql -c "CREATE USER $CURRENT_USER WITH PASSWORD 'fuxion2026';" || true
sudo -u postgres psql -c "ALTER DATABASE fuxion_db OWNER TO $CURRENT_USER;"
sudo -u postgres psql -d fuxion_db -c "GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO $CURRENT_USER;"

# 5. Instalación de Node.js 20
if ! command -v node &> /dev/null; then
    echo "📦 Instalando Node.js 20..."
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y nodejs
fi

# 6. Sincronización de Código y Compilación
REPO_DIR="/home/ubuntu/fuxion-repo"
echo "⚙️ Sincronizando repositorio en $REPO_DIR..."
cd $REPO_DIR || exit 1
git checkout master
git pull origin master

echo "⚙️ Compilando Motor de Cierre C++ (Postgres Edition)..."
g++ -std=c++17 cierre_postgres.cpp -o cierre_postgres -I/usr/include/postgresql -lpq

echo "🐘 Aplicando Esquema de Base de Datos y Poblado..."
psql -d fuxion_db -f schema_fuxion.sql
node populate_pg.js

# 7. Despliegue con PM2
echo "🛰️ Lanzando Servicios con PM2..."
sudo npm install -g pm2
pm2 delete fuxion-dashboard 2>/dev/null || true
npm install --silent
pm2 start server.js --name fuxion-dashboard
pm2 save

echo "✅ DESPLIEGUE AWS COMPLETADO."
echo "🔗 URL: http://$(curl -s ifconfig.me):3000"
