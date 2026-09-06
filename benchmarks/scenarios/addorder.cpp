#include "../workload.hpp"
#include "orderbook.hpp"
#include <benchmark/benchmark.h>
#include <cstddef>

static void BM_AddOrder(benchmark::State& state) {
    constexpr uint64_t WORKLOAD_SIZE = 1'000'000;
    constexpr uint32_t SEED = 42;

    OrderSpecs workload = generateWorkload(WORKLOAD_SIZE, SEED);
    OrderBook book;
    size_t i {0};
    
    for (auto _ : state) {
        if (i >= workload.size()) i = 0;
        const auto& spec = workload[i++];

        auto result = spec.isMarket
            ? book.addMarketOrder(spec.side, spec.qty)
            : book.addOrder(spec.type, spec.side, spec.price, spec.qty);

        benchmark::DoNotOptimize(result); // so that compiler doesn't delete result given how is never used
    }
}

BENCHMARK(BM_AddOrder)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(10)
    ->ReportAggregatesOnly(true);
