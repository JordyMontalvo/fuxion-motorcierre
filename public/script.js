document.addEventListener('DOMContentLoaded', () => {
    const runBtn = document.getElementById('runBtn');
    const consoleOutput = document.getElementById('console');
    const clearConsole = document.getElementById('clearConsole');
    const tabBtns = document.querySelectorAll('.tab-btn');
    const tabContents = document.querySelectorAll('.tab-content');
    const seedPurchasesBtn = document.getElementById('seedPurchasesBtn');
    const simStatus = document.getElementById('sim-status');
    const refreshTree = document.getElementById('refreshTree');

    // --- Tab Switching ---
    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const tabId = btn.getAttribute('data-tab');
            tabBtns.forEach(b => b.classList.remove('active'));
            tabContents.forEach(c => c.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(tabId).classList.add('active');

            if (tabId === 'dashboard') updateStats();
        });
    });

    async function updateStats() {
        try {
            const res = await fetch('/api/stats');
            const data = await res.json();
            
            // Cuadritos (KPIs Superiores)
            document.getElementById('totalUsers').textContent = (data.totalUsers >= 1000000) ? (data.totalUsers/1000000).toFixed(1) + "M" : data.totalUsers.toLocaleString();
            document.getElementById('avgResidual').textContent = data.avgPv4 > 0 ? "$" + parseFloat(data.avgPv4).toFixed(2) : "-";
            document.getElementById('totalResidual').textContent = data.totalDv4 > 0 ? "$" + parseFloat(data.totalDv4).toLocaleString() : "-";

            const total = data.totalUsers || 1;
            const updateRow = (id, count) => {
                const el = document.getElementById(id);
                if (el) {
                    el.textContent = count.toLocaleString();
                    const pctEl = el.nextElementSibling;
                    if (pctEl) pctEl.textContent = ((count / total) * 100).toFixed(2) + "%";
                }
            };

            // Tabla de Estadísticas de Negocio con Porcentajes
            updateRow('count-debtors', data.debtors);
            updateRow('count-entre', data.entrepreneur);
            updateRow('count-exec', data.executive);
            updateRow('count-senior', data.senior);
            updateRow('count-team', data.team);
            updateRow('count-senior-team', data.seniorTeam);
            updateRow('count-leader', data.leader);
            updateRow('count-premier', data.premier);
            updateRow('count-elite', data.elite);
            updateRow('count-diamond-real', data.diamond);
            updateRow('count-blue-diamond', data.blueDiamond);
        } catch (err) {
            console.error("Error fetching stats:", err);
        }
    }

    // Initial load
    updateStats();

    // --- C++ Execution (Benchmark) ---
    runBtn.addEventListener('click', () => {
        runBtn.disabled = true;
        consoleOutput.innerHTML = '<span class="system-msg">Iniciando motor PostgreSQL...</span><br>';
        
        const eventSource = new EventSource('/run-benchmark');

        eventSource.onmessage = (event) => {
            const data = JSON.parse(event.data);
            
            if (data.type === 'log') {
                const line = document.createElement('div');
                line.textContent = data.message;
                if (data.message.includes('OK') || data.message.includes('éxito') || data.message.includes('COMPLETADO')) {
                    line.style.color = '#00f2ff';
                }
                consoleOutput.appendChild(line);
                consoleOutput.scrollTop = consoleOutput.scrollHeight;

                // Extraer KPIs de los logs
                if (data.message.includes('COMPLETADO EN')) {
                   const time = data.message.split('EN')[1].trim();
                   document.getElementById('totalTime').textContent = time;
                }
                if (data.message.includes('DV4 Total')) {
                    document.getElementById('totalResidual').textContent = data.message.split('$')[1];
                }
            }

            if (data.type === 'done') {
                eventSource.close();
                runBtn.disabled = false;
                updateStats(); // Refrescar tabla de rangos al terminar
                const doneMsg = document.createElement('div');
                doneMsg.innerHTML = '<br><span class="success-msg">✅ Proceso finalizado con éxito.</span>';
                consoleOutput.appendChild(doneMsg);
            }
        };

        eventSource.onerror = (err) => {
            eventSource.close();
            runBtn.disabled = false;
        };
    });

    // --- Simulation (Purchases) ---
    seedPurchasesBtn.addEventListener('click', async () => {
        seedPurchasesBtn.disabled = true;
        simStatus.textContent = 'Generando compras en base de datos...';
        
        try {
            const res = await fetch('/api/simulate-purchases', { method: 'POST' });
            const data = await res.json();
            simStatus.textContent = '✅ 15,000 transacciones generadas con éxito.';
        } catch (err) {
            simStatus.textContent = '❌ Error al generar compras.';
        } finally {
            seedPurchasesBtn.disabled = false;
        }
    });

    // --- Tree Visualization (D3.js) ---
    async function renderTree() {
        const container = document.getElementById('tree-container');
        container.innerHTML = '';
        
        const width = container.clientWidth || 1000;
        const height = 600;

        const res = await fetch('/api/tree');
        const flatData = await res.json();

        if (flatData.length === 0) {
            container.innerHTML = '<p style="padding: 20px;">No hay usuarios en la DB. Corre el seeder de Postgres.</p>';
            return;
        }

        const stratify = d3.stratify().id(d => d.id).parentId(d => d.parentid);
        const root = stratify(flatData);

        const treeLayout = d3.tree().size([width - 100, height - 100]);
        treeLayout(root);

        const svg = d3.select('#tree-container')
            .append('svg')
            .attr('width', width)
            .attr('height', height)
            .call(d3.zoom().on('zoom', (event) => {
                g.attr('transform', event.transform);
            }))
            .append('g');

        const g = svg.append('g').attr('transform', 'translate(50, 50)');

        // Links
        g.selectAll('.link')
            .data(root.links())
            .enter()
            .append('path')
            .attr('class', 'link')
            .attr('d', d3.linkVertical()
                .x(d => d.x)
                .y(d => d.y));

        // Nodes
        const node = g.selectAll('.node')
            .data(root.descendants())
            .enter()
            .append('g')
            .attr('class', 'node')
            .attr('transform', d => `translate(${d.x},${d.y})`);

        node.append('circle').attr('r', 4);
        node.append('text')
            .attr('dy', '.35em')
            .attr('y', d => d.children ? -10 : 10)
            .style('text-anchor', 'middle')
            .text(d => d.data.name.split(' ')[1] || d.data.name);
    }

    refreshTree.addEventListener('click', renderTree);
    clearConsole.addEventListener('click', () => consoleOutput.innerHTML = '');
});
