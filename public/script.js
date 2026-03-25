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
            
            document.getElementById('count-debtors').textContent = data.debtors;
            document.getElementById('count-entre').textContent = data.entrepreneur.toLocaleString();
            document.getElementById('count-exec').textContent = data.executive.toLocaleString();
            document.getElementById('count-senior').textContent = data.senior.toLocaleString();
            document.getElementById('count-team').textContent = data.team.toLocaleString();
            document.getElementById('count-senior-team').textContent = data.seniorTeam.toLocaleString();
            document.getElementById('count-leader').textContent = data.leader.toLocaleString();
            document.getElementById('count-premier').textContent = data.premier.toLocaleString();
            document.getElementById('count-elite').textContent = data.elite.toLocaleString();
            document.getElementById('count-diamond-real').textContent = data.diamond.toLocaleString();
            document.getElementById('count-blue-diamond').textContent = data.blueDiamond.toLocaleString();
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
                if (data.message.includes('OK') || data.message.includes('éxito')) line.style.color = '#00f2ff';
                consoleOutput.appendChild(line);
                consoleOutput.scrollTop = consoleOutput.scrollHeight;

                // Update KPIs qualitatively for demo
                if (data.message.includes('DV4 Total')) {
                    document.getElementById('totalResidual').textContent = data.message.split('$')[1];
                }
                if (data.message.includes('completado en')) {
                    document.getElementById('totalTime').textContent = data.message.split('en')[1].trim();
                }
            }

            if (data.type === 'done') {
                eventSource.close();
                runBtn.disabled = false;
                const doneMsg = document.createElement('div');
                doneMsg.innerHTML = '<br><span class="success-msg">Cierre Completado en Postgres.</span>';
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
