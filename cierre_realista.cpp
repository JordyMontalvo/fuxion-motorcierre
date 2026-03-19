/*
 ════════════════════════════════════════════════════════════════════════════════
   SIMULACIÓN REALISTA — CIERRE DE 10 MILLONES DE USUARIOS
   Pipeline completo:
     [Fase 0] Genera datos CSV  (simula export de BD)
     [Fase 1] Lee archivo del disco
     [Fase 2] Parsea registros
     [Fase 3] Calcula residual por usuario  (secuencial)
     [Fase 4] Calcula residual por usuario  (paralelo)
     [Fase 5] Agrega resultados (totales, deudores, saldo a favor)
     [Fase 6] Escribe reporte de cierre
     [Fase 7] Resumen ejecutivo con tiempos
 ════════════════════════════════════════════════════════════════════════════════
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <thread>
#include <future>
#include <cmath>
#include <cstring>

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
void logOK(const std::string& lbl, double ms, const std::string& extra="") {
    std::cout << GRN << "  ✔ " << BLD << std::left << std::setw(40) << lbl
              << RST << GRN << "→ " << YLW << BLD
              << std::fixed << std::setprecision(3) << ms << " ms" << RST;
    if (!extra.empty()) std::cout << "   " << WHT << extra << RST;
    std::cout << "\n";
}
void logI(const std::string& s) { std::cout << MGT << "  ℹ " << RST << s << "\n"; }
void logW(const std::string& s) { std::cout << YLW << "  ⚠ " << RST << s << "\n"; }

// ─── Números con comas ────────────────────────────────────────────────────────
std::string fmt(long long n) {
    std::string s = std::to_string(std::abs(n));
    for (int i = (int)s.size()-3; i > 0; i -= 3) s.insert(i, ",");
    return (n < 0 ? "-" : "") + s;
}
std::string fmtD(double v) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

// ══════════════════════════════════════════════════════════════════════════════
//  Registro de usuario (lo que vendría de la BD)
// ══════════════════════════════════════════════════════════════════════════════
struct UserRecord {
    int    userId;
    double saldoAnterior;  // saldo al inicio del período
    double totalCargos;    // cargos/consumos del período
    double totalAbonos;    // pagos realizados
    double tasaInteres;    // tasa de interés aplicable (%)
    // Calculado durante el cierre:
    double residual;       // = saldoAnterior + totalCargos - totalAbonos + intereses
};

// ══════════════════════════════════════════════════════════════════════════════
//  FASE 0: Generar archivo CSV (simula exportación de BD)
// ══════════════════════════════════════════════════════════════════════════════
std::string generateCSV(int n) {
    const std::string filename = "/tmp/usuarios_cierre.csv";
    logS("Abriendo archivo para escritura: " + filename);

    auto t0 = Clock::now();

    std::ofstream f(filename, std::ios::binary);
    if (!f) { std::cerr << RED << "ERROR: no se pudo crear el archivo\n" << RST; exit(1); }

    // Cabecera
    f << "user_id,saldo_anterior,total_cargos,total_abonos,tasa_interes\n";

    std::mt19937     rng(42);
    std::uniform_real_distribution<double> saldoDist(-10000.0, 50000.0);
    std::uniform_real_distribution<double> cargosDist(0.0,     8000.0);
    std::uniform_real_distribution<double> abonosDist(0.0,     12000.0);
    std::uniform_real_distribution<double> tasaDist(0.5,       3.5);

    // Buffer grande para escritura rápida
    const size_t BUFSZ = 8 * 1024 * 1024; // 8 MB buffer
    std::string buf;
    buf.reserve(BUFSZ + 256);

    for (int i = 1; i <= n; ++i) {
        // Formato manual (más rápido que sprintf múltiple)
        buf += std::to_string(i);
        buf += ',';
        buf += fmtD(saldoDist(rng));
        buf += ',';
        buf += fmtD(cargosDist(rng));
        buf += ',';
        buf += fmtD(abonosDist(rng));
        buf += ',';
        buf += fmtD(tasaDist(rng));
        buf += '\n';

        if (buf.size() >= BUFSZ) {
            f.write(buf.data(), buf.size());
            buf.clear();
        }
    }
    if (!buf.empty()) f.write(buf.data(), buf.size());
    f.close();

    return filename;
}

// ══════════════════════════════════════════════════════════════════════════════
//  FASE 1+2: Leer y parsear CSV rápido (memoria mapeada manual)
// ══════════════════════════════════════════════════════════════════════════════

// Parser ultra-rápido de double sin usar strtod/atof completo
inline double fastDouble(const char*& p) {
    double sign = 1.0;
    if (*p == '-') { sign = -1.0; ++p; }
    double v = 0.0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p++ - '0'); }
    if (*p == '.') {
        ++p;
        double frac = 0.1;
        while (*p >= '0' && *p <= '9') { v += (*p++ - '0') * frac; frac *= 0.1; }
    }
    return sign * v;
}

inline int fastInt(const char*& p) {
    int v = 0;
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    return v;
}

std::vector<UserRecord> readAndParse(const std::string& filename) {
    // Leer todo el archivo en memoria de un golpe
    logS("Leyendo archivo completo en RAM ...");
    auto t0 = Clock::now();

    std::ifstream f(filename, std::ios::binary | std::ios::ate);
    if (!f) { std::cerr << RED << "ERROR abriendo archivo\n" << RST; exit(1); }
    std::streamsize sz = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<char> raw(sz + 1);
    f.read(raw.data(), sz);
    raw[sz] = '\0';
    f.close();

    double readMs = Ms(Clock::now() - t0).count();
    logOK("Lectura de disco", readMs,
          std::to_string((int)(sz/1024/1024)) + " MB leídos @ " +
          fmtD(sz / 1024.0 / 1024.0 / (readMs/1000.0)) + " MB/s");

    // ── Parsear línea por línea ───────────────────────────────────────────────
    logS("Parseando registros ...");
    auto t1 = Clock::now();

    std::vector<UserRecord> records;
    records.reserve(10'000'001);

    const char* p   = raw.data();
    const char* end = raw.data() + sz;

    // Saltar cabecera
    while (p < end && *p != '\n') ++p;
    ++p; // saltar '\n'

    while (p < end && *p != '\0') {
        UserRecord u;
        u.userId       = fastInt(p);    if (*p == ',') ++p;
        u.saldoAnterior= fastDouble(p); if (*p == ',') ++p;
        u.totalCargos  = fastDouble(p); if (*p == ',') ++p;
        u.totalAbonos  = fastDouble(p); if (*p == ',') ++p;
        u.tasaInteres  = fastDouble(p);
        u.residual     = 0.0;
        // saltar hasta fin de línea
        while (p < end && *p != '\n') ++p;
        if (p < end) ++p;
        records.push_back(u);
    }

    double parseMs = Ms(Clock::now() - t1).count();
    logOK("Parseo de registros", parseMs,
          fmt(records.size()) + " registros @ " +
          fmtD(records.size() / (parseMs/1000.0) / 1e6) + " M reg/s");

    return records;
}

// ══════════════════════════════════════════════════════════════════════════════
//  FASE 3: Cálculo de residual — SECUENCIAL
//  residual = saldoAnterior + totalCargos - totalAbonos
//           + (saldoAnterior * tasaInteres / 100)
// ══════════════════════════════════════════════════════════════════════════════
double calcSecuencial(std::vector<UserRecord>& r) {
    for (auto& u : r)
        u.residual = u.saldoAnterior
                   + u.totalCargos
                   - u.totalAbonos
                   + (u.saldoAnterior * u.tasaInteres / 100.0);
    return 0;
}

// ══════════════════════════════════════════════════════════════════════════════
//  FASE 4: Cálculo de residual — PARALELO (todos los cores)
// ══════════════════════════════════════════════════════════════════════════════
void calcChunk(UserRecord* data, size_t from, size_t to) {
    for (size_t i = from; i < to; ++i) {
        data[i].residual = data[i].saldoAnterior
                         + data[i].totalCargos
                         - data[i].totalAbonos
                         + (data[i].saldoAnterior * data[i].tasaInteres / 100.0);
    }
}

double calcParalelo(std::vector<UserRecord>& r) {
    unsigned int nT = std::thread::hardware_concurrency();
    if (nT == 0) nT = 4;
    size_t n     = r.size();
    size_t chunk = n / nT;
    std::vector<std::thread> threads;
    threads.reserve(nT);
    for (unsigned int t = 0; t < nT; ++t) {
        size_t from = t * chunk;
        size_t to   = (t == nT-1) ? n : from + chunk;
        threads.emplace_back(calcChunk, r.data(), from, to);
    }
    for (auto& th : threads) th.join();
    return 0;
}

// ══════════════════════════════════════════════════════════════════════════════
//  FASE 5: Agregación de resultados
// ══════════════════════════════════════════════════════════════════════════════
struct CierreStats {
    long long  totalUsuarios;
    long long  deudores;        // residual > 0  (deben dinero)
    long long  aFavor;          // residual < 0  (saldo a favor)
    long long  saldoCero;       // residual == 0
    double     sumaResidual;
    double     maxDeuda;
    double     maxFavor;
    int        userMaxDeuda;
    int        userMaxFavor;
};

CierreStats agregar(const std::vector<UserRecord>& r) {
    CierreStats s{};
    s.totalUsuarios = (long long)r.size();
    s.maxDeuda = -1e18;
    s.maxFavor =  1e18;

    for (const auto& u : r) {
        s.sumaResidual += u.residual;
        if (u.residual > 0) {
            ++s.deudores;
            if (u.residual > s.maxDeuda) { s.maxDeuda = u.residual; s.userMaxDeuda = u.userId; }
        } else if (u.residual < 0) {
            ++s.aFavor;
            if (u.residual < s.maxFavor) { s.maxFavor = u.residual; s.userMaxFavor = u.userId; }
        } else {
            ++s.saldoCero;
        }
    }
    return s;
}

// ══════════════════════════════════════════════════════════════════════════════
//  FASE 6: Escribir reporte de cierre
// ══════════════════════════════════════════════════════════════════════════════
void escribirReporte(const std::vector<UserRecord>& r, const CierreStats& st) {
    const std::string out = "/tmp/reporte_cierre.csv";
    logS("Escribiendo reporte → " + out);

    std::ofstream f(out, std::ios::binary);
    f << "user_id,saldo_anterior,total_cargos,total_abonos,tasa_interes,residual,estado\n";

    const size_t BUFSZ = 8*1024*1024;
    std::string buf;
    buf.reserve(BUFSZ + 256);

    for (const auto& u : r) {
        buf += std::to_string(u.userId); buf += ',';
        buf += fmtD(u.saldoAnterior);   buf += ',';
        buf += fmtD(u.totalCargos);     buf += ',';
        buf += fmtD(u.totalAbonos);     buf += ',';
        buf += fmtD(u.tasaInteres);     buf += ',';
        buf += fmtD(u.residual);        buf += ',';
        buf += (u.residual > 0 ? "DEUDOR" : (u.residual < 0 ? "A_FAVOR" : "CERO"));
        buf += '\n';
        if (buf.size() >= BUFSZ) { f.write(buf.data(), buf.size()); buf.clear(); }
    }
    if (!buf.empty()) f.write(buf.data(), buf.size());
    f.close();
}

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════════════════════════════
int main() {
    T0 = Clock::now();

    logH("CIERRE REALISTA — 10 MILLONES DE USUARIOS");
    std::cout << BLD << "  Pipeline: BD export → Leer → Parsear → Calcular → Reportar\n" << RST;

    unsigned int nCores = std::thread::hardware_concurrency();
    logI("Cores disponibles: " + std::to_string(nCores));
    logI("Fecha de cierre:   2026-03-13");

    constexpr int N = 10'000'000;

    // ══ FASE 0: Generar datos ═════════════════════════════════════════════════
    logH("FASE 0: Generar CSV (simula exportación de BD)");
    logS("Generando " + fmt(N) + " registros de usuarios ...");
    auto t0 = Clock::now();
    std::string csvFile = generateCSV(N);
    double genMs = Ms(Clock::now() - t0).count();
    logOK("Escritura CSV completa", genMs,
          "~" + std::to_string((int)(N * 55 / 1024 / 1024)) + " MB escritos");

    // ══ FASE 1+2: Leer y parsear ══════════════════════════════════════════════
    logH("FASE 1+2: Leer archivo de disco y parsear registros");
    auto t1 = Clock::now();
    std::vector<UserRecord> users = readAndParse(csvFile);
    double readParseMs = Ms(Clock::now() - t1).count();
    logI("Total registros cargados: " + fmt(users.size()));

    // ══ FASE 3: Cálculo secuencial ════════════════════════════════════════════
    logH("FASE 3: Calcular residual por usuario (SECUENCIAL)");
    logS("Fórmula: residual = saldo_ant + cargos - abonos + (saldo_ant * tasa / 100)");
    auto t3 = Clock::now();
    calcSecuencial(users);
    double seqMs = Ms(Clock::now() - t3).count();
    logOK("Cálculo secuencial 10M usuarios", seqMs,
          fmtD(N / (seqMs/1000.0) / 1e6) + " M usuarios/s");

    // Reset residuales para medir paralelo limpio
    for (auto& u : users) u.residual = 0.0;

    // ══ FASE 4: Cálculo paralelo ══════════════════════════════════════════════
    logH("FASE 4: Calcular residual por usuario (PARALELO — " +
         std::to_string(nCores) + " cores)");
    auto t4 = Clock::now();
    calcParalelo(users);
    double parMs = Ms(Clock::now() - t4).count();
    logOK("Cálculo paralelo 10M usuarios", parMs,
          fmtD(N / (parMs/1000.0) / 1e6) + " M usuarios/s"
          + " | speedup: " + fmtD(seqMs/parMs) + "x");

    // ══ FASE 5: Agregación ════════════════════════════════════════════════════
    logH("FASE 5: Agregar resultados del cierre");
    auto t5 = Clock::now();
    CierreStats st = agregar(users);
    double aggMs = Ms(Clock::now() - t5).count();
    logOK("Agregación de estadísticas", aggMs,
          fmt(st.totalUsuarios) + " usuarios procesados");

    // ══ FASE 6: Escribir reporte ══════════════════════════════════════════════
    logH("FASE 6: Escribir reporte de cierre");
    auto t6 = Clock::now();
    escribirReporte(users, st);
    double repMs = Ms(Clock::now() - t6).count();
    logOK("Reporte CSV generado", repMs, "/tmp/reporte_cierre.csv");

    // ══ FASE 7: Resumen ejecutivo ═════════════════════════════════════════════
    logH("FASE 7: RESUMEN EJECUTIVO DEL CIERRE");
    std::cout << "\n";

    // Tabla de tiempos
    struct Row { std::string fase; double ms; std::string desc; };
    std::vector<Row> rows = {
        {"Generación CSV (export BD)",   genMs,        "Simula dump de producción"},
        {"Lectura de disco",             0,             ""}, // desglosado en readAndParse
        {"Leer + Parsear registros",     readParseMs,  "I/O + parse completo"},
        {"Cálculo secuencial (1 core)",  seqMs,        "Residual por usuario"},
        {"Cálculo paralelo (" + std::to_string(nCores) + " cores)", parMs, "Residual por usuario"},
        {"Agregación resultados",        aggMs,        "Deudores / A favor / Totales"},
        {"Escritura reporte CSV",        repMs,        "Archivo de salida"},
    };

    std::cout << BLD << CYN
        << "  " << std::left << std::setw(44) << "Fase"
        << std::setw(14) << "Tiempo"
        << "Descripción\n" << RST
        << CYN << "  " << std::string(78, '-') << "\n" << RST;

    double totalCalculo = 0;
    for (auto& r : rows) {
        std::string ms = std::to_string(r.ms).substr(0, std::to_string(r.ms).find('.')+4) + " ms";
        std::cout << "  " << std::left << std::setw(44) << r.fase
                  << YLW << std::setw(14) << ms << RST
                  << WHT << r.desc << RST << "\n";
        if (r.fase.find("Cálculo secuencial") == std::string::npos)
            totalCalculo += r.ms;
    }

    // Solo el pipeline real (sin CSV generator)
    double pipelineReal = readParseMs + parMs + aggMs + repMs;

    std::cout << CYN << "  " << std::string(78, '-') << "\n" << RST;
    std::cout << BLD << "  " << std::left << std::setw(44) << "PIPELINE REAL (sin generar datos)"
              << YLW << std::to_string(pipelineReal).substr(0, std::to_string(pipelineReal).find('.')+4) + " ms"
              << RST << "\n\n";

    // Estadísticas de negocio
    logH("ESTADÍSTICAS DE NEGOCIO — CIERRE 10M USUARIOS");
    std::cout << "\n";
    std::cout << BLD << "  " << std::left << std::setw(38) << "Total usuarios procesados:"
              << YLW << fmt(st.totalUsuarios) << RST << "\n";
    std::cout << BLD << RED << "  " << std::left << std::setw(38) << "Deudores (residual > 0):"
              << YLW << fmt(st.deudores)
              << " (" << fmtD(100.0*st.deudores/st.totalUsuarios) << "%)" << RST << "\n";
    std::cout << BLD << GRN << "  " << std::left << std::setw(38) << "Saldo a favor (residual < 0):"
              << YLW << fmt(st.aFavor)
              << " (" << fmtD(100.0*st.aFavor/st.totalUsuarios) << "%)" << RST << "\n";
    std::cout << BLD << "  " << std::left << std::setw(38) << "Saldo cero:"
              << YLW << fmt(st.saldoCero) << RST << "\n";
    std::cout << BLD << "  " << std::left << std::setw(38) << "Residual TOTAL del portafolio:"
              << YLW << "$" << fmtD(st.sumaResidual) << RST << "\n";
    std::cout << BLD << "  " << std::left << std::setw(38) << "Residual promedio por usuario:"
              << YLW << "$" << fmtD(st.sumaResidual / st.totalUsuarios) << RST << "\n";
    std::cout << BLD << RED << "  " << std::left << std::setw(38) << "Mayor deuda individual:"
              << YLW << "$" << fmtD(st.maxDeuda)
              << " (userId=" + std::to_string(st.userMaxDeuda) + ")" << RST << "\n";
    std::cout << BLD << GRN << "  " << std::left << std::setw(38) << "Mayor saldo a favor:"
              << YLW << "$" << fmtD(st.maxFavor)
              << " (userId=" + std::to_string(st.userMaxFavor) + ")" << RST << "\n";

    // Conclusión de velocidad
    double totalMs = now_ms();
    std::cout << "\n" << BLD << CYN
        << "  ⏱  Tiempo total del programa: "
        << std::fixed << std::setprecision(3) << totalMs << " ms  ("
        << totalMs/1000.0 << " segundos)\n" << RST;

    std::cout << "\n" << BLD << GRN
        << "  ┌─────────────────────────────────────────────────────────┐\n"
        << "  │  PIPELINE REAL (leer + calcular + agregar + reportar)  │\n"
        << "  │  " << YLW << std::fixed << std::setprecision(3) << pipelineReal
        << " ms  ≈  " << pipelineReal/1000.0 << " segundos"
        << GRN << std::string(22 - std::to_string((int)pipelineReal).size(), ' ')
        << "│\n"
        << "  │  Para 10,000,000 usuarios reales de producción         │\n"
        << "  └─────────────────────────────────────────────────────────┘\n"
        << RST << "\n";

    std::cout << BLD << MGT
        << "  ✅ Cierre completado. Reporte disponible en /tmp/reporte_cierre.csv\n"
        << RST << "\n";

    return 0;
}
