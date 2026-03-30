#include <iostream>
#include <libpq-fe.h>
#include <chrono>
#include <iomanip>
#include <string>
#include <vector>
#include <map>

using Clock = std::chrono::high_resolution_clock;

// Reglas de Rango FuXion Pro-Lev X
struct RankRules {
    int id;
    std::string name;
    double min_pv4;
    double min_dv4;
    double mvr; // Máximo Volumen por Línea
};

const std::vector<RankRules> RULES = {
    {1, "Entrepreneur",       40, 0,      0},
    {2, "Executive Entr.",    100, 500,   300},
    {3, "Senior Entr.",       100, 1000,  600},
    {4, "Team Builder",       150, 2000,  1200},
    {5, "Senior Team B.",     150, 4000,  2400},
    {6, "Leader X",           200, 6000,  3600},
    {7, "Premier Leader",     200, 15000, 9000},
    {8, "Elite Leader",       200, 30000, 18000},
    {9, "Diamond",            200, 60000, 30000}
};

void finish_with_error(PGconn *conn) {
    std::cerr << "PG Error: " << PQerrorMessage(conn) << std::endl;
    PQfinish(conn);
    exit(1);
}

int main() {
    const char *conninfo = "dbname=fuxion_db user=ubuntu password=fuxion2026";
    PGconn *conn = PQconnectdb(conninfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        conninfo = "dbname=fuxion_db user=jordymontalvo";
        conn = PQconnectdb(conninfo);
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Conexión fallida: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            return 1;
        }
    }

    auto start_total = Clock::now();

    std::cout << "\033[1;36m╔═══════════════════════════════════════════════════════════╗\n"
              << "║      MOTOR FUXION PRO-LEV-X — POSTGRESQL ENGINE (v2)      ║\n"
              << "╚═══════════════════════════════════════════════════════════╝\033[0m\n\n";

    // --- FASE 1: LIMPIEZA Y PREPARACIÓN ---
    std::cout << "🚀 \033[1;34mIniciando motor de cierre master (Pro-Lev X)...\033[0m\n";
    PQexec(conn, "DELETE FROM closing_results WHERE period_id = 1;");

    // --- FASE 2: CALCULO PV4 (Personal Volumen) ---
    std::cout << "📥 \033[1;37mCalculando PV4 por Usuario... \033[0m";
    auto t1 = Clock::now();
    std::string pv_q = 
        "INSERT INTO closing_results (user_id, period_id, pv4, dv4, rank_id) "
        "SELECT u.id, 1, COALESCE(SUM(o.qv), 0), 0, 1 "
        "FROM users u "
        "LEFT JOIN orders o ON o.user_id = u.id AND o.period_id = 1 "
        "GROUP BY u.id;";
    PQexec(conn, pv_q.c_str());
    std::cout << "\033[1;32mOK\033[0m (" << std::chrono::duration<double, std::milli>(Clock::now() - t1).count() << "ms)\n";

    // --- FASE 3: VOLUMEN GRUPAL (DV4) CON LTREE ---
    std::cout << "⚙️  \033[1;37mCalculando Rollup DV4 (Sin MVR)... \033[0m";
    auto t3 = Clock::now();
    // Aquí usamos la ventaja de ltree para un rollup masivo
    std::string dv_q = 
        "UPDATE closing_results cr SET dv4 = sub.total_dv4 "
        "FROM ( "
        "  SELECT ancestor.id, SUM(o.qv) as total_dv4 "
        "  FROM users ancestor "
        "  JOIN users descendant ON descendant.path <@ ancestor.path "
        "  JOIN orders o ON o.user_id = descendant.id AND o.period_id = 1 "
        "  GROUP BY ancestor.id "
        ") sub WHERE cr.user_id = sub.id AND cr.period_id = 1;";
    PQexec(conn, dv_q.c_str());
    std::cout << "\033[1;32mOK\033[0m (" << std::chrono::duration<double, std::milli>(Clock::now() - t3).count() << "ms)\n";

    // --- FASE 4: CALIFICACIÓN DE RANGOS REAL (PV4 + DV4 + Rank thresholds) ---
    std::cout << "🎖️  \033[1;37mCalificando Rangos (Cumplimiento de PV4 y Umbrales)... \033[0m";
    auto t5 = Clock::now();
    for (const auto& rule : RULES) {
        if (rule.id == 1) continue; // Entrepreneur es base
        std::string rank_q = "UPDATE closing_results SET rank_id = " + std::to_string(rule.id) + 
            " WHERE pv4 >= " + std::to_string(rule.min_pv4) + 
            " AND dv4 >= " + std::to_string(rule.min_dv4) + 
            " AND period_id = 1;";
        PQexec(conn, rank_q.c_str());
    }
    // Sync a tabla users
    PQexec(conn, "UPDATE users u SET rank_id = cr.rank_id FROM closing_results cr WHERE u.id = cr.user_id AND cr.period_id = 1;");
    std::cout << "\033[1;32mOK\033[0m (" << std::chrono::duration<double, std::milli>(Clock::now() - t5).count() << "ms)\n";

    // --- FASE 5: BONO FAMILIA (6 Niveles con Porcentajes según PDF) ---
    std::cout << "💰 \033[1;37mCalculando Residual Pro-Lev X (6 Niveles)... \033[0m";
    auto t7 = Clock::now();
    // P1: Cargar CV por nivel (usando ltree para profundidad exacta)
    for (int lvl = 1; lvl <= 6; lvl++) {
        std::string lvl_q = 
            "UPDATE closing_results cr SET cv_n" + std::to_string(lvl) + " = sub.cv_total "
            "FROM ( "
            "  SELECT u.id, SUM(o.cv) as cv_total "
            "  FROM users u "
            "  JOIN users sub ON sub.path <@ u.path AND nlevel(sub.path) = nlevel(u.path) + " + std::to_string(lvl) + " "
            "  JOIN orders o ON o.user_id = sub.id AND o.period_id = 1 "
            "  GROUP BY u.id "
            ") sub WHERE cr.user_id = sub.id;";
        PQexec(conn, lvl_q.c_str());
    }
    
    // P2: Aplicar porcentajes (Ej: Lider X paga 10%, 7%, 6%, 4%, 3%, 2%)
    // Simplificación para el motor rápido: Promedio ponderado dinámico por nivel
    PQexec(conn, "UPDATE closing_results SET residual_total = "
                 "(cv_n1 * 0.10) + (cv_n2 * 0.07) + (cv_n3 * 0.06) + "
                 "(cv_n4 * 0.04) + (cv_n5 * 0.03) + (cv_n6 * 0.02) "
                 "WHERE rank_id >= 6;");
    
    PQexec(conn, "UPDATE closing_results SET residual_total = "
                 "(cv_n1 * 0.08) + (cv_n2 * 0.05) + (cv_n3 * 0.04) + (cv_n4 * 0.03) "
                 "WHERE rank_id < 6 AND rank_id >= 4;"); // Team Builder
    
    std::cout << "\033[1;32mOK\033[0m (" << std::chrono::duration<double, std::milli>(Clock::now() - t7).count() << "ms)\n";

    // --- FASE 6: RESUMEN FINAL ---
    std::cout << "\n\033[1;35m--- REPORTAJE DE CUMPLIMIENTO FUXION ---\033[0m\n";
    PGresult *res_st = PQexec(conn, "SELECT rank_id, count(*) FROM closing_results GROUP BY rank_id ORDER BY rank_id");
    for (int i = 0; i < PQntuples(res_st); i++) {
        std::cout << " Rango ID " << PQgetvalue(res_st, i, 0) << " : \033[1;33m" << PQgetvalue(res_st, i, 1) << " Calificados \033[0m\n";
    }
    PQclear(res_st);

    double total_ms = std::chrono::duration<double, std::milli>(Clock::now() - start_total).count();
    std::cout << "\n\033[1;32m✅ CIERRE PRO-LEV X COMPLETADO EN " << total_ms << " ms\033[0m\n";

    PQfinish(conn);
    return 0;
}
