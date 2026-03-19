#!/bin/bash
set -e

echo "🚀 Iniciando configuración maestra de Fuxion en AWS (Ubuntu 24.04 Noble)..."

# 1. Actualización y Dependencias base
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake git libssl-dev libsasl2-dev pkg-config curl

# 2. Instalación de Node.js 20 si no existe
if ! command -v node &> /dev/null; then
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y nodejs
fi

# 3. Compilación de Drivers MongoDB (C y C++) desde Fuente
cd /tmp
echo "📦 Clonando Mongo C Driver..."
rm -rf mongo-c-driver
git clone --depth 1 -b 1.29.1 https://github.com/mongodb/mongo-c-driver.git
cd mongo-c-driver
mkdir -p cmake-build && cd cmake-build
cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF ..
echo "🏗️ Compilando C Driver..."
make -j$(nproc) && sudo make install
sudo ldconfig

cd /tmp
echo "📦 Clonando Mongo CXX Driver..."
rm -rf mongo-cxx-driver
git clone --depth 1 -b r3.11.0 https://github.com/mongodb/mongo-cxx-driver.git
cd mongo-cxx-driver
mkdir -p cmake-build && cd cmake-build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
echo "🏗️ Compilando CXX Driver..."
make -j$(nproc) && sudo make install
sudo ldconfig

# 4. Compilación de los binarios del Proyecto Fuxion
echo "🛠️ Compilando binarios de Fuxion (Motor C++)..."
cd ~/fuxion-motorcierre
g++ -std=c++17 mongo_benchmark.cpp -o mongo_benchmark $(pkg-config --cflags --libs libmongocxx libbsoncxx)
g++ -std=c++17 cierre_fuxion.cpp -o cierre_fuxion $(pkg-config --cflags --libs libmongocxx libbsoncxx)

# 5. Población de la Base de Datos (10 Millones)
echo "🔥 Generando 10,000,000 de usuarios en MongoDB (esto toma ~1 min)..."
./mongo_benchmark | head -n 20 # Mostrar progreso inicial

# 6. Configuración de PM2 para Servidor Web
echo "🛰️ Lanzando Dashboard con PM2..."
sudo npm install -g pm2
pm2 delete fuxion-dashboard 2>/dev/null || true
pm2 start server.js --name fuxion-dashboard
pm2 save

echo "✅ CONFIGURACIÓN COMPLETADA CON ÉXITO."
echo "🔗 Dashboard disponible en: http://$(curl -s ifconfig.me):3000"
