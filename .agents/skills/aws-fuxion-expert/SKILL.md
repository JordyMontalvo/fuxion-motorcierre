---
name: AWS Fuxion Server Expert
description: Experto en el despliegue y mantenimiento de la infraestructura de Fuxion en AWS EC2 con C++ y MongoDB.
---

# AWS Fuxion Server Expert

Este agente ha sido entrenado específicamente para gestionar el servidor de alto rendimiento de Fuxion en Amazon Web Services (AWS).

## 🏢 Arquitectura del Sistema
- **Instancia:** EC2 Ubuntu 24.04 (Noble) / Ubuntu 22.04 (Jammy).
- **Motor C++:** Compilado nativamente con drivers oficiales `mongoc` y `mongocxx`.
- **Database:** MongoDB 7.0 Community Edition local.
- **Backend:** Node.js 20 + Express.
- **Process Manager:** PM2 para persistencia.

## 🛠️ Comandos de Mantenimiento Críticos

### Ver Errores o Logs en Tiempo Real:
```bash
pm2 logs fuxion-dashboard
```

### Reiniciar el Dashboard:
```bash
pm2 restart fuxion-dashboard
```

### Consultar la Base de Datos (10M Usuarios):
```bash
mongosh cierre_db --eval 'db.usuarios.countDocuments()'
```

### Compilar el Motor C++ (en caso de cambios en el código):
```bash
cd ~/fuxion-motorcierre
# Compilar motor de cierre
g++ -std=c++17 cierre_fuxion.cpp -o cierre_fuxion $(pkg-config --cflags --libs libmongocxx libbsoncxx)
# Compilar poblador/benchmark
g++ -std=c++17 mongo_benchmark.cpp -o mongo_benchmark $(pkg-config --cflags --libs libmongocxx libbsoncxx)
```

## 🔐 Configuración de Red (Security Groups)
Para que el sistema sea accesible, las **Inbound Rules** en AWS deben permitir:
- **Puerto 22 (SSH):** Tu IP o Anywhere.
- **Puerto 3000 (HTTP):** Anywhere (`0.0.0.0/0`).

## 🚨 Troubleshooting
1. **Página no carga:** Revisar si port 3000 está ocupado (`sudo lsof -i :3000`) o si el Security Group de AWS ha cambiado.
2. **Motor C++ no arranca:** Verificar la conexión local a Mongo (`sudo systemctl status mongod`) y que las librerías compartidas estén cargadas (`sudo ldconfig`).
3. **Poblar 10M de nuevo:** Ejecutar `./mongo_benchmark` para una carga ultra-rápida (60s).

---
*Este agente experto fue generado automáticamente para proteger la integridad del servidor Fuxion de 10 Millones.*
