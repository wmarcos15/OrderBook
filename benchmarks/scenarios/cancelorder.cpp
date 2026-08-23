#include "cancelorder.hpp"
#include "orderbook.hpp"
#include "../harness.hpp"
#include "../workload.hpp"
#include "types.hpp"
#include <vector>

constexpr double WARMUP_PERCENTAGE = 0.05;

void benchmarkCancelOrderHitThroughput(uint32_t seed) {
    constexpr uint64_t iterations = 1'000'000;
    constexpr uint64_t warmup = static_cast<uint64_t>(iterations * WARMUP_PERCENTAGE);

    OrderBook book;
    OrderSpecs workload = generateWorkload(iterations + warmup, seed);

    // Populate the book and get the IDs
    std::vector<OrderID> warmupIDs;
    std::vector<OrderID> measureIDs;
    warmupIDs.reserve(warmup);
    measureIDs.reserve(iterations);
    for (size_t i {0}; i < warmup + iterations; ++i) {
        // Same side and type so that we don't lose IDs
        auto [id, trades] = book.addOrder(OrderType::GTC, Side::buy, workload[i].price, workload[i].qty);
        if (i < warmup) warmupIDs.push_back(id);
        else measureIDs.push_back(id);
    }


    size_t wi {0};
    size_t mi {0};
    auto fn = [&]() {
        if (wi < warmup) book.cancelOrder(warmupIDs[wi++]);
        else book.cancelOrder(measureIDs[mi++]);
    };

    BenchmarkResult result = measureThroughput(SCENARIO_CANCELORDER_HIT_THROUGHPUT, warmup, iterations, fn);
    printResult(result);
}

void benchmarkCancelOrderMissThroughput(uint32_t seed) {
    constexpr uint64_t iterations = 1'000'000;
    constexpr uint64_t warmup = static_cast<uint64_t>(iterations * WARMUP_PERCENTAGE);
    
    OrderBook book;

    auto fn = [&]() {
        book.cancelOrder(0); // doesn't exist
    };

    BenchmarkResult result = measureThroughput(SCENARIO_CANCELORDER_MISS_THROUGHPUT, warmup, iterations, fn);
    printResult(result);
}

void benchmarkCancelOrderHitLatency(uint32_t seed);
void benchmarkCancelOrderMissLatency(uint32_t seed);

