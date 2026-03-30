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
        const totalUsersResult = await pool.query('SELECT COUNT(*) as total FROM users');
        const dv4Result = await pool.query('SELECT SUM(dv4) as total FROM closing_results WHERE period_id = 1');
        const avgPv4Result = await pool.query('SELECT AVG(qv) as avg_pv4 FROM orders WHERE period_id = 1');
        const debtorsResult = await pool.query("SELECT COUNT(*) as cnt FROM closing_results WHERE period_id = 1 AND dv4 = 0");
        
        const stats = {
            debtors: parseInt(debtorsResult.rows[0].cnt) || 0,
            totalUsers: parseInt(totalUsersResult.rows[0].total) || 10000000,
            totalDv4: dv4Result.rows[0].total || 0,
            avgPv4: avgPv4Result.rows[0].avg_pv4 || 0,
            entrepreneur: 0, executive: 0, senior: 0,
            team: 0, seniorTeam: 0, leader: 0,
            premier: 0, elite: 0, diamond: 0, blueDiamond: 0
        };

        countsResult.rows.forEach(row => {
            const rankId = parseInt(row.rank_id);
            const count = parseInt(row.count);

            if (rankId === 1) stats.entrepreneur = count;
            else if (rankId === 2) stats.executive = count;
            else if (rankId === 3) stats.senior = count;
            else if (rankId === 4) stats.team = count;
            else if (rankId === 5) stats.seniorTeam = count;
            else if (rankId === 6) stats.leader = count;
            else if (rankId === 7) stats.premier = count;
            else if (rankId === 8) stats.elite = count;
            else if (rankId === 9) stats.diamond = count;
            else if (rankId >= 10) stats.blueDiamond += count;
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
        // Para 10M usuarios: insertar por lotes usando generate_series nativo de Postgres
        // Se limita a 100k usuarios como batch de demo (ajustar en producción AWS)
        await pool.query(`
            INSERT INTO orders (user_id, qv, cv, period_id)
            SELECT u.id, 
                   (RANDOM() * 100 + 10)::numeric(10,2), 
                   (RANDOM() * 80 + 5)::numeric(10,2),
                   1
            FROM users u, generate_series(1, 5) gs -- 5 compras optimizadas x usuario
            LIMIT 500000; -- Batch demo: 100k usuarios x 5 = 500k orders
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
