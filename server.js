const express = require('express');
const { spawn } = require('child_process');
const path = require('path');
const app = express();
const port = 3000;

app.use(express.static('public'));

app.get('/run-benchmark', (req, res) => {
    res.setHeader('Content-Type', 'text/event-stream');
    res.setHeader('Cache-Control', 'no-cache');
    res.setHeader('Connection', 'keep-alive');

    const cppProcess = spawn('./cierre_fuxion', [], {
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
