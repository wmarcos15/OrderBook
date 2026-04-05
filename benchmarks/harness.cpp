#include "harness.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>

Stats computeStats(const BenchmarkResult& result) {
    auto sortedLatencies = result.rawLatencies;
    std::sort(sortedLatencies.begin(), sortedLatencies.end());
    auto p = [&](double percentage) {
        return sortedLatencies[static_cast<size_t>(percentage / 100.0 * sortedLatencies.size())];
    };

    Stats stats;
    stats.min = sortedLatencies.front();
    stats.max = sortedLatencies.back();
    stats.p50 = p(50);
    stats.p95 = p(95);
    stats.p99 = p(99);
    stats.p999 = p(99.9);

    return stats;
}

BenchmarkResult measureThroughput(const std::string& name, uint64_t warmup, uint64_t iterations, std::function<void()> fn) {
    BenchmarkResult result;
    result.scenarioName = name;

    for (uint64_t i {0}; i < warmup; ++i) fn();

    auto start = std::chrono::high_resolution_clock::now();
    for (uint64_t i {0}; i < iterations; ++i) fn();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    auto seconds = ns / 1'000'000'000.0;
    double opsPerSec = iterations / seconds;

    result.ops = iterations;
    result.totalDurationNs = ns;
    result.opsPerSec = opsPerSec;

    return result;
}

BenchmarkResult measureLatency(const std::string& name, uint64_t warmup, uint64_t iterations, std::function<void()> fn) {
    BenchmarkResult result;
    result.scenarioName = name;
    result.rawLatencies.reserve(iterations);

    for (uint64_t i {0}; i < warmup; ++i) fn();

    for (uint64_t i {0}; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        fn();
        auto end = std::chrono::high_resolution_clock::now();
        long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        result.rawLatencies.push_back(ns);
    }

    return result;
}

void printResult(const BenchmarkResult& result) {
    std::cout << "=== " << result.scenarioName << " ===\n";
    std::cout << "Throughput\n";
    std::cout << "\tops:\t\t" << result.ops << "\n";
    std::cout << "\tduration:\t" << result.totalDurationNs / 1'000'000 << " ms\n";
    std::cout << "\tops/sec:\t" << static_cast<uint64_t>(result.opsPerSec) << "\n";
    if (!result.rawLatencies.empty()) {
        Stats stats = computeStats(result);
        std::cout << "Latency (ns)\n";
        std::cout << "\tmin:\t" << stats.min << "\n";
        std::cout << "\tp50:\t" << stats.p50 << "\n";
        std::cout << "\tp95:\t" << stats.p95 << "\n";
        std::cout << "\tp99:\t" << stats.p99 << "\n";
        std::cout << "\tp99.9:\t" << stats.p999 << "\n";
        std::cout << "\tmax:\t" << stats.max << "\n";
    }
}

