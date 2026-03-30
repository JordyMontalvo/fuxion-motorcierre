/*
 ════════════════════════════════════════════════════════════════════════════════
   CIERRE REALISTA — CONEXIÓN DIRECTA A MONGODB
   Calcula el residual de 10 millones de usuarios desde la base de datos.
 ════════════════════════════════════════════════════════════════════════════════
*/

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <string>
#include <cmath>
#include <sstream>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;
namespace mc = mongocxx;

// ─── Colores ──────────────────────────────────────────────────────────────────
#define RST  "\033[0m"
#define BLD  "\033[1m"
#define CYN  "\033[36m"
#define GRN  "\033[32m"
#define YLW  "\033[33m"
#define MGT  "\033[35m"
#define RED  "\033[31m"
#define BLU  "\033[34m"
#define WHT  "\033[37m"

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

int main() {
    T0 = Clock::now();

    logH("CIERRE MONGO — Procesando 10M Usuarios desde DB");
    
    // ══ Inicializar driver ════════════════════════════════════════════════════
    try {
        mc::instance inst{};
        mc::client   client{ mc::uri{"mongodb://127.0.0.1:27017"} };
        auto db  = client["cierre_db"];
        auto col = db["usuarios"];

        logI("Conexión establecida: cierre_db.usuarios");

        // ══ FASE 1: Contar registros ══════════════════════════════════════════
        logS("Contando registros existentes ...");
        long long totalDocs = col.count_documents({});
        logI("Total registros en la base de datos: " + fmt(totalDocs));

        // ══ FASE 2: Recorrido + Cálculo Residual ══════════════════════════════
        logH("EJECUTANDO CÁLCULO DE CIERRE");
        logS("Recorriendo cursor y calculando residuales ...");

        auto tCalc = Clock::now();
        long long count  = 0;
        double    totalResid = 0.0;
        long long deudores   = 0;
        long long aFavor     = 0;
        long long cero       = 0;
        double    maxDeuda   = -1e18;
        double    maxFavor   =  1e18;
        int       uidMaxD    = 0, uidMaxF = 0;

        auto cursor = col.find({});
        for (auto&& doc : cursor) {
            double sa = doc["saldoAnterior"].get_double().value;
            double ca = doc["totalCargos"].get_double().value;
            double ab = doc["totalAbonos"].get_double().value;
            double ti = doc["tasaInteres"].get_double().value;
            int    uid = doc["userId"].get_int32().value;

            double residual = sa + ca - ab + (sa * ti / 100.0);
            totalResid += residual;
            count++;

            if (residual > 0.001) {
                deudores++;
                if (residual > maxDeuda) { maxDeuda = residual; uidMaxD = uid; }
            } else if (residual < -0.001) {
                aFavor++;
                if (residual < maxFavor) { maxFavor = residual; uidMaxF = uid; }
            } else {
                cero++;
            }

            // Progreso cada 1M
            if (count % 1000000 == 0) {
                logS("Procesados " + fmt(count) + " usuarios ...");
            }
        }

        double calcMs = Ms(Clock::now() - tCalc).count();
        logOK("Cálculo completado", calcMs, fmt(count) + " registros procesados");

        // ══ FASE 3: Resumen Ejecutivo ═════════════════════════════════════════
        logH("RESUMEN EJECUTIVO — CIERRE 10M MONGODB");
        std::cout << "\n";
        
        std::cout << BLD << "  Total usuarios procesados:       " << YLW << fmt(count) << RST << "\n";
        std::cout << RED << "  Deudores (residual > 0):         " << YLW << fmt(deudores) 
                  << " (" << fmtD(100.0*deudores/count) << "%)" << RST << "\n";
        std::cout << GRN << "  Saldo a favor (residual < 0):    " << YLW << fmt(aFavor) 
                  << " (" << fmtD(100.0*aFavor/count) << "%)" << RST << "\n";
        std::cout << BLD << "  Saldo cero:                      " << YLW << fmt(cero) << RST << "\n";
        
        std::cout << CYN << "  " << std::string(60, '-') << "\n" << RST;
        
        std::cout << BLD << "  Residual TOTAL del portafolio:   " << YLW << "$" << fmtD(totalResid) << RST << "\n";
        std::cout << BLD << "  Residual promedio por usuario:   " << YLW << "$" << fmtD(totalResid / count) << RST << "\n";
        
        std::cout << RED << "  Mayor deuda individual:          " << YLW << "$" << fmtD(maxDeuda) 
                  << " (userId=" << uidMaxD << ")" << RST << "\n";
        std::cout << GRN << "  Mayor saldo a favor:             " << YLW << "$" << fmtD(maxFavor) 
                  << " (userId=" << uidMaxF << ")" << RST << "\n";

        std::cout << "\n" << BLD << CYN
                  << "  ⏱  PIPELINE REAL (desde MongoDB): " 
                  << std::fixed << std::setprecision(3) << calcMs << " ms  ("
                  << calcMs/1000.0 << " segundos)\n" << RST;

        std::cout << BLD << MGT << "\n  ✅ Cierre MongoDB finalizado con éxito.\n" << RST;

    } catch (const std::exception& x) {
        std::cerr << RED << "ERROR: " << x.what() << RST << std::endl;
        return 1;
    }

    return 0;
}
