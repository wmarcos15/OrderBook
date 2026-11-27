#include "../workload.hpp"
#include "orderbook.hpp"
#include <benchmark/benchmark.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

using namespace orderbook;

static void BM_CancelOrderMiss(benchmark::State& state) {
    OrderBook book;
    OrderID id {0};
    
    // Changing the ID stops the compiler from optimising the loop away: cancelOrder() returns void
    for (auto _ : state) {
        benchmark::DoNotOptimize(id);
        book.cancelOrder(id++);
        benchmark::ClobberMemory();
    }
}

static void BM_CancelOrderHit(benchmark::State& state) {
    constexpr uint64_t WORKLOAD_SIZE = 2'000'000;
    constexpr uint32_t SEED = 42;
    OrderSpecs workload = generateWorkload(WORKLOAD_SIZE, SEED);

    OrderBook book;

    for (size_t i {0}; i < WORKLOAD_SIZE; ++i) {
        const auto& spec = workload[i];
        // All on the same side so that they don't fill each other
        // All are GTC so that they are not cancelled
        if (spec.isMarket) continue;  // no price; can't rest in the book anyway
        book.addOrder(spec.owner, OrderType::GTC, Side::buy, spec.price, spec.qty);
    }

    // IDs go from 1 to WORKLOAD_SIZE, so they need not be saved
    // Changing the ID stops the compiler from optimising the loop away: cancelOrder() returns void
    OrderID id {1};
    for (auto _ : state) {
        benchmark::DoNotOptimize(id);
        book.cancelOrder(id++);
        benchmark::ClobberMemory();
    }
}

// BM_CancelOrderHit cancels in ID order, where neighbouring IDs help the cache.
// Real cancels come in no particular order, so here the IDs are shuffled.
static void BM_CancelOrderShuffled(benchmark::State& state) {
    constexpr uint64_t WORKLOAD_SIZE = 2'000'000;
    constexpr uint32_t SEED = 42;
    constexpr uint32_t SHUFFLE_SEED = 7;
    OrderSpecs workload = generateWorkload(WORKLOAD_SIZE, SEED);

    OrderBook book;
    std::vector<OrderID> ids;
    ids.reserve(WORKLOAD_SIZE);

    for (size_t i {0}; i < WORKLOAD_SIZE; ++i) {
        const auto& spec = workload[i];
        // All on the same side so that they don't fill each other
        // All are GTC so that they are not cancelled
        if (spec.isMarket) continue;  // no price; can't rest in the book anyway
        ids.push_back(book.addOrder(spec.owner, OrderType::GTC, Side::buy, spec.price, spec.qty).orderID);
    }

    std::mt19937 rng {SHUFFLE_SEED};
    std::shuffle(ids.begin(), ids.end(), rng);

    // Iterations are pinned well below the number of IDs, so the index stays in bounds
    size_t i {0};
    for (auto _ : state) {
        benchmark::DoNotOptimize(i);
        book.cancelOrder(ids[i++]);
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_CancelOrderMiss)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(10)
    ->ReportAggregatesOnly(true);

BENCHMARK(BM_CancelOrderHit)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(10)
    ->Iterations(100'000) // with 10 repetitions consumes exactly 1M IDs against a 2M pool
    ->ReportAggregatesOnly(true);

BENCHMARK(BM_CancelOrderShuffled)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(10)
    ->Iterations(100'000)
    ->ReportAggregatesOnly(true);

