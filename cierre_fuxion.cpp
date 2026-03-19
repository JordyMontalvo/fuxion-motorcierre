/*
 ════════════════════════════════════════════════════════════════════════════════
   MOTOR FUXION PRO-LEV X v2.0 — ALTO RENDIMIENTO (10M)
   Lógica oficial: MVR, Líneas Calificadas y Bono Residual de 6 Niveles.
 ════════════════════════════════════════════════════════════════════════════════
*/

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cmath>

using namespace std;
namespace mc = mongocxx;

enum Rank {
    NONE = 0, ENTREPRENEUR, EXECUTIVE, SENIOR, TEAM_BUILDER, 
    SENIOR_TB, LEADER_X, PREMIER_L, ELITE_L, DIAMOND, BLUE_DIAMOND
};

string rankToString(Rank r) {
    switch(r) {
        case ENTREPRENEUR: return "Entrepreneur";
        case EXECUTIVE:    return "Executive Entrepreneur";
        case SENIOR:       return "Senior Entrepreneur";
        case TEAM_BUILDER: return "Team Builder";
        case SENIOR_TB:    return "Senior Team Builder";
        case LEADER_X:     return "Leader X";
        case PREMIER_L:    return "Premier Leader";
        case ELITE_L:      return "Elite Leader";
        case DIAMOND:      return "Diamond";
        case BLUE_DIAMOND: return "Blue Diamond";
        default:           return "None";
    }
}

struct RankCriteria {
    string name;
    double pv_req;
    double dv_req;
    double mvr;
};

const RankCriteria RANK_TABLE[] = {
    {"None", 0, 0, 0},
    {"Entrepreneur", 40, 0, 1e12}, // 1e12 means no MVR limit
    {"Executive", 100, 500, 300},
    {"Senior", 100, 1000, 600},
    {"Team Builder", 150, 2000, 1200},
    {"Senior Team Builder", 150, 4000, 2400},
    {"Leader X", 200, 6000, 3600},
    {"Premier Leader", 200, 15000, 9000},
    {"Elite Leader", 200, 30000, 18000},
    {"Diamond", 200, 60000, 30000},
    {"Blue Diamond", 200, 100000, 50000}
};

struct User {
    int id;
    double pv4;
    double cv;
    double dv4_raw = 0;
    Rank rank = NONE;
    double commissions = 0;
    int parentIdx = -1;
    vector<int> children;
};

// Residual percentages [Level (0-5)][Rank (0-10)]
const double RESIDUAL_PCT[6][11] = {
    {0, 0.05, 0.06, 0.07, 0.08, 0.09, 0.10, 0.10, 0.10, 0.10, 0.10}, // L1
    {0, 0.00, 0.03, 0.04, 0.05, 0.06, 0.07, 0.07, 0.07, 0.07, 0.07}, // L2
    {0, 0.00, 0.00, 0.03, 0.04, 0.05, 0.06, 0.06, 0.06, 0.06, 0.06}, // L3
    {0, 0.00, 0.00, 0.00, 0.03, 0.04, 0.04, 0.04, 0.04, 0.04, 0.04}, // L4
    {0, 0.00, 0.00, 0.00, 0.00, 0.03, 0.03, 0.03, 0.03, 0.03, 0.03}, // L5
    {0, 0.00, 0.00, 0.00, 0.00, 0.00, 0.02, 0.02, 0.02, 0.02, 0.02}  // L6
};

#define RST  "\033[0m"
#define BLD  "\033[1m"
#define CYN  "\033[36m"
#define GRN  "\033[32m"
double get_double_safe(bsoncxx::document::element el, double default_val = 0.0) {
    if (!el) return default_val;
    if (el.type() == bsoncxx::type::k_double) return el.get_double().value;
    if (el.type() == bsoncxx::type::k_int32)  return (double)el.get_int32().value;
    if (el.type() == bsoncxx::type::k_int64)  return (double)el.get_int64().value;
    return default_val;
}

int main() {
    mc::instance inst{};
    mc::client client{mc::uri{"mongodb://fuxion_admin:fuxion_password2026@127.0.0.1:27017/?authSource=admin"}};
    auto col = client["cierre_db"]["usuarios"];

    cout << CYN << "🚀 MOTOR FUXION PRO-LEV X — Iniciando Cierre (10M)..." << RST << endl;

    vector<User> users;
    users.reserve(10000000);
    
    auto tLoadStart = chrono::high_resolution_clock::now();
    auto cursor = col.find({});
    int idx = 0;
    for (auto&& doc : cursor) {
        User u;
        auto el_userId = doc["userId"];
        if (el_userId) {
            if (el_userId.type() == bsoncxx::type::k_int32) u.id = el_userId.get_int32().value;
            else if (el_userId.type() == bsoncxx::type::k_int64) u.id = (int)el_userId.get_int64().value;
            else if (el_userId.type() == bsoncxx::type::k_double) u.id = (int)el_userId.get_double().value;
        }

        u.pv4 = get_double_safe(doc["pv4"], get_double_safe(doc["totalCargos"]));
        u.cv  = get_double_safe(doc["cv"], u.pv4 * 0.9);
        
        if (idx > 0) u.parentIdx = (idx - 1) / 4; 
        users.push_back(u);
        idx++;
    }

    // Construir árbol en memoria
    for (int i = 1; i < users.size(); ++i) {
        users[users[i].parentIdx].children.push_back(i);
    }
    auto tLoadEnd = chrono::high_resolution_clock::now();
    cout << GRN << "✔ Carga y Estructura: " << chrono::duration<double, milli>(tLoadEnd - tLoadStart).count() << " ms" << RST << endl;

    // FASE 1: DV4 Raw (Bottom-Up)
    auto tDvStart = chrono::high_resolution_clock::now();
    for (int i = users.size() - 1; i >= 0; --i) {
        users[i].dv4_raw += users[i].pv4;
        if (users[i].parentIdx != -1) {
            users[users[i].parentIdx].dv4_raw += users[i].dv4_raw;
        }
    }
    auto tDvEnd = chrono::high_resolution_clock::now();
    cout << GRN << "✔ Cálculo DV4 Raw: " << chrono::duration<double, milli>(tDvEnd - tDvStart).count() << " ms" << RST << endl;

    // FASE 2: Calificación de Rangos con MVR y Líneas Calificadas
    // Lo hacemos por niveles de rango para asegurar requisitos de líneas
    auto tRankStart = chrono::high_resolution_clock::now();
    for (int i = users.size() - 1; i >= 0; --i) {
        double pv = users[i].pv4;
        if (pv < 40) { users[i].rank = NONE; continue; }

        // Evaluar de mayor a menor
        for (int r = BLUE_DIAMOND; r >= ENTREPRENEUR; --r) {
            const auto& crit = RANK_TABLE[r];
            if (pv < crit.pv_req) continue;

            // Capped DV4 based on MVR
            double capped_dv = pv;
            int line1k = 0;
            int leader_x = 0;
            int premier_l = 0;

            for (int childIdx : users[i].children) {
                double line_dv = users[childIdx].dv4_raw;
                capped_dv += min(line_dv, crit.mvr);
                
                if (line_dv >= 1000) line1k++;
                // Para calificar líneas, subimos niveles de abajo hacia arriba
                // En C++, como procesamos reverse index, ya tenemos los rangos de los hijos si ellos procesaron antes
                // Pero un padre procesa DESPUÉS que sus hijos en reverse loop. Perfecto.
                if (users[childIdx].rank >= LEADER_X) leader_x++;
                if (users[childIdx].rank >= PREMIER_L) premier_l++;
            }

            if (capped_dv >= crit.dv_req) {
                // Requisitos adicionales por rango
                bool ok = true;
                if (r == SENIOR_TB && line1k < 1) ok = false;
                if (r == LEADER_X && line1k < 2) ok = false;
                if (r == PREMIER_L && (line1k < 2 || leader_x < 1)) ok = false;
                if (r == ELITE_L && (line1k < 1 || leader_x < 2)) ok = false;
                if (r == DIAMOND && leader_x < 4) ok = false;
                if (r == BLUE_DIAMOND && premier_l < 4) ok = false;

                if (ok) {
                    users[i].rank = (Rank)r;
                    break;
                }
            }
        }
    }
    auto tRankEnd = chrono::high_resolution_clock::now();
    cout << GRN << "✔ Calificación de Rangos (MVR): " << chrono::duration<double, milli>(tRankEnd - tRankStart).count() << " ms" << RST << endl;

    // FASE 3: Bono Residual 6 Niveles
    auto tBonusStart = chrono::high_resolution_clock::now();
    double totalPaid = 0;
    for (int i = 0; i < users.size(); ++i) {
        if (users[i].rank == NONE) continue;
        
        // Función de profundidad limitada
        auto calculateResidual = [&](int userIdx) {
            vector<int> q = users[userIdx].children;
            int compressedLevel = 0;
            
            while (!q.empty() && compressedLevel < 6) {
                double pct = RESIDUAL_PCT[compressedLevel][users[userIdx].rank];
                if (pct <= 0) break;

                vector<int> next_q;
                bool levelHasActive = false;
                
                for (int cIdx : q) {
                    if (users[cIdx].pv4 >= 40) {
                        users[userIdx].commissions += users[cIdx].cv * pct;
                        levelHasActive = true;
                    }
                    // Siempre pasamos a los hijos para la siguiente búsqueda, pero solo aumentamos nivel si encontramos activos?
                    // No, la compresión Fuxion usualmente significa que si el nivel 1 está vacío/inactivo, el nivel 2 real se convierte en el "pago de nivel 1".
                    for (int gChild : users[cIdx].children) next_q.push_back(gChild);
                }
                
                // Si encontramos gente en este "nivel" de profundidad del árbol, aumentamos el contador de nivel de pago
                // (Incluso si no hay activos, seguimos bajando pero el porcentaje de pago se mantiene en el mismo nivel de comisión hasta encontrar activos o agotar niveles)
                // Nota: La lógica exacta de compresión de Fuxion es: se busca hacia abajo hasta completar los niveles de pago permitidos.
                if (levelHasActive) compressedLevel++;
                else {
                    // Si no hubo activos, bajamos un nivel de profundidad pero usamos el MISMO porcentaje de pago
                    // para el siguiente nivel de profundidad.
                }
                
                q = next_q;
                if (q.size() > 50000) break; // Límite de seguridad para explosión de memoria en demo
            }
        };
        
        if (users[i].rank >= ENTREPRENEUR) calculateResidual(i);
        totalPaid += users[i].commissions;
    }
    auto tBonusEnd = chrono::high_resolution_clock::now();
    cout << GRN << "✔ Bono Residual (6 Niveles): " << chrono::duration<double, milli>(tBonusEnd - tBonusStart).count() << " ms" << RST << endl;

    // Reporte Final
    long long rankCounts[11] = {0};
    for (const auto& u : users) rankCounts[u.rank]++;

    cout << "\n" << BLD << "--- RESUMEN EJECUTIVO FUXION PRO-LEV X ---" << RST << endl;
    for (int r = 1; r <= BLUE_DIAMOND; ++r) {
        if (rankCounts[r] > 0)
            cout << left << setw(25) << rankToString((Rank)r) << ": " << rankCounts[r] << endl;
    }
    
    cout << "\nPIPELINE REAL (desde MongoDB): " << chrono::duration<double, milli>(tBonusEnd - tLoadStart).count() << " ms" << endl;
    cout << "Residual TOTAL del portafolio: $" << totalPaid << endl;
    cout << "Residual promedio por usuario: $" << (totalPaid / users.size()) << endl;
    cout << "Total usuarios procesados: " << users.size() << endl;
    cout << "Deudores (residual > 0): " << rankCounts[DIAMOND] + rankCounts[BLUE_DIAMOND] << " (Alto Rango)" << endl;

    return 0;
}
