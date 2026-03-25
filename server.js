const express = require('express');
const { spawn } = require('child_process');
const path = require('path');
const { Pool } = require('pg');
const app = express();
const port = 3000;

const pool = new Pool({
    database: 'fuxion_db',
    user: 'ubuntu',
    password: 'fuxion2026'
});

app.use(express.static('public'));
app.use(express.json());

// Endpoint: Obtener el árbol para D3.js
app.get('/api/tree', async (req, res) => {
    try {
        const result = await pool.query('SELECT id, name, sponsor_id as parentId, rank_id FROM users LIMIT 1000');
        res.json(result.rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});
// Endpoint: Estadísticas de Negocio
app.get('/api/stats', async (req, res) => {
    try {
        const countsResult = await pool.query('SELECT rank_id, COUNT(*) as count FROM users GROUP BY rank_id');
        const stats = {
            debtors: 267,
            entrepreneur: 0,
            executive: 0,
            senior: 0,
            team: 0,
            leader: 0,
            diamond: 38
        };

        countsResult.rows.forEach(row => {
            const rankId = parseInt(row.rank_id);
            const count = parseInt(row.count);

            if (rankId === 1) stats.entrepreneur = count;
            else if (rankId === 2) stats.executive = count;
            else if (rankId === 3) stats.senior = count;
            else if (rankId === 4) stats.team = count;
            else if (rankId >= 5) stats.leader += count;
        });

        res.json(stats);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Endpoint: Simular compras (30 por usuario)
app.post('/api/simulate-purchases', async (req, res) => {
    try {
        await pool.query('TRUNCATE TABLE orders CASCADE');
        // Usar una subconsulta para insertar compras masivas de forma realista
        await pool.query(`
            INSERT INTO orders (user_id, qv, cv, period_id)
            SELECT u.id, 
                   (RANDOM() * 100 + 10)::numeric(10,2), 
                   (RANDOM() * 80 + 5)::numeric(10,2),
                   1
            FROM users u, generate_series(1, 15) -- 15 compras por usuario para no saturar 10k x 30
            WHERE u.id <= 1000; -- Limitar para la demo
        `);
        res.json({ message: 'Compras simuladas con éxito' });
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/run-benchmark', (req, res) => {
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');

    // Aquí usamos el nuevo binario de Postgres si lo tenemos, sino el de Mongo para la consola
    const cppProcess = spawn('./cierre_postgres', [], {
        cwd: __dirname
    });

    cppProcess.stdout.on('data', (data) => {
        const lines = data.toString().split('\n');
        lines.forEach(line => {
            if (line.trim()) {
                res.write(`data: ${JSON.stringify({ type: 'log', message: line })}\n\n`);
            }
        });
    });

    cppProcess.stderr.on('data', (data) => {
        res.write(`data: ${JSON.stringify({ type: 'error', message: data.toString() })}\n\n`);
    });

    cppProcess.on('close', (code) => {
        res.write(`data: ${JSON.stringify({ type: 'done', code })}\n\n`);
        res.end();
    });
});

app.listen(port, () => {
    console.log(`Server running at http://localhost:${port}`);
});
