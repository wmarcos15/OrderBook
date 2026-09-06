#include "benchmark/benchmark.h"
#include "orderbook.hpp"
#include "types.hpp"

static void BM_ModifyOrder_Friendly(benchmark::State& state) {
    constexpr uint64_t POOL_SIZE = 2'000'000;

    OrderBook book;
    constexpr Price PRICE = 10'000;
    constexpr Quantity QTY = 100;

    for (size_t i {0}; i < POOL_SIZE; ++i) {
        book.addOrder(OrderType::GTC, Side::buy, PRICE, QTY);
    }

    OrderID id {1};
    for (auto _ : state) {
        benchmark::DoNotOptimize(id);
        auto result = book.modifyOrder(id++, PRICE, QTY/2);
        benchmark::DoNotOptimize(result);
    }
}

static void BM_ModifyOrder_Unfriendly(benchmark::State& state) {
    constexpr uint64_t POOL_SIZE = 2'000'000;
    OrderBook book;
    constexpr Price PRICE = 10'000;
    constexpr Price NEW_PRICE = 9'999;
    constexpr Quantity QTY = 100;
    for (size_t i {0}; i < POOL_SIZE; ++i) {
        book.addOrder(OrderType::GTC, Side::buy, PRICE, QTY);
    }
    OrderID id {1};
    for (auto _ : state) {
        benchmark::DoNotOptimize(id);
        auto result = book.modifyOrder(id++, NEW_PRICE, QTY);
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(BM_ModifyOrder_Friendly)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(10)
    ->Iterations(100'000) // 100k × 10 repetitions = 1M against a 2M pool
    ->ReportAggregatesOnly(true);

BENCHMARK(BM_ModifyOrder_Unfriendly)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(10)
    ->Iterations(100'000) // 100k × 10 repetitions = 1M against a 2M pool
    ->ReportAggregatesOnly(true);
