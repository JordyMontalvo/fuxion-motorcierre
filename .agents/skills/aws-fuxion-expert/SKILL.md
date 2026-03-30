---
name: AWS Fuxion Server Expert (PostgreSQL + Ltree Edition)
description: Experto en el despliegue de la infraestructura de Fuxion en AWS EC2 optimizada con PostgreSQL, Ltree y C++ para cierres masivos.
---

# AWS Fuxion Server Expert (v2.0 - Postgres)

Este agente experto gestiona el ecosistema de **Fuxion (Pro-Lev X)** en AWS, especializado en el motor de cierre de ultra-alto rendimiento basado en **PostgreSQL**.

## 🏢 Arquitectura de Producción

- **OS:** Ubuntu 22.04 / 24.04 (EC2 t3.xlarge+ recomendado para 10M).
- **Database:** **PostgreSQL 16** con extensión **`ltree`** para jerarquías.
- **Motor C++:** Compilado con `libpq` para acceso directo a Postgres.
- **Backend:** Node.js 20 + Express (Serving Dashboard & Tree-View).
- **Process Manager:** PM2.

## 🛠️ Comandos de Mantenimiento Críticos

### Ver Estado de PostgreSQL:
```bash
sudo systemctl status postgresql
# Acceder a la consola
sudo -u postgres psql -d fuxion_db
```

### Consultar Jerarquía (ltree):
```sql
-- Buscar todos los descendientes del nodo 1.44
SELECT id, name FROM users WHERE path <@ '1.44';
```

### Compilar el Motor Postgres C++:
```bash
cd ~/fuxion-motorcierre
g++ -std=c++17 cierre_postgres.cpp -o cierre_postgres -I/usr/include/postgresql -lpq
```

### Logs de PM2:
```bash
pm2 logs fuxion-dashboard
```

## 🔐 Configuración de Red AWS (Security Groups)

- **Port 22 (SSH):** Gestión.
- **Port 3000 (HTTP):** Dashboard de Cierre.
- **Port 5432 (PG):** **SOLO INTERNO** (o IP específica para DB Compass). No abrir a Anywhere (0.0.0.0/0).

## 🚨 Troubleshooting

1. **Error de Conexión DB:** Verificar que el rol de postgres permita acceso local (Archivo `/etc/postgresql/16/main/pg_hba.conf`).
2. **Ltree no habilitado:** Ejecutar `CREATE EXTENSION ltree;` en `psql`.
3. **Poblar datos:** Usar el script `node populate_pg.js` para los primeros 100k o el motor C++ masivo para 10M.

---
*Este agente experto fue actualizado para soportar el nuevo motor PostgreSQL Ltree de alta eficiencia.*
