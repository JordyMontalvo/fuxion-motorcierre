/*
 ════════════════════════════════════════════════════════════════════════════
   BENCHMARK: Árbol Binario de 10 Millones de Nodos
   Prueba de rendimiento para cierre de 10M usuarios
   Operaciones: construcción, traversal, búsqueda, estadísticas
 ════════════════════════════════════════════════════════════════════════════
*/

#include <iostream>
#include <vector>
#include <stack>
#include <queue>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <string>
#include <memory>
#include <functional>
#include <numeric>
#include <cmath>

// ─── Colores ANSI ─────────────────────────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define GREEN   "\033[32m"
#define CYAN    "\033[36m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define RED     "\033[31m"
#define BLUE    "\033[34m"
#define WHITE   "\033[37m"

// ─── Tipos de tiempo ──────────────────────────────────────────────────────────
using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Ms        = std::chrono::duration<double, std::milli>;

static TimePoint programStart;

double elapsed() {
    return Ms(Clock::now() - programStart).count();
}

void logHeader(const std::string& title) {
    std::cout << "\n" << BOLD << CYAN
              << "╔══════════════════════════════════════════════════════════╗\n"
              << "║  " << std::left << std::setw(56) << title << "║\n"
              << "╚══════════════════════════════════════════════════════════╝"
              << RESET << "\n";
}

void logStep(const std::string& msg) {
    std::cout << BLUE << "[" << BOLD << std::fixed << std::setprecision(2)
              << elapsed() << " ms" << RESET << BLUE << "] "
              << RESET << msg << "\n";
}

void logOk(const std::string& label, double ms, const std::string& extra = "") {
    std::cout << GREEN << "  ✔ " << BOLD << std::left << std::setw(36) << label
              << RESET << GREEN << "→ " << YELLOW << BOLD
              << std::fixed << std::setprecision(4) << ms << " ms"
              << RESET;
    if (!extra.empty())
        std::cout << "   " << WHITE << extra << RESET;
    std::cout << "\n";
}

void logInfo(const std::string& msg) {
    std::cout << MAGENTA << "  ℹ " << RESET << msg << "\n";
}

void logSep() {
    std::cout << CYAN << "  ──────────────────────────────────────────────────────\n" << RESET;
}

// ══════════════════════════════════════════════════════════════════════════════
//  ESTRUCTURA: Nodo del árbol
//  Contiene: id de usuario, valor (saldo/residual), punteros
// ══════════════════════════════════════════════════════════════════════════════
struct Node {
    int      userId;   // ID del usuario (clave del BST)
    double   residual; // residual / saldo calculado
    Node*    left;
    Node*    right;

    Node() : userId(0), residual(0.0), left(nullptr), right(nullptr) {}
    Node(int id, double res)
        : userId(id), residual(res), left(nullptr), right(nullptr) {}
};

// ══════════════════════════════════════════════════════════════════════════════
//  ÁRBOL BINARIO DE BÚSQUEDA (BST) — Pool de memoria personalizado
//  Usando un pool evita millones de new/delete individuales (mucho más rápido)
// ══════════════════════════════════════════════════════════════════════════════
class NodePool {
public:
    explicit NodePool(size_t capacity)
        : pool_(capacity), idx_(0) {}

    Node* alloc(int id, double res) {
        if (idx_ >= pool_.size()) return nullptr;
        pool_[idx_] = Node(id, res);
        return &pool_[idx_++];
    }

    size_t used() const { return idx_; }

private:
    std::vector<Node> pool_;
    size_t            idx_;
};

// ══════════════════════════════════════════════════════════════════════════════
//  BST con pool de memoria
// ══════════════════════════════════════════════════════════════════════════════
class BST {
public:
    explicit BST(size_t capacity)
        : pool_(capacity), root_(nullptr), size_(0), maxDepth_(0) {}

    // ── Inserción iterativa (evita stack overflow con 10M nodos) ──────────────
    void insert(int id, double res) {
        Node* nuevo = pool_.alloc(id, res);
        if (!nuevo) return;
        ++size_;

        if (!root_) { root_ = nuevo; return; }

        Node* cur    = root_;
        int   depth  = 1;
        while (true) {
            ++depth;
            if (id < cur->userId) {
                if (!cur->left)  { cur->left  = nuevo; break; }
                cur = cur->left;
            } else {
                if (!cur->right) { cur->right = nuevo; break; }
                cur = cur->right;
            }
        }
        if (depth > maxDepth_) maxDepth_ = depth;
    }

    // ── Búsqueda iterativa ────────────────────────────────────────────────────
    Node* search(int id) const {
        Node* cur = root_;
        while (cur) {
            if (id == cur->userId) return cur;
            cur = (id < cur->userId) ? cur->left : cur->right;
        }
        return nullptr;
    }

    // ── In-Order iterativo → recorre todos los nodos en orden ascendente ──────
    long long inorderSum() const {
        long long count = 0;
        double    total = 0.0;
        std::stack<Node*> stk;
        Node* cur = root_;
        while (cur || !stk.empty()) {
            while (cur) { stk.push(cur); cur = cur->left; }
            cur   = stk.top(); stk.pop();
            total += cur->residual;
            ++count;
            cur   = cur->right;
        }
        (void)total; // suprimir warning
        return count;
    }

    // ── BFS (recorrido por niveles) ───────────────────────────────────────────
    long long bfsCount() const {
        if (!root_) return 0;
        long long count = 0;
        std::queue<Node*> q;
        q.push(root_);
        while (!q.empty()) {
            Node* n = q.front(); q.pop();
            ++count;
            if (n->left)  q.push(n->left);
            if (n->right) q.push(n->right);
        }
        return count;
    }

    // ── Suma de residuales (DFS iterativo) ────────────────────────────────────
    double sumResiduals() const {
        double total = 0.0;
        std::stack<Node*> stk;
        if (root_) stk.push(root_);
        while (!stk.empty()) {
            Node* n = stk.top(); stk.pop();
            total += n->residual;
            if (n->left)  stk.push(n->left);
            if (n->right) stk.push(n->right);
        }
        return total;
    }

    // ── Min / Max (más rápido, sin traversal completo) ────────────────────────
    int minKey() const {
        Node* cur = root_;
        while (cur->left) cur = cur->left;
        return cur->userId;
    }
    int maxKey() const {
        Node* cur = root_;
        while (cur->right) cur = cur->right;
        return cur->userId;
    }

    size_t size()     const { return size_;     }
    int    maxDepth() const { return maxDepth_;  }
    Node*  root()     const { return root_;      }

private:
    NodePool pool_;
    Node*    root_;
    size_t   size_;
    int      maxDepth_;
};

// ══════════════════════════════════════════════════════════════════════════════
//  Helper para format de números con separadores de miles
// ══════════════════════════════════════════════════════════════════════════════
std::string fmtNum(long long n) {
    std::string s = std::to_string(n);
    int i = (int)s.size() - 3;
    while (i > 0) { s.insert(i, ","); i -= 3; }
    return s;
}

// ══════════════════════════════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════════════════════════════
int main() {
    programStart = Clock::now();

    logHeader("ÁRBOL BINARIO — 10 Millones de Nodos (Usuarios)");
    std::cout << BOLD << "  Simulación: cierre de " << YELLOW
              << "10,000,000" << RESET << BOLD << " usuarios con residuales\n" << RESET;

    constexpr int N = 10'000'000;

    // ── PASO 1: Generar IDs y residuales aleatorios ───────────────────────────
    logHeader("PASO 1: Generación de datos de entrada");
    logStep("Generando " + fmtNum(N) + " IDs únicos y residuales ...");

    auto t0 = Clock::now();

    std::vector<int> ids(N);
    std::iota(ids.begin(), ids.end(), 1); // ids 1..10M

    std::mt19937 rng(2026);
    std::shuffle(ids.begin(), ids.end(), rng); // orden aleatorio → BST balanceado

    std::uniform_real_distribution<double> resDist(-5000.0, 5000.0);
    std::vector<double> residuals(N);
    for (auto& r : residuals) r = resDist(rng);

    double genMs = Ms(Clock::now() - t0).count();
    logOk("Generación de datos", genMs,
          "IDs: 1.." + fmtNum(N) + " | Residuales: [-5000, 5000]");
    logInfo("Memoria estimada del arreglo de datos: ~" +
            std::to_string((int)((N * (sizeof(int) + sizeof(double))) / 1024 / 1024)) + " MB");

    // ── PASO 2: Construir el BST ──────────────────────────────────────────────
    logHeader("PASO 2: Construcción del BST (10M nodos)");
    logStep("Reservando pool de memoria para " + fmtNum(N) + " nodos ...");

    // Tamaño de cada nodo: userId(4) + residual(8) + 2 punteros(16) = ~28-32 bytes
    size_t nodeBytes = sizeof(Node);
    logInfo("Tamaño de cada Node: " + std::to_string(nodeBytes) + " bytes");
    logInfo("Memoria total del árbol: ~" +
            std::to_string((size_t)nodeBytes * N / 1024 / 1024) + " MB");

    BST tree(N);

    logStep("Insertando " + fmtNum(N) + " nodos...");
    auto tInsert = Clock::now();

    for (int i = 0; i < N; ++i)
        tree.insert(ids[i], residuals[i]);

    double insertMs = Ms(Clock::now() - tInsert).count();
    logOk("Inserción de " + fmtNum(N) + " nodos", insertMs,
          "(" + std::to_string((int)(N / (insertMs / 1000.0) / 1e6)) + " M nodos/seg)");
    logInfo("Profundidad máxima alcanzada: " + std::to_string(tree.maxDepth()) + " niveles");
    logSep();

    // ── PASO 3: Traversals ────────────────────────────────────────────────────
    logHeader("PASO 3: Traversals del árbol");

    // DFS In-Order
    logStep("In-Order traversal (DFS) ...");
    auto tInorder = Clock::now();
    long long countInorder = tree.inorderSum();
    double inorderMs = Ms(Clock::now() - tInorder).count();
    logOk("In-Order (DFS)", inorderMs,
          "Nodos visitados: " + fmtNum(countInorder));
    logSep();

    // BFS por niveles
    logStep("BFS (recorrido por niveles) ...");
    auto tBfs = Clock::now();
    long long countBfs = tree.bfsCount();
    double bfsMs = Ms(Clock::now() - tBfs).count();
    logOk("BFS por niveles", bfsMs,
          "Nodos visitados: " + fmtNum(countBfs));
    logSep();

    // Suma de residuales (DFS pre-order)
    logStep("Suma de residuales (DFS pre-order) ...");
    auto tSum = Clock::now();
    double totalResidual = tree.sumResiduals();
    double sumMs = Ms(Clock::now() - tSum).count();
    logOk("Suma de residuales", sumMs,
          "Total residual: $" + std::to_string((long long)totalResidual));
    logSep();

    // ── PASO 4: Búsquedas ─────────────────────────────────────────────────────
    logHeader("PASO 4: Benchmark de Búsquedas");

    // Búsqueda de 1 nodo específico
    int target = ids[N / 2];
    logStep("Buscando userId=" + std::to_string(target) + " ...");
    auto tSearch1 = Clock::now();
    Node* found = tree.search(target);
    double search1Ms = Ms(Clock::now() - tSearch1).count();
    std::string foundInfo = found
        ? "residual=$" + std::to_string((int)found->residual)
        : "NO encontrado";
    logOk("Búsqueda puntual (1 nodo)", search1Ms, foundInfo);
    logSep();

    // 1,000 búsquedas aleatorias
    logStep("Ejecutando 1,000 búsquedas aleatorias ...");
    std::uniform_int_distribution<int> idDist(1, N);
    auto tSearch1k = Clock::now();
    int hits = 0;
    for (int i = 0; i < 1000; ++i) {
        if (tree.search(idDist(rng))) ++hits;
    }
    double search1kMs = Ms(Clock::now() - tSearch1k).count();
    logOk("1,000 búsquedas aleatorias", search1kMs,
          "Hits: " + std::to_string(hits) + "/1000 | Prom: " +
          std::to_string(search1kMs / 1000.0).substr(0, 7) + " ms/búsqueda");
    logSep();

    // Min y Max
    logStep("Calculando Min y Max ID ...");
    auto tMinMax = Clock::now();
    int minId = tree.minKey();
    int maxId = tree.maxKey();
    double minMaxMs = Ms(Clock::now() - tMinMax).count();
    logOk("Min/Max del BST", minMaxMs,
          "Min ID: " + std::to_string(minId) + " | Max ID: " + std::to_string(maxId));

    // ── PASO 5: Tabla resumen ─────────────────────────────────────────────────
    logHeader("PASO 5: Tabla Comparativa de Operaciones");
    std::cout << "\n";

    struct Row { std::string op; double ms; std::string nota; };
    std::vector<Row> rows = {
        {"Generación de datos",          genMs,      "Aleatorio, shuffled"},
        {"Inserción 10M nodos",          insertMs,   "BST + pool memoria"},
        {"In-Order traversal (DFS)",     inorderMs,  "Visita ordenada"},
        {"BFS por niveles",              bfsMs,      "Breadth-first"},
        {"Suma de residuales (DFS)",     sumMs,      "Pre-order DFS"},
        {"Búsqueda puntual (1 nodo)",    search1Ms,  "O(log n)"},
        {"1,000 búsquedas aleatorias",   search1kMs, "O(k·log n)"},
        {"Min/Max del árbol",            minMaxMs,   "O(log n)"},
    };

    std::cout << BOLD << CYAN
              << "  " << std::left << std::setw(38) << "Operación"
              << std::setw(14) << "Tiempo (ms)"
              << "Notas\n" << RESET;
    std::cout << CYAN << "  " << std::string(72, '-') << "\n" << RESET;

    double best = 1e18, worst = 0;
    std::string bestOp, worstOp;
    for (auto& r : rows) {
        std::string msStr = std::to_string(r.ms);
        msStr = msStr.substr(0, msStr.find('.')+5);
        std::cout << "  " << std::left << std::setw(38) << r.op
                  << YELLOW << std::setw(14) << (msStr + " ms") << RESET
                  << WHITE << r.nota << RESET << "\n";
        if (r.ms < best)  { best = r.ms;  bestOp  = r.op; }
        if (r.ms > worst) { worst = r.ms; worstOp = r.op; }
    }

    // ── PASO 6: Estadísticas del árbol ────────────────────────────────────────
    logHeader("PASO 6: Estadísticas del Árbol");
    std::cout << "\n";
    logInfo("Nodos totales:        " + fmtNum(tree.size()));
    logInfo("Profundidad máxima:   " + std::to_string(tree.maxDepth()) + " niveles");
    logInfo("Esperada (log2 10M):  ~" + std::to_string((int)std::log2(N)) + " niveles (ideal)");
    logInfo("Nodo size:            " + std::to_string(sizeof(Node)) + " bytes");
    logInfo("Memoria total árbol:  ~" +
            std::to_string((size_t)sizeof(Node) * N / 1024 / 1024) + " MB");
    logInfo("Residual total:       $" + std::to_string((long long)totalResidual));
    logInfo("Residual promedio:    $" +
            std::to_string((int)(totalResidual / N)) + " por usuario");
    logInfo("Rango IDs:            [" + std::to_string(minId) +
            " ... " + std::to_string(maxId) + "]");

    // ── Tiempo total ──────────────────────────────────────────────────────────
    double totalMs = elapsed();
    std::cout << "\n" << BOLD << CYAN
              << "  ⏱  Tiempo total del programa: "
              << std::fixed << std::setprecision(3) << totalMs << " ms ("
              << totalMs / 1000.0 << " s)\n" << RESET;

    std::cout << "\n" << BOLD << GREEN
              << "  🚀 Operación más rápida: " << YELLOW << bestOp  << "\n" << RESET;
    std::cout << BOLD << RED
              << "  🐢 Operación más lenta:  " << YELLOW << worstOp << "\n" << RESET;

    std::cout << "\n" << BOLD << MAGENTA
              << "  ✅ Árbol de 10M usuarios procesado. "
              << "Listo para cálculo de residuales en producción.\n"
              << RESET << "\n";

    return 0;
}
