// Runs one operation in a loop for a few seconds, so that a sampling profiler
// can be attached to it. The loops are the ones in scenarios/, without Google
// Benchmark around them.
//
// The benchmarks themselves are a poor target for a profiler: most of their run
// is the setup that builds the book. Here every measured loop is in its own
// function whose name starts with "timed", and that function is never inlined,
// so breakdown.py can keep the samples under it and drop the rest.
#include "orderbook.hpp"
#include "workload.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace orderbook;
using Clock = std::chrono::steady_clock;

#define NOINLINE __attribute__((noinline))

constexpr std::uint32_t SEED = 42;
constexpr std::uint64_t ADD_WORKLOAD_SIZE = 1'000'000;
constexpr std::uint64_t CANCEL_WORKLOAD_SIZE = 2'000'000;

constexpr OwnerID MAKER = 1;
constexpr OwnerID TAKER = 2;
constexpr std::int64_t RESTING_ORDERS = 128'000;
constexpr std::int64_t MAX_SWEEP = 64;
constexpr Price FIRST_PRICE = 1'000;
constexpr Price MAX_PRICE = 1'000'000;
constexpr Quantity ORDER_QTY = 100;

constexpr double DEFAULT_SECONDS = 5.0;

// Keeps the results alive, as benchmark::DoNotOptimize does in the scenarios
static volatile std::uint64_t tradeCount {0};

// --------------------------
// addOrder: BM_AddOrder
// --------------------------
NOINLINE static void timedAddOrders(OrderBook& book, const OrderSpecs& workload) {
    for (const auto& spec : workload) {
        auto result = spec.isMarket
            ? book.addMarketOrder(spec.owner, spec.side, spec.qty)
            : book.addOrder(spec.owner, spec.type, spec.side, spec.price, spec.qty);

        tradeCount = tradeCount + result.trades.size();
    }
}

// --------------------------
// cancelOrder: BM_CancelOrderHit
// --------------------------
NOINLINE static std::uint64_t restBuys(OrderBook& book, const OrderSpecs& workload) {
    std::uint64_t restingCount {0};
    for (const auto& spec : workload) {
        if (spec.isMarket) continue;

        book.addOrder(spec.owner, OrderType::GTC, Side::buy, spec.price, spec.qty);
        ++restingCount;
    }
    return restingCount;
}

// The benchmark cancels the first half of its pool in ID order; so does this
NOINLINE static void timedCancels(OrderBook& book, std::uint64_t restingCount) {
    for (OrderID id {1}; id <= restingCount / 2; ++id) {
        book.cancelOrder(id);
    }
}

// --------------------------
// Crossing order: BM_MatchOrder_Sweep
// --------------------------
NOINLINE static void buildLadder(OrderBook& book, std::int64_t ordersPerLevel) {
    const std::int64_t ladderLevels = RESTING_ORDERS / ordersPerLevel;

    for (std::int64_t level {0}; level < ladderLevels; ++level) {
        Price price = FIRST_PRICE + static_cast<Price>(level);
        for (std::int64_t i {0}; i < ordersPerLevel; ++i) {
            book.addOrder(MAKER, OrderType::GTC, Side::sell, price, ORDER_QTY);
        }
    }
}

NOINLINE static void timedSweeps(OrderBook& book, std::int64_t levels, std::int64_t ordersPerLevel) {
    const std::int64_t sweeps = RESTING_ORDERS / (levels * ordersPerLevel);
    const Quantity sweepQty = static_cast<Quantity>(levels * ordersPerLevel) * ORDER_QTY;
    const Price sweepLevels = static_cast<Price>(levels);

    Price bestAsk = FIRST_PRICE;
    for (std::int64_t i {0}; i < sweeps; ++i) {
        Price limit = bestAsk + sweepLevels - 1;
        auto result = book.addOrder(TAKER, OrderType::GTC, Side::buy, limit, sweepQty);

        tradeCount = tradeCount + result.trades.size();
        bestAsk += sweepLevels;
    }
}

static void runSweeps(Clock::time_point end, std::int64_t levels, std::int64_t ordersPerLevel) {
    while (Clock::now() < end) {
        OrderBook book(BookConfig{.maxPrice = MAX_PRICE});
        buildLadder(book, ordersPerLevel);
        timedSweeps(book, levels, ordersPerLevel);
    }
}

static int printUsage() {
    std::cerr << "Usage: orderbook_profile <add|cancel|sweep-levels|sweep-orders> [seconds]\n";
    return EXIT_FAILURE;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) return printUsage();

    const std::string operation = argv[1];
    const double seconds = (argc == 3) ? std::atof(argv[2]) : DEFAULT_SECONDS;
    if (seconds <= 0) {
        std::cerr << "ERROR: seconds must be greater than zero\n";
        return EXIT_FAILURE;
    }

    const auto duration = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    const Clock::time_point end = Clock::now() + duration;

    if (operation == "add") {
        const OrderSpecs workload = generateWorkload(ADD_WORKLOAD_SIZE, SEED);
        OrderBook book;
        while (Clock::now() < end) timedAddOrders(book, workload);
    } else if (operation == "cancel") {
        const OrderSpecs workload = generateWorkload(CANCEL_WORKLOAD_SIZE, SEED);
        while (Clock::now() < end) {
            OrderBook book;
            std::uint64_t restingCount = restBuys(book, workload);
            timedCancels(book, restingCount);
        }
    } else if (operation == "sweep-levels") {
        runSweeps(end, MAX_SWEEP, 1); // many levels, one order at each
    } else if (operation == "sweep-orders") {
        runSweeps(end, 1, MAX_SWEEP); // one level, many orders in it
    } else {
        std::cerr << "ERROR: unknown operation '" << operation << "'\n";
        return printUsage();
    }

    std::cout << "Trades: " << tradeCount << '\n';
    return EXIT_SUCCESS;
}
