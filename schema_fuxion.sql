-- Esquema para el Plan de Pagos FuXion (Pro-Lev X) en PostgreSQL
-- Habilitando ltree para jerarquías ultra-rápidas
-- Habilitando ltree para jerarquías ultra-rápidas
CREATE EXTENSION IF NOT EXISTS ltree;

DROP TABLE IF EXISTS closing_results;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS activity;
DROP TABLE IF EXISTS view_user_volumes; -- View must drop before users normally or use CASCADE

-- Dropping tables to reset for clean demo, preserving users if possible?
-- Actually, the user asked if trees are formed. They are.
-- I will keep users but alter the column rank_id.

ALTER TABLE users ALTER COLUMN rank_id TYPE INT USING CASE 
    WHEN rank_id = 'Entrepreneur' THEN 1
    WHEN rank_id = 'Executive' THEN 2
    ELSE 1 END;

-- Tabla de Usuarios con Estructura Jerárquica
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    sponsor_id INTEGER REFERENCES users(id),
    name VARCHAR(100),
    path ltree, -- Ruta completa en el árbol: '1.44.506'
    level INTEGER, -- Profundidad en el árbol
    rank_id INT DEFAULT 1 -- 1: Entrepreneur, 2: Executive, etc.
);

-- Tabla de Compras/Transacciones Reales (QV)
CREATE TABLE IF NOT EXISTS orders (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    qv NUMERIC(10, 2) NOT NULL, -- Puntos de Calificación
    cv NUMERIC(10, 2) NOT NULL, -- Puntos Comisionables
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    period_id INTEGER DEFAULT 1
);

-- Tabla de Actividad Agregada (PV4)
-- Esto se calcula a partir de las compras
CREATE TABLE IF NOT EXISTS activity (
    user_id INTEGER REFERENCES users(id),
    pv4 NUMERIC(10, 2) DEFAULT 0, -- Volumen Personal 4 Semanas
    dv4 NUMERIC(10, 2) DEFAULT 0, -- Volumen Grupal 4 Semanas
    period_id INTEGER REFERENCES periods(id),
    PRIMARY KEY (user_id, period_id)
);

-- Tabla de Resultados del Cierre (Histórico)
CREATE TABLE IF NOT EXISTS closing_results (
    user_id INTEGER REFERENCES users(id),
    period_id INTEGER REFERENCES periods(id),
    rank_id VARCHAR(50),
    residual_bonus NUMERIC(12, 2),
    is_qualified BOOLEAN,
    processed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, period_id)
);

-- Índices GIST para ltree (Búsquedas de sub-árboles instantáneas)
CREATE INDEX IF NOT EXISTS path_gist_idx ON users USING GIST (path);
CREATE INDEX IF NOT EXISTS path_idx ON users USING BTREE (path);
CREATE INDEX IF NOT EXISTS sponsor_idx ON users (sponsor_id);

-- Vista para ver el Volumen Acumulado por ltree
CREATE OR REPLACE VIEW view_user_volumes AS
SELECT 
    u.id,
    u.name,
    u.path,
    u.rank_id,
    COALESCE((SELECT SUM(qv) FROM orders o WHERE o.user_id = u.id), 0) as pv_periodo,
    COALESCE((SELECT SUM(o.qv) 
     FROM orders o 
     JOIN users sub ON o.user_id = sub.id 
     WHERE sub.path <@ u.path), 0) as dv_total
FROM users u;
