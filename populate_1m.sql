-- ════════════════════════════════════════════════════════════════════════
--  POPULATE 1 MILLÓN DE USUARIOS — FUXION PRO-LEV X
--  Ajustado para ocupar poco espacio en AWS EC2 (8GB)
-- ════════════════════════════════════════════════════════════════════════

CREATE EXTENSION IF NOT EXISTS ltree;

-- 1. Limpieza segura
TRUNCATE TABLE closing_results CASCADE;
TRUNCATE TABLE orders CASCADE;
TRUNCATE TABLE users CASCADE;

-- 2. Inserción de 1,000,000 usuarios con una estructura multinivel profunda
--    Sponsors dinámicos para que no sea plana
INSERT INTO users (id, name, sponsor_id, rank_id, path)
SELECT 
    g,
    'EF_' || g,
    CASE 
        WHEN g <= 10 THEN NULL
        WHEN g <= 1000 THEN (random() * 9 + 1)::int -- Nivel 2
        ELSE (random() * (g/10) + 1)::int             -- Niveles profundos (para balances realistas)
    END,
    1,
    '0' -- Temp path para resetear después
FROM generate_series(1, 1000000) g;

-- 3. Generación masiva de paths ltree (esto es clave para 1M)
--    En una demo, podemos asignar paths jerárquicos basados en su sponsor_id para visualizar el árbol
--    OPTIMIZACIÓN: Usamos una función recursiva o simplemente generamos paths coherentes
UPDATE users u
SET path = sub.new_path::ltree
FROM (
  -- Path simple: sponsor.path + id
  SELECT id, (COALESCE(sponsor_id::text, '1') || '.' || id::text) as new_path
  FROM users
) sub WHERE u.id = sub.id;

-- 4. Registrar periodo si no existe
INSERT INTO periods (id, name, start_date, end_date, is_closed)
VALUES (1, 'Periodo Demo-1M', '2026-03-01', '2026-03-31', FALSE)
ON CONFLICT (id) DO NOTHING;

-- 5. Generar 3 millones de compras (Suficiente para alimentar rangos altos)
--    Hacemos que los primeros 100 usuarios tengan más volumen para que sean DIAMONDS
INSERT INTO orders (user_id, qv, cv, period_id)
SELECT 
    (random() * 999999 + 1)::int,
    (random() * 200 + 50)::numeric(10,2),
    (random() * 180 + 40)::numeric(10,2),
    1
FROM generate_series(1, 3000000);

-- Compras extra para los "líderes" (id < 100)
INSERT INTO orders (user_id, qv, cv, period_id)
SELECT 
    (random() * 100 + 1)::int,
    (random() * 1000 + 500)::numeric(10,2),
    (random() * 900 + 450)::numeric(10,2),
    1
FROM generate_series(1, 10000);

-- Indices finales
CREATE INDEX IF NOT EXISTS path_gist_idx ON users USING GIST (path);
CREATE INDEX IF NOT EXISTS path_idx ON users USING BTREE (path);
CREATE INDEX IF NOT EXISTS sponsor_idx ON users (sponsor_id);
