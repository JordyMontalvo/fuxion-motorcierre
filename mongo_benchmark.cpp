/*
 ════════════════════════════════════════════════════════════════════════════════
   BENCHMARK MongoDB — Árbol Binario de 10M Nodos desde C++
   Pipeline:
     [Fase 0] Genera 10M documentos (nodos del árbol)
     [Fase 1] Inserta en MongoDB en bulk
     [Fase 2] Query: obtener TODOS los documentos y medir tiempo
     [Fase 3] Query: recorrer + calcular residual en cada documento
     [Fase 4] Query: buscar por userId (O log n equivalente)
     [Fase 5] Resumen ejecutivo
 ════════════════════════════════════════════════════════════════════════════════
*/

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/bulk_write.hpp>
#include <mongocxx/options/insert.hpp>
#include <mongocxx/options/find.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/json.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <string>
#include <cmath>
#include <numeric>
#include <sstream>

using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::kvp;
namespace mc = mongocxx;

// ─── Colores ──────────────────────────────────────────────────────────────────
#define RST  "\033[0m"
#define BLD  "\033[1m"
#define GRN  "\033[32m"
#define CYN  "\033[36m"
#define YLW  "\033[33m"
#define MGT  "\033[35m"
#define RED  "\033[31m"
#define BLU  "\033[34m"
#define WHT  "\033[37m"

// ─── Tiempo ───────────────────────────────────────────────────────────────────
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;
static auto T0 = Clock::now();

double now_ms() { return Ms(Clock::now() - T0).count(); }

void logH(const std::string& s) {
    std::cout << "\n" << BLD << CYN
        << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║  " << std::left << std::setw(60) << s << "║\n"
        << "╚══════════════════════════════════════════════════════════════╝"
        << RST << "\n";
}
void logS(const std::string& s) {
    std::cout << BLU << "[" << BLD << std::fixed << std::setprecision(1)
              << now_ms() << " ms" << RST << BLU << "] " << RST << s << "\n";
}
void logOK(const std::string& lbl, double ms, const std::string& extra = "") {
    std::cout << GRN << "  ✔ " << BLD << std::left << std::setw(42) << lbl
              << RST << GRN << "→ " << YLW << BLD
              << std::fixed << std::setprecision(3) << ms << " ms" << RST;
    if (!extra.empty()) std::cout << "   " << WHT << extra << RST;
    std::cout << "\n";
}
void logI(const std::string& s) { std::cout << MGT << "  ℹ " << RST << s << "\n"; }

std::string fmt(long long n) {
    std::string s = std::to_string(std::abs(n));
    for (int i = (int)s.size()-3; i > 0; i -= 3) s.insert(i, ",");
    return (n < 0 ? "-" : "") + s;
}
std::string fmtD(double v, int prec = 2) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

// ══════════════════════════════════════════════════════════════════════════════
int main() {
    T0 = Clock::now();

    logH("MONGO BENCHMARK — Arbol Binario 10M Nodos desde C++");
    std::cout << BLD << "  MongoDB localhost | DB: cierre_db | Col: usuarios\n" << RST;

    constexpr int N        = 10'000'000;
    constexpr int BATCH_SZ = 100'000;   // inserts por bulk

    // ══ Inicializar driver ════════════════════════════════════════════════════
    mc::instance inst{};
    mc::client   client{ mc::uri{"mongodb://127.0.0.1:27017"} };
    auto db  = client["cierre_db"];
    auto col = db["usuarios"];

    logI("Conexion establecida: mongodb://127.0.0.1:27017");

    // ══ FASE 0: Limpiar coleccion anterior ═══════════════════════════════════
    logH("FASE 0: Limpiar coleccion y crear indice");
    logS("Dropping coleccion previa ...");
    col.drop();

    // Crear indice en userId para busquedas O(log n)
    auto idx = make_document(kvp("userId", 1));
    mongocxx::options::index idx_opts{};
    idx_opts.unique(true);
    col.create_index(idx.view(), idx_opts);
    logI("Indice creado: { userId: 1 }, unique");

    // ══ FASE 1: Insertar 10M documentos en bulk ════════════════════════════
    logH("FASE 1: Insertar 10M documentos (bulk de " +
         std::to_string(BATCH_SZ) + ")");

    std::mt19937 rng(2026);
    std::uniform_int_distribution<int>    idxDist(1, N);
    std::uniform_real_distribution<double> saldoDist(-10000.0, 50000.0);
    std::uniform_real_distribution<double> cargosDist(0.0, 8000.0);
    std::uniform_real_distribution<double> abonosDist(0.0, 12000.0);
    std::uniform_real_distribution<double> tasaDist(0.5, 3.5);

    // IDs únicos shuffled (simula árbol binario con IDs aleatorios)
    std::vector<int> ids(N);
    std::iota(ids.begin(), ids.end(), 1);
    std::shuffle(ids.begin(), ids.end(), rng);

    logS("Iniciando insercion bulk de " + fmt(N) + " documentos ...");
    auto tInsert = Clock::now();

    long long inserted = 0;
    int batches = N / BATCH_SZ;

    std::vector<int> pvs = {0, 40, 60, 100, 150, 200, 400, 800};
    std::uniform_int_distribution<int> pvDist(0, pvs.size() - 1);

    for (int b = 0; b < batches; ++b) {
        std::vector<mc::model::insert_one> ops;
        ops.reserve(BATCH_SZ);

        for (int i = 0; i < BATCH_SZ; ++i) {
            int    uid    = ids[b * BATCH_SZ + i];
            double saldo  = saldoDist(rng);
            double cargos = cargosDist(rng);
            double abonos = abonosDist(rng);
            double tasa   = tasaDist(rng);
            int    pv     = pvs[pvDist(rng)];

            ops.emplace_back(make_document(
                kvp("userId",        uid),
                kvp("saldoAnterior", saldo),
                kvp("totalCargos",   cargos),
                kvp("totalAbonos",   abonos),
                kvp("tasaInteres",   tasa),
                kvp("pv4",           pv),
                kvp("cv",            pv * 0.9)
            ));
        }

        col.bulk_write(ops, mc::options::bulk_write{}.ordered(false));
        inserted += BATCH_SZ;

        // Progreso cada 1M
        if ((b+1) % 10 == 0) {
            double pct  = 100.0 * inserted / N;
            double elap = Ms(Clock::now() - tInsert).count();
            std::cout << "  " << GRN << "  [" << std::fixed << std::setprecision(1)
                      << pct << "%] " << fmt(inserted) << " docs | "
                      << fmtD(elap) << " ms\n" << RST;
        }
    }

    double insertMs = Ms(Clock::now() - tInsert).count();
    logOK("Insercion total " + fmt(N) + " docs", insertMs,
          fmtD(N / (insertMs/1000.0) / 1000.0) + "K docs/s");

    // Contar en DB
    long long dbCount = (long long)col.count_documents({});
    logI("Documentos en MongoDB: " + fmt(dbCount));

    // ══ FASE 2: Query — obtener TODOS los documentos ══════════════════════
    logH("FASE 2: Query — leer TODOS los 10M documentos de MongoDB");
    logS("Ejecutando find({}) sin filtro ...");

    auto tRead = Clock::now();
    long long countRead = 0;
    double    sumSaldo  = 0.0;

    {
        auto cursor = col.find({});
        for (auto&& doc : cursor) {
            countRead++;
            sumSaldo += doc["saldoAnterior"].get_double().value;
        }
    }

    double readMs = Ms(Clock::now() - tRead).count();
    logOK("Lectura de " + fmt(countRead) + " docs", readMs,
          fmtD(countRead / (readMs/1000.0) / 1e6) + " M docs/s");
    logI("Suma saldos anteriores: $" + fmtD(sumSaldo));

    // ══ FASE 3: Query — recorrer + calcular residual en cada doc ══════════
    logH("FASE 3: Recorrer todos + calcular residual por usuario");
    logS("Fórmula: residual = saldoAnt + cargos - abonos + (saldoAnt * tasa / 100)");

    auto tCalc = Clock::now();
    long long countCalc  = 0;
    double    totalResid = 0.0;
    long long deudores   = 0;
    long long aFavor     = 0;
    double    maxDeuda   = -1e18;
    double    maxFavor   =  1e18;
    int       uidMaxD    = 0, uidMaxF = 0;

    {
        auto cursor = col.find({});
        for (auto&& doc : cursor) {
            double sa = doc["saldoAnterior"].get_double().value;
            double ca = doc["totalCargos"].get_double().value;
            double ab = doc["totalAbonos"].get_double().value;
            double ti = doc["tasaInteres"].get_double().value;
            int    uid= doc["userId"].get_int32().value;

            double residual = sa + ca - ab + (sa * ti / 100.0);
            totalResid += residual;
            ++countCalc;

            if (residual > 0) {
                ++deudores;
                if (residual > maxDeuda) { maxDeuda = residual; uidMaxD = uid; }
            } else {
                ++aFavor;
                if (residual < maxFavor) { maxFavor = residual; uidMaxF = uid; }
            }
        }
    }

    double calcMs = Ms(Clock::now() - tCalc).count();
    logOK("Recorrido + calculo " + fmt(countCalc) + " users", calcMs,
          fmtD(countCalc / (calcMs/1000.0) / 1e6) + " M usuarios/s");

    // ══ FASE 4: Busquedas puntuales (equivalente a busqueda en arbol) ══════
    logH("FASE 4: Busquedas puntuales por userId (indice en MongoDB)");

    // 1 busqueda
    int targetId = ids[N / 2];
    logS("Buscando userId=" + std::to_string(targetId) + " ...");
    auto tS1 = Clock::now();
    {
        auto doc = col.find_one(make_document(kvp("userId", targetId)));
    }
    double s1Ms = Ms(Clock::now() - tS1).count();
    logOK("Busqueda puntual (1 doc)", s1Ms, "userId=" + std::to_string(targetId));

    // 100 busquedas aleatorias
    logS("Ejecutando 100 busquedas aleatorias ...");
    auto tS100 = Clock::now();
    int hits = 0;
    std::uniform_int_distribution<int> pick(0, N-1);
    for (int i = 0; i < 100; ++i) {
        int uid = ids[pick(rng)];
        if (col.find_one(make_document(kvp("userId", uid)))) ++hits;
    }
    double s100Ms = Ms(Clock::now() - tS100).count();
    logOK("100 busquedas aleatorias", s100Ms,
          "hits: " + std::to_string(hits) + "/100 | prom: " +
          fmtD(s100Ms/100.0, 3) + " ms/query");

    // ══ FASE 5: Resumen ejecutivo ══════════════════════════════════════════
    logH("FASE 5: RESUMEN EJECUTIVO");
    std::cout << "\n";

    struct Row { std::string fase; double ms; std::string nota; };
    std::vector<Row> rows = {
        {"Insercion 10M docs (bulk)",      insertMs, "100K docs por batch"},
        {"Lectura total (find all)",        readMs,   "Cursor + suma saldos"},
        {"Recorrido + calculo residuales",  calcMs,   "Formula completa por doc"},
        {"Busqueda puntual (1 userId)",     s1Ms,     "Indice B-Tree MongoDB"},
        {"100 busquedas aleatorias",        s100Ms,   fmtD(s100Ms/100.0,3)+" ms/cada"},
    };

    std::cout << BLD << CYN
        << "  " << std::left << std::setw(42) << "Operacion"
        << std::setw(14) << "Tiempo"
        << "Notas\n" << RST
        << CYN << "  " << std::string(76, '-') << "\n" << RST;

    for (auto& r : rows) {
        std::string ms = fmtD(r.ms, 3) + " ms";
        std::cout << "  " << std::left << std::setw(42) << r.fase
                  << YLW << std::setw(14) << ms << RST
                  << WHT << r.nota << RST << "\n";
    }

    // Comparativa clave
    std::cout << CYN << "  " << std::string(76, '-') << "\n" << RST;
    std::cout << BLD << GRN
        << "  Pipeline util (lectura + calculo + agregacion):"
        << YLW << " " << fmtD(readMs + calcMs, 3) << " ms\n" << RST;

    // Estadisticas de negocio
    logH("ESTADISTICAS DE NEGOCIO (calculadas desde MongoDB)");
    std::cout << "\n";
    logI("Total usuarios procesados: " + fmt(countCalc));
    std::cout << RED  << "  ℹ " << RST << "Deudores:         " << YLW
              << fmt(deudores) << " (" << fmtD(100.0*deudores/countCalc) << "%)\n" << RST;
    std::cout << GRN  << "  ℹ " << RST << "Saldo a favor:    " << YLW
              << fmt(aFavor) << " (" << fmtD(100.0*aFavor/countCalc) << "%)\n" << RST;
    logI("Residual total portafolio: $" + fmtD(totalResid));
    logI("Residual promedio:         $" + fmtD(totalResid / countCalc));
    std::cout << RED  << "  ℹ " << RST << "Mayor deuda:      " << YLW
              << "$" << fmtD(maxDeuda) << " (userId=" << uidMaxD << ")\n" << RST;
    std::cout << GRN  << "  ℹ " << RST << "Mayor saldo fav:  " << YLW
              << "$" << fmtD(maxFavor) << " (userId=" << uidMaxF << ")\n" << RST;

    double totalMs = now_ms();
    std::cout << "\n" << BLD << CYN
        << "  ⏱  Tiempo total: " << fmtD(totalMs, 3)
        << " ms (" << fmtD(totalMs/1000.0, 3) << " s)\n" << RST;

    std::cout << "\n" << BLD << MGT
        << "  ✅ Benchmark MongoDB completado. 10M usuarios procesados desde DB.\n"
        << RST << "\n";

    return 0;
}
