-- ════════════════════════════════════════════════════════════════════════
--  POPULATE 10 MILLONES DE USUARIOS — FUXION PRO-LEV X
--  Ejecutar en AWS EC2 con PostgreSQL 16 + ltree
--  Tiempo estimado: ~2-5 minutos en t3.xlarge
-- ════════════════════════════════════════════════════════════════════════

-- 1. Activar ltree si no está activo
CREATE EXTENSION IF NOT EXISTS ltree;

-- 2. Deshabilitar índices temporalmente para inserción masiva rápida
DROP INDEX IF EXISTS path_gist_idx;
DROP INDEX IF EXISTS path_idx;
DROP INDEX IF EXISTS sponsor_idx;

-- 3. Insertar 10,000,000 usuarios en un solo comando usando generate_series
--    Estructura jerárquica unilevel: cada 10 usuarios comparten un patrocinador
--    Nivel 1 (raíz): usuarios 1-10 (los 10 primeros ya deben existir como semilla)
--    Nivel 2+: usuarios 11 en adelante, patrocinados por (id/10)

INSERT INTO users (id, name, sponsor_id, rank_id, path)
SELECT 
    g,
    'EF_' || g,
    CASE 
        WHEN g <= 10 THEN NULL           -- Los primeros 10 son raíces
        ELSE (g / 10)::int               -- Cada 10 usuarios → 1 patrocinador
    END,
    1,  -- rank_id = 1 (Entrepreneur por defecto)
    CASE
        WHEN g <= 10 THEN g::text::ltree                         -- Path raíz: "5"
        ELSE ((g/10)::int::text || '.' || g::text)::ltree       -- Path: "44.441"
    END
FROM generate_series(1, 10000000) g
ON CONFLICT (id) DO NOTHING;

-- 4. Recrear índices DESPUÉS de la inserción masiva (mucho más rápido)
CREATE INDEX path_gist_idx ON users USING GIST (path);
CREATE INDEX path_idx      ON users USING BTREE (path);
CREATE INDEX sponsor_idx   ON users (sponsor_id);

-- 5. Crear período activo si no existe
INSERT INTO periods (name, start_date, end_date, is_closed)
VALUES ('Periodo 2026-Q1', '2026-01-01', '2026-03-31', FALSE)
ON CONFLICT DO NOTHING;

-- 6. Verificar resultado
SELECT 
    COUNT(*) as total_usuarios,
    COUNT(DISTINCT sponsor_id) as total_patrocinadores,
    MAX(id) as max_id
FROM users;
