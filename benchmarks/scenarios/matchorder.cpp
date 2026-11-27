#include "orderbook.hpp"
#include <benchmark/benchmark.h>
#include <cstdint>

using namespace orderbook;

// Resting orders belong to MAKER and sweeping orders to TAKER, so self-trade
// prevention never fires and only matching is measured.
constexpr OwnerID MAKER = 1;
constexpr OwnerID TAKER = 2;

// Each repetition rests this many orders and sweeps them all, so every point does the same work
constexpr std::int64_t RESTING_ORDERS = 128'000;
constexpr std::int64_t MAX_SWEEP = 64;

constexpr Price FIRST_PRICE = 1'000;
constexpr Price MAX_PRICE = 1'000'000; // the ladder needs more prices than the default band has
constexpr Quantity ORDER_QTY = 100;

// One long ask ladder is built in the setup and never refilled: each iteration
// sweeps the next `levels` levels, and the pinned iteration count keeps it from running out
static void BM_MatchOrder_Sweep(benchmark::State& state) {
    const std::int64_t levels = state.range(0);
    const std::int64_t ordersPerLevel = state.range(1);
    const std::int64_t ladderLevels = RESTING_ORDERS / ordersPerLevel;

    OrderBook book(BookConfig{.maxPrice = MAX_PRICE});

    for (std::int64_t level {0}; level < ladderLevels; ++level) {
        Price price = FIRST_PRICE + static_cast<Price>(level);
        for (std::int64_t i {0}; i < ordersPerLevel; ++i) {
            book.addOrder(MAKER, OrderType::GTC, Side::sell, price, ORDER_QTY);
        }
    }

    // Exactly what the swept levels hold, so every order involved fills completely
    const Quantity sweepQty = static_cast<Quantity>(levels * ordersPerLevel) * ORDER_QTY;
    const Price sweepLevels = static_cast<Price>(levels);

    Price bestAsk = FIRST_PRICE;
    for (auto _ : state) {
        Price limit = bestAsk + sweepLevels - 1;
        auto result = book.addOrder(TAKER, OrderType::GTC, Side::buy, limit, sweepQty);
        benchmark::DoNotOptimize(result);

        bestAsk += sweepLevels;
    }

    // Outside the timed loop: if anything is left, some sweep did not consume what it should
    if (!book.empty())
        state.SkipWithError("the ladder was not consumed exactly");
}

// Iterations() applies to a whole benchmark family, and each point needs its
// own count, so the points are registered one by one instead of with BENCHMARK.
static void registerSweep(std::int64_t levels, std::int64_t ordersPerLevel) {
    benchmark::RegisterBenchmark("BM_MatchOrder_Sweep", BM_MatchOrder_Sweep)
        ->Args({levels, ordersPerLevel})
        ->ArgNames({"levels", "orders"})
        ->Unit(benchmark::kNanosecond)
        ->Repetitions(10)
        ->Iterations(RESTING_ORDERS / (levels * ordersPerLevel)) // consumes the whole ladder, no more
        ->ReportAggregatesOnly(true);
}

static bool registerSweeps() {
    // Cost per level: one order at each level
    for (std::int64_t levels {1}; levels <= MAX_SWEEP; levels *= 2) {
        registerSweep(levels, 1);
    }

    // Cost per order: one level; the (1, 1) point is already registered above
    for (std::int64_t ordersPerLevel {2}; ordersPerLevel <= MAX_SWEEP; ordersPerLevel *= 2) {
        registerSweep(1, ordersPerLevel);
    }
    return true;
}

static const bool SWEEPS_REGISTERED = registerSweeps();
