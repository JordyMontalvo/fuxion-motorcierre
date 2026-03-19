const runBtn = document.getElementById('runBtn');
const consoleEl = document.getElementById('console');
const statusDot = document.querySelector('.dot');
const statusText = document.querySelector('.status-text');
const clearConsoleBtn = document.getElementById('clearConsole');

// KPI elements
const totalUsersEl = document.getElementById('totalUsers');
const totalTimeEl = document.getElementById('totalTime');
const totalResidualEl = document.getElementById('totalResidual');
const avgResidualEl = document.getElementById('avgResidual');

// Table elements
const deudoresEl = document.getElementById('deudores');
const deudoresPctEl = document.getElementById('deudoresPct');
const favorEl = document.getElementById('favor');
const favorPctEl = document.getElementById('favorPct');
const ceroEl = document.getElementById('cero');
const ceroPctEl = document.getElementById('ceroPct');

function cleanAnsi(text) {
    // Robust cleaning for ANSI codes including background and complex sequences
    return text.replace(/\x1B\[[0-9;]*[a-zA-Z]/g, '').trim();
}

function formatLargeNumber(n) {
    const num = parseInt(n.toString().replace(/,/g, ''));
    if (isNaN(num)) return '-';
    if (num >= 1e9) return (num / 1e9).toFixed(1) + 'B';
    if (num >= 1e6) return (num / 1e6).toFixed(1) + 'M';
    return new Intl.NumberFormat('en-US').format(num);
}

function formatUSD(value) {
    if (typeof value === 'string') {
        value = value.replace(/[$,]/g, '').trim();
    }
    const num = parseFloat(value);
    if (isNaN(num)) return '-';

    if (num >= 1e9) {
        return '$' + (num / 1e9).toFixed(1) + 'B';
    }
    if (num >= 1e6) {
        return '$' + (num / 1e6).toFixed(1) + 'M';
    }
    if (num >= 1e3) {
        return '$' + (num / 1e3).toFixed(1) + 'K';
    }

    return new Intl.NumberFormat('en-US', {
        style: 'currency',
        currency: 'USD',
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    }).format(num);
}

function appendLog(message, type = '') {
    const line = document.createElement('div');
    line.className = type ? type + '-msg' : '';
    line.textContent = cleanAnsi(message);
    consoleEl.appendChild(line);
    consoleEl.scrollTop = consoleEl.scrollHeight;
}

function parseBusinessStats(line) {
    const cleanLine = cleanAnsi(line);
    
    // Total usuarios
    if (cleanLine.includes('Total usuarios procesados:')) {
        const val = cleanLine.split(':')[1].trim();
        totalUsersEl.textContent = formatLargeNumber(val);
    }
    // Deudores
    if (cleanLine.includes('Deudores (residual > 0):')) {
        const parts = cleanLine.split(':')[1].trim().split(/\s+/);
        deudoresEl.textContent = formatLargeNumber(parts[0]);
        deudoresPctEl.textContent = parts[1] || '';
    }
    // A favor
    if (cleanLine.includes('Saldo a favor (residual < 0):')) {
        const parts = cleanLine.split(':')[1].trim().split(/\s+/);
        favorEl.textContent = formatLargeNumber(parts[0]);
        favorPctEl.textContent = parts[1] || '';
    }
    // Saldo cero
    if (cleanLine.includes('Saldo cero:')) {
        const val = cleanLine.split(':')[1].trim();
        ceroEl.textContent = formatLargeNumber(val);
        ceroPctEl.textContent = '-';
    }
    // Residual TOTAL
    if (cleanLine.includes('Residual TOTAL del portafolio:')) {
        const val = cleanLine.split(':')[1].trim();
        totalResidualEl.textContent = formatUSD(val);
    }
    // Residual promedio
    if (cleanLine.includes('Residual promedio por usuario:')) {
        const val = cleanLine.split(':')[1].trim();
        avgResidualEl.textContent = formatUSD(val);
    }
    // Rango Distribution
    if (cleanLine.includes(':') && !cleanLine.includes('TOTAL') && (cleanLine.includes('Leader') || cleanLine.includes('Diamond') || cleanLine.includes('Builder') || cleanLine.includes('Entrepreneur'))) {
        const [rank, count] = cleanLine.split(':').map(s => s.trim());
        const row = Array.from(document.querySelectorAll('#resultsTable tr')).find(r => r.textContent.includes(rank));
        if (row) {
            row.cells[1].textContent = formatLargeNumber(count);
        } else {
            // Append new row if rank not in static table
            const tbody = document.querySelector('#resultsTable tbody');
            const tr = document.createElement('tr');
            tr.innerHTML = `<td>🏆 ${rank}</td><td>${formatLargeNumber(count)}</td><td>-</td>`;
            tbody.appendChild(tr);
        }
    }
    // Pipeline Real (Time)
    if (cleanLine.includes('PIPELINE REAL (desde MongoDB)')) {
        const parts = cleanLine.match(/(\d+\.\d+)\s+ms/);
        if (parts) {
            const ms = parseFloat(parts[1]);
            totalTimeEl.textContent = (ms / 1000).toFixed(2) + ' s';
        }
    }
}

runBtn.addEventListener('click', () => {
    // Reset UI
    consoleEl.innerHTML = '';
    appendLog('🚀 Iniciando proceso en servidor...', 'system');
    runBtn.disabled = true;
    statusDot.classList.add('running');
    statusText.textContent = 'Procesando 10M registros...';

    const eventSource = new EventSource('/run-benchmark');

    eventSource.onmessage = (event) => {
        const data = JSON.parse(event.data);

        if (data.type === 'log') {
            appendLog(data.message);
            parseBusinessStats(data.message);
        } else if (data.type === 'error') {
            appendLog(data.message, 'error');
        } else if (data.type === 'done') {
            eventSource.close();
            runBtn.disabled = false;
            statusDot.classList.remove('running');
            statusText.textContent = 'Cierre completado con éxito';
            appendLog('✅ Proceso finalizado.', 'success');
        }
    };

    eventSource.onerror = (err) => {
        console.error('SSE Error:', err);
        eventSource.close();
        runBtn.disabled = false;
        statusDot.classList.remove('running');
        statusText.textContent = 'Error en el servidor';
        appendLog('❌ Error de conexión con el servidor.', 'error');
    };
});

clearConsoleBtn.addEventListener('click', () => {
    consoleEl.innerHTML = '<span class="system-msg">Consola limpia. Esperando inicio...</span>';
});

// Export to Image Logic
const exportBtn = document.getElementById('exportBtn');
exportBtn.addEventListener('click', () => {
    // Feedback visual
    exportBtn.disabled = true;
    exportBtn.innerHTML = '<span class="btn-icon">⌛</span> Generando...';
    
    // Configuración para captura de alta calidad
    const options = {
        backgroundColor: '#f0f4f8',
        scale: 2, // X2 para que no se pixelee en WhatsApp
        useCORS: true,
        logging: false,
        windowWidth: 1200, // Forzar ancho de escritorio
        onclone: (clonedDoc) => {
            // Asegurarse de que la consola clonada muestre todo el contenido
            const clonedConsole = clonedDoc.getElementById('console');
            if (clonedConsole) {
                clonedConsole.style.height = 'auto';
                clonedConsole.style.overflow = 'visible';
            }
            // Ocultar botones en la captura
            const clonedBtns = clonedDoc.querySelectorAll('button');
            clonedBtns.forEach(b => b.style.display = 'none');
            
            // Ajustar el contenedor para que no se corte
            const clonedContainer = clonedDoc.querySelector('.container');
            if (clonedContainer) {
                clonedContainer.style.padding = '40px';
                clonedContainer.style.maxWidth = 'none';
            }
        }
    };

    html2canvas(document.body, options).then(canvas => {
        const link = document.createElement('a');
        link.download = `Fuxion_Reporte_${new Date().getTime()}.png`;
        link.href = canvas.toDataURL('image/png', 1.0);
        link.click();
        
        // Restaurar botón
        exportBtn.disabled = false;
        exportBtn.innerHTML = '<span class="btn-icon">📸</span> Exportar Reporte';
    }).catch(err => {
        console.error('Export Error:', err);
        exportBtn.disabled = false;
        exportBtn.innerHTML = '<span class="btn-icon">❌</span> Error';
    });
});
