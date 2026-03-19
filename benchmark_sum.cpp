#include <iostream>
#include <vector>
#include <numeric>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>
#include <string>
#include <thread>
#include <future>
#include <functional>

// ─── ANSI Colors para logs bonitos ───────────────────────────────────────────
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define GREEN   "\033[32m"
#define CYAN    "\033[36m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define RED     "\033[31m"
#define BLUE    "\033[34m"

// ─── Utilidad de tiempo ───────────────────────────────────────────────────────
using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration  = std::chrono::duration<double, std::milli>; // en milisegundos

static TimePoint globalStart;

void logHeader(const std::string& title) {
    std::cout << "\n" << BOLD << CYAN
              << "╔══════════════════════════════════════════════════╗\n"
              << "║  " << std::left << std::setw(48) << title << "║\n"
              << "╚══════════════════════════════════════════════════╝"
              << RESET << "\n";
}

void logStep(const std::string& msg) {
    auto now     = Clock::now();
    double elapsed = Duration(now - globalStart).count();
    std::cout << BLUE << "[" << BOLD << std::fixed << std::setprecision(3)
              << elapsed << " ms" << RESET << BLUE << "] "
              << RESET << msg << "\n";
}

void logResult(const std::string& label, double ms, long long result) {
    std::cout << GREEN << "  ✔ " << BOLD << std::left << std::setw(30) << label
              << RESET << GREEN
              << "| Tiempo: " << YELLOW << BOLD << std::fixed << std::setprecision(4)
              << ms << " ms" << RESET << GREEN
              << "  | Suma: " << MAGENTA << result << RESET << "\n";
}

void logSeparator() {
    std::cout << CYAN << "  ──────────────────────────────────────────────────\n" << RESET;
}

// ─── Constante principal ──────────────────────────────────────────────────────
constexpr int N = 10'000'000; // 10 millones de usuarios

// ─── MÉTODO 1: Loop clásico ───────────────────────────────────────────────────
long long sumLoop(const std::vector<int>& arr) {
    long long total = 0;
    for (int i = 0; i < (int)arr.size(); ++i)
        total += arr[i];
    return total;
}

// ─── MÉTODO 2: Range-based for (moderno) ─────────────────────────────────────
long long sumRangeFor(const std::vector<int>& arr) {
    long long total = 0;
    for (const auto& x : arr)
        total += x;
    return total;
}

// ─── MÉTODO 3: std::accumulate ────────────────────────────────────────────────
long long sumAccumulate(const std::vector<int>& arr) {
    return std::accumulate(arr.begin(), arr.end(), 0LL);
}

// ─── MÉTODO 4: Unroll manual x4 (simula SIMD en software) ────────────────────
long long sumUnrolled4(const std::vector<int>& arr) {
    long long a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int i = 0;
    int n = (int)arr.size();
    for (; i + 3 < n; i += 4) {
        a0 += arr[i];
        a1 += arr[i+1];
        a2 += arr[i+2];
        a3 += arr[i+3];
    }
    long long total = a0 + a1 + a2 + a3;
    for (; i < n; ++i) total += arr[i];
    return total;
}

// ─── MÉTODO 5: Unroll manual x8 ──────────────────────────────────────────────
long long sumUnrolled8(const std::vector<int>& arr) {
    long long a0=0, a1=0, a2=0, a3=0, a4=0, a5=0, a6=0, a7=0;
    int i = 0;
    int n = (int)arr.size();
    for (; i + 7 < n; i += 8) {
        a0 += arr[i];   a1 += arr[i+1];
        a2 += arr[i+2]; a3 += arr[i+3];
        a4 += arr[i+4]; a5 += arr[i+5];
        a6 += arr[i+6]; a7 += arr[i+7];
    }
    long long total = a0+a1+a2+a3+a4+a5+a6+a7;
    for (; i < n; ++i) total += arr[i];
    return total;
}

// ─── MÉTODO 6: Multithreading (divide en N cores) ────────────────────────────
long long sumMultithread(const std::vector<int>& arr) {
    unsigned int nThreads = std::thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 4;

    int n         = (int)arr.size();
    int chunkSize = n / nThreads;

    std::vector<std::future<long long>> futures;
    futures.reserve(nThreads);

    for (unsigned int t = 0; t < nThreads; ++t) {
        int start = t * chunkSize;
        int end   = (t == nThreads - 1) ? n : start + chunkSize;

        futures.push_back(std::async(std::launch::async,
            [&arr, start, end]() {
                long long partial = 0;
                for (int i = start; i < end; ++i)
                    partial += arr[i];
                return partial;
            }
        ));
    }

    long long total = 0;
    for (auto& f : futures)
        total += f.get();
    return total;
}

// ─── Runner genérico con cronómetro ──────────────────────────────────────────
struct BenchResult {
    std::string name;
    double      ms;
    long long   sum;
};

template <typename Func>
BenchResult bench(const std::string& name, Func fn, const std::vector<int>& arr) {
    logStep("  Ejecutando: " + name + " ...");
    auto t0  = Clock::now();
    long long result = fn(arr);
    auto t1  = Clock::now();
    double ms = Duration(t1 - t0).count();
    return {name, ms, result};
}

// ─── MAIN ─────────────────────────────────────────────────────────────────────
int main() {
    globalStart = Clock::now();

    logHeader("BENCHMARK: Suma de 10 Millones de Elementos");
    std::cout << BOLD << "  Simulación de cierre de 10M usuarios\n" << RESET;
    std::cout << "  Fecha/hora aproximada: " YELLOW "2026-03-13 11:37 CST" RESET "\n";

    // ── PASO 1: Allocar memoria ───────────────────────────────────────────────
    logStep("PASO 1: Allocando arreglo de " + std::to_string(N) + " enteros...");
    std::vector<int> arr;
    arr.reserve(N);
    logStep("  -> Memoria reservada: " +
            std::to_string(N * sizeof(int) / 1024 / 1024) + " MB");

    // ── PASO 2: Llenar con números ────────────────────────────────────────────
    logStep("PASO 2: Generando 10M números aleatorios (1..100)...");
    auto t_fill_start = Clock::now();
    {
        std::mt19937 rng(42); // semilla fija para reproducibilidad
        std::uniform_int_distribution<int> dist(1, 100);
        for (int i = 0; i < N; ++i)
            arr.push_back(dist(rng));
    }
    double fillMs = Duration(Clock::now() - t_fill_start).count();
    logStep("  -> Generación completada en " + std::to_string((int)fillMs) + " ms");

    // ── PASO 3: Correr benchmarks ─────────────────────────────────────────────
    logHeader("PASO 3: Benchmarks de Suma");
    std::cout << "\n";

    std::vector<BenchResult> results;
    results.push_back(bench("Loop clásico (índice)",  sumLoop,        arr));  logSeparator();
    results.push_back(bench("Range-based for",        sumRangeFor,    arr));  logSeparator();
    results.push_back(bench("std::accumulate",        sumAccumulate,  arr));  logSeparator();
    results.push_back(bench("Unroll x4",              sumUnrolled4,   arr));  logSeparator();
    results.push_back(bench("Unroll x8",              sumUnrolled8,   arr));  logSeparator();
    results.push_back(bench("Multithreading",         sumMultithread, arr));

    // ── PASO 4: Resultados ────────────────────────────────────────────────────
    logHeader("PASO 4: Tabla de Resultados");
    std::cout << "\n";

    double bestMs   = 1e18;
    std::string bestName;
    for (auto& r : results) {
        logResult(r.name, r.ms, r.sum);
        if (r.ms < bestMs) { bestMs = r.ms; bestName = r.name; }
    }

    std::cout << "\n" << BOLD << GREEN
              << "  🏆 GANADOR: " << bestName
              << " (" << std::fixed << std::setprecision(4) << bestMs << " ms)"
              << RESET << "\n";

    // ── PASO 5: Resumen final ─────────────────────────────────────────────────
    logHeader("PASO 5: Resumen de Escalabilidad");
    auto& ref = results[0];
    std::cout << "\n";
    for (auto& r : results) {
        double speedup = ref.ms / r.ms;
        std::cout << "  " << BOLD << std::left << std::setw(30) << r.name << RESET
                  << " speedup: " << YELLOW << std::fixed << std::setprecision(2)
                  << speedup << "x" << RESET << "\n";
    }

    double totalElapsed = Duration(Clock::now() - globalStart).count();
    std::cout << "\n" << BOLD << CYAN
              << "  ⏱  Tiempo total del benchmark: "
              << std::fixed << std::setprecision(3) << totalElapsed << " ms ("
              << totalElapsed / 1000.0 << " s)\n" << RESET;

    std::cout << "\n" << BOLD << MAGENTA
              << "  ✅ Test completado. Listo para escalar a cierre de 10M usuarios.\n"
              << RESET << "\n";

    return 0;
}
