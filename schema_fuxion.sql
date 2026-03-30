-- Esquema para el Plan de Pagos FuXion (Pro-Lev X) en PostgreSQL
CREATE EXTENSION IF NOT EXISTS ltree;

-- Limpieza para resetear demo (CASCADE para borrar dependencias)
DROP VIEW IF EXISTS view_user_volumes CASCADE;
DROP TABLE IF EXISTS closing_results CASCADE;
DROP TABLE IF EXISTS orders CASCADE;
DROP TABLE IF EXISTS activity CASCADE;
DROP TABLE IF EXISTS periods CASCADE;

-- Tabla de Periodos
CREATE TABLE periods (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50),
    start_date DATE,
    end_date DATE,
    is_closed BOOLEAN DEFAULT FALSE
);

-- Reset de la tabla users (sin borrarla para mantener el árbol de 10k)
ALTER TABLE users DROP COLUMN IF EXISTS rank_id;
ALTER TABLE users ADD COLUMN rank_id INT DEFAULT 1;

-- Tabla de Usuarios con Estructura Jerárquica
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    sponsor_id INTEGER REFERENCES users(id),
    name VARCHAR(100),
    path ltree,
    level INTEGER,
    rank_id INT DEFAULT 1
);

-- Tabla de Compras/Transacciones Reales (QV)
CREATE TABLE orders (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    qv NUMERIC(10, 2) NOT NULL,
    cv NUMERIC(10, 2) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    period_id INT DEFAULT 1
);

-- Tabla de Resultados del Cierre (Histórico)
CREATE TABLE closing_results (
    user_id INTEGER REFERENCES users(id),
    period_id INT DEFAULT 1,
    rank_id INT DEFAULT 1,
    dv4 NUMERIC(16, 2) DEFAULT 0,
    pv4 NUMERIC(12, 2) DEFAULT 0, -- Volumen Personal
    residual_total NUMERIC(16, 2) DEFAULT 0,
    -- Desglose por niveles para Bono Familia
    cv_n1 NUMERIC(16, 2) DEFAULT 0,
    cv_n2 NUMERIC(16, 2) DEFAULT 0,
    cv_n3 NUMERIC(16, 2) DEFAULT 0,
    cv_n4 NUMERIC(16, 2) DEFAULT 0,
    cv_n5 NUMERIC(16, 2) DEFAULT 0,
    cv_n6 NUMERIC(16, 2) DEFAULT 0,
    processed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, period_id)
);

-- Índices GIST para ltree
CREATE INDEX IF NOT EXISTS path_gist_idx ON users USING GIST (path);
CREATE INDEX IF NOT EXISTS path_idx ON users USING BTREE (path);
CREATE INDEX IF NOT EXISTS sponsor_idx ON users (sponsor_id);

-- Vista de volúmenes OPTIMIZADA para 10M usuarios
-- Usa JOINs con índices ltree en vez de subconsultas correlacionadas O(n²)
CREATE OR REPLACE VIEW view_user_volumes AS
SELECT 
    u.id,
    u.name,
    u.path,
    u.rank_id,
    COALESCE(pv.pv_periodo, 0)  AS pv_periodo,
    COALESCE(dv.dv_total,  0)   AS dv_total
FROM users u
-- PV4: volumen personal del usuario (JOIN simple, O(n) con índice en orders.user_id)
LEFT JOIN (
    SELECT user_id, SUM(qv) AS pv_periodo
    FROM orders
    WHERE period_id = 1
    GROUP BY user_id
) pv ON pv.user_id = u.id
-- DV4: volumen grupal usando ltree <@ (O(log n) con índice GIST)
LEFT JOIN (
    SELECT ancestor.id AS user_id, SUM(o.qv) AS dv_total
    FROM users ancestor
    JOIN users descendant ON descendant.path <@ ancestor.path
    JOIN orders o ON o.user_id = descendant.id AND o.period_id = 1
    GROUP BY ancestor.id
) dv ON dv.user_id = u.id;

-- Índice adicional recomendado para 10M: orders.user_id + period_id
CREATE INDEX IF NOT EXISTS orders_user_period_idx ON orders (user_id, period_id);
