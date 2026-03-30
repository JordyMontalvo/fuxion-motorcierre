const { MongoClient } = require('mongodb');

async function populate10M() {
    const uri = "mongodb://fuxion_admin:fuxion_password2026@127.0.0.1:27017/?authSource=admin";
    const client = new MongoClient(uri);

    try {
        await client.connect();
        const db = client.db('cierre_db');
        const col = db.collection('usuarios');

        console.log("🔥 Recreando base de datos con 10 millones de usuarios...");
        await col.drop().catch(() => {}); // Limpiar existente
        await col.createIndex({ userId: 1 }, { unique: true });

        const BATCH_SIZE = 25000;
        const TOTAL = 10000000;
        const pvs = [0, 40, 60, 100, 150, 400, 800, 1000, 2000];

        for (let i = 0; i < TOTAL; i += BATCH_SIZE) {
            const batch = [];
            for (let j = 0; j < BATCH_SIZE; j++) {
                const userId = i + j;
                const pv = pvs[Math.floor(Math.random() * pvs.length)];
                batch.push({
                    userId: userId,
                    name: `User_${userId}`,
                    sponsorId: userId === 0 ? null : Math.floor((userId - 1) / 4),
                    pv4: pv,
                    cv: pv * 0.9,
                    isActive: pv >= 40
                });
            }
            await col.insertMany(batch);
            if ((i + BATCH_SIZE) % 100000 === 0) {
                console.log(`\rCargados: ${i + BATCH_SIZE} / ${TOTAL} (${((i + BATCH_SIZE) / TOTAL * 100).toFixed(1)}%)`);
            }
        }

        console.log("\n✅ Base de Datos Poblada con 10,000,000 usuarios.");

    } finally {
        await client.close();
    }
}

populate10M().catch(console.error);
