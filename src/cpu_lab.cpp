#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef _OPENMP
#include <omp.h>
#endif

// Stateless per-index sampling makes the input identical for every thread count.
static uint64_t mix(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

static bool inside(uint64_t index, uint64_t seed) {
    constexpr double scale = 1.0 / 9007199254740992.0;
    const double x = static_cast<double>(mix(seed ^ (index * 2)) >> 11) * scale;
    const double y = static_cast<double>(mix(seed ^ (index * 2 + 1)) >> 11) * scale;
    return x * x + y * y <= 1.0;
}

static uint64_t count_hits(uint64_t n, uint64_t seed, int threads, bool parallel) {
    uint64_t hits = 0;
#ifndef _OPENMP
    (void)threads;
    (void)parallel;
#endif
#ifdef _OPENMP
#pragma omp parallel for reduction(+:hits) num_threads(threads) if(parallel) schedule(static)
#endif
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(n); ++i)
        hits += inside(static_cast<uint64_t>(i), seed);
    return hits;
}

static uint64_t positive_number(const char* value) {
    const std::string s(value);
    if (s.empty() || s[0] == '-' || s[0] == '+') throw std::invalid_argument("expected positive integer");
    size_t end = 0;
    const auto n = std::stoull(s, &end);
    if (end != s.size() || !n || n > static_cast<uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw std::invalid_argument("expected positive integer within int64 range");
    return n;
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--self-test") {
            for (uint64_t n : {1ULL, 2ULL, 17ULL, 10007ULL}) {
                for (uint64_t seed : {0ULL, 42ULL}) {
                    const auto expected = count_hits(n, seed, 1, false);
                    for (int threads : {1, 2, 4, 8})
                        if (count_hits(n, seed, threads, true) != expected) return 1;
                }
            }
            std::cout << "CPU correctness passed\n";
            return 0;
        }
        if (argc != 5) throw std::invalid_argument("usage: cpu_lab <trials> <seed> <threads> <repeats> | --self-test");
        const uint64_t n = positive_number(argv[1]);
        const uint64_t seed = std::stoull(argv[2]);
        const auto requested_threads = positive_number(argv[3]);
        if (requested_threads > static_cast<uint64_t>(std::numeric_limits<int>::max()))
            throw std::invalid_argument("too many threads");
        const int threads = static_cast<int>(requested_threads);
        const auto repeats = positive_number(argv[4]);
        if (threads < 1 || repeats > 1000) throw std::invalid_argument("invalid threads or repeats");
#ifndef _OPENMP
        if (threads != 1) throw std::invalid_argument("OpenMP unavailable; use one thread");
#endif
        const auto oracle = count_hits(n, seed, 1, false);
        std::cout << "trials,seed,threads,repeat,hits,pi_estimate,seconds,backend\n";
        for (uint64_t repeat = 0; repeat < repeats; ++repeat) {
            const auto start = std::chrono::steady_clock::now();
            const auto hits = count_hits(n, seed, threads, true);
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (hits != oracle) throw std::runtime_error("parallel result differs from serial oracle");
            std::cout << n << ',' << seed << ',' << threads << ',' << repeat << ',' << hits << ','
                      << std::setprecision(12) << 4.0 * static_cast<double>(hits) / static_cast<double>(n)
                      << ',' << seconds << ','
#ifdef _OPENMP
                      << "openmp\n";
#else
                      << "serial\n";
#endif
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
