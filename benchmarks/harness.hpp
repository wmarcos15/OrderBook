#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct BenchmarkResult {
    std::string scenarioName;
    uint64_t ops;
    long long totalDurationNs;
    double opsPerSec;
    std::vector<long long> rawLatencies;
};

struct Stats {
    long long min;
    long long p50;
    long long p95;
    long long p99;
    long long p999;
    long long max;
};

BenchmarkResult measureThroughput(const std::string& name, uint64_t warmup, uint64_t iterations, std::function<void()> fn);
BenchmarkResult measureLatency(const std::string& name, uint64_t warmup, uint64_t iterations, std::function<void()> fn);
void printResult(const BenchmarkResult& result);
