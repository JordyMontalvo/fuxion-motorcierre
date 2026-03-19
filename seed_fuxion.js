const { MongoClient } = require('mongodb');

async function sedFuxion() {
    const uri = "mongodb://127.0.0.1:27017";
    const client = new MongoClient(uri);

    try {
        await client.connect();
        const db = client.db('cierre_db');
        const col = db.collection('usuarios');

        console.log("🚀 Normalizando base de datos para Fuxion (PV4, CV, Jerarquía)...");

        // Usaremos userId como base. Asumiremos que el campo ya existe.
        // Simularemos PV4: mayoría 0-100, algunos 200+
        const pvs = [0, 40, 60, 100, 150, 200, 400, 800];
        
        // Para 10 millones, el bulk update es necesario
        let bulk = col.initializeUnorderedBulkOp();
        let count = 0;
        
        const cursor = col.find({});
        while (await cursor.hasNext()) {
            const doc = await cursor.next();
            const pv = pvs[Math.floor(Math.random() * pvs.length)];
            
            bulk.find({ _id: doc._id }).updateOne({
                $set: {
                    pv4: pv,
                    cv: pv * 0.9,
                    // El sponsorId será determinista para no saturar la DB: (userId-1)/4
                    // Esto permite reconstruir el árbol en C++ sin buscar en la DB
                }
            });

            count++;
            if (count % 10000 === 0) {
                await bulk.execute();
                bulk = col.initializeUnorderedBulkOp();
                process.stdout.write(`\rProcesados: ${count}`);
            }
        }
        
        if (count % 10000 !== 0) await bulk.execute();
        console.log("\n✅ DB normalizada con campos pv4 y cv.");

    } finally {
        await client.close();
    }
}

sedFuxion().catch(console.error);
