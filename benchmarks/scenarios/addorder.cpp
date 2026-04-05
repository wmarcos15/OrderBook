#include "addorder.hpp"
#include "../workload.hpp"
#include "../harness.hpp"
#include "orderbook.hpp"
#include <cstddef>

constexpr double WARMUP_PERCENTAGE = 0.05;

void benchmarkAddOrderThroughput(uint32_t seed) {
    constexpr uint64_t iterations = 1'000'000;
    constexpr uint64_t warmup = static_cast<uint64_t>(iterations * WARMUP_PERCENTAGE);
    
    auto workload = generateWorkload(warmup + iterations, seed);
    OrderBook book;
    
    size_t i {0};
    auto fn = [&]() {
        if (i >= workload.size()) i = 0;
        const auto& spec = workload[i++];
        if (spec.isMarket) book.addMarketOrder(spec.side, spec.qty);
        else book.addOrder(spec.type, spec.side, spec.price, spec.qty);
    };

    BenchmarkResult result = measureThroughput(SCENARIO_ADDORDER_THROUGHPUT, warmup, iterations, fn);
    printResult(result);
}

void benchmarkAddOrderLatency(uint32_t seed) {
    constexpr uint64_t iterations = 100'000;
    constexpr uint64_t warmup = static_cast<uint64_t>(iterations * WARMUP_PERCENTAGE);
    
    auto workload = generateWorkload(warmup + iterations, seed);
    OrderBook book;

    size_t i {0};
    auto fn = [&]() {
        if (i >= workload.size()) i = 0;
        const auto& spec = workload[i++];
        if (spec.isMarket) book.addMarketOrder(spec.side, spec.qty);
        else book.addOrder(spec.type, spec.side, spec.price, spec.qty);
    };

    BenchmarkResult result = measureLatency(SCENARIO_ADDORDER_LATENCY, warmup, iterations, fn);
    printResult(result);
}

