const { Client } = require('pg');

async function seed() {
    const client = new Client({
        database: 'fuxion_db',
        user: 'ubuntu',
        password: 'fuxion2026'
    });

    await client.connect();
    console.log("Connected to fuxion_db. Seeding 10,000 users...");

    await client.query('TRUNCATE TABLE activity CASCADE');
    await client.query('TRUNCATE TABLE users CASCADE');

    const totalUsers = 10000;
    
    // Insertar raíz
    await client.query("INSERT INTO users (id, name, path, level) VALUES (1, 'FuXion Global', '1', 0)");
    
    const paths = { 1: { path: '1', level: 0 } };

    for (let i = 2; i <= totalUsers; i += 100) {
        let valuesUsers = [];
        let valuesActivity = [];
        const end = Math.min(i + 100, totalUsers + 1);

        for (let j = i; j < end; j++) {
            const sponsorId = Math.floor(Math.random() * (j - 1)) + 1;
            const sp = paths[sponsorId];
            const myPath = `${sp.path}.${j}`;
            const myLevel = sp.level + 1;
            paths[j] = { path: myPath, level: myLevel };

            valuesUsers.push(`(${j}, 'User ${j}', ${sponsorId}, '${myPath}', ${myLevel})`);
            valuesActivity.push(`(${j}, ${(Math.random() * 500).toFixed(2)}, 1)`);
        }

        await client.query(`INSERT INTO users (id, name, sponsor_id, path, level) VALUES ${valuesUsers.join(',')}`);
        await client.query(`INSERT INTO activity (user_id, pv4, period_id) VALUES ${valuesActivity.join(',')}`);

        if (i % 2000 === 0) console.log(`Inserted ${i} users...`);
    }

    console.log("Seeding complete!");
    await client.end();
}

seed().catch(console.error);
