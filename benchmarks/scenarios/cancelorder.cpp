#include "../workload.hpp"
#include "orderbook.hpp"
#include <benchmark/benchmark.h>
#include <cstddef>

static void BM_CancelOrderMiss(benchmark::State& state) {
    OrderBook book;
    OrderID id {0};
    
    // We change the ID so that the compiler doesn't inline the loop.
    // It cannot be optmized because cancelOrder() returns void.
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
        book.addOrder(OrderType::GTC, Side::buy, spec.price, spec.qty);
    }

    // OrderIDs start from 1, so we don't have to save the IDs
    // since we already know the IDs go from 1 to WORKLOAD_SIZE
    // We change the ID so that the compiler doesn't inline the loop.
    // It cannot be optmized because cancelOrder() returns void.
    OrderID id {1};
    for (auto _ : state) {
        benchmark::DoNotOptimize(id);
        book.cancelOrder(id++);
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

