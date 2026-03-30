-- Script para generar 1 MILLON de usuarios en AWS (EC2) de forma instantánea
-- Usamos el generador interno de Postgres para no saturar con miles de INSERTs individuales.

INSERT INTO users (id, name, sponsor_id, rank_id, path)
SELECT 
    g, 
    'Usuario_' || g, 
    (g / 10)::int + 1, -- Simular una rama (cada 10 usuarios penden de 1)
    (random() * 4 + 1)::int,
    ((g / 10)::int + 1)::text || '.' || g::text
FROM generate_series(10001, 1010000) g
ON CONFLICT (id) DO NOTHING;

-- Actualizar los paths ltree jerárquicamente (Solo para una estructura balanceada demo)
-- En un sistema real, los paths se calculan recursivamente al insertar.
