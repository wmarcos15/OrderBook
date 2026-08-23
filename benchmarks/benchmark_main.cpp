#include "scenarios/addorder.hpp"
#include "scenarios/cancelorder.hpp"
#include <iostream>
#include <string>
#include <functional>
#include <unordered_map>

constexpr uint32_t SEED = 42;

void printHelp(const std::unordered_map<std::string, std::function<void()>>& scenarios) {
    std::cout << "Usage: orderbook_benchmark [scenario]\n\n";
    std::cout << "Scenarios:\n";
    std::cout << "\tall\t\trun all scenarios (default)\n";
    for (const auto& [name, fn] : scenarios)
        std::cout << "\t" << name << "\n";
    std::cout << "\nExample:\n";
    std::cout << "\t./orderbook_benchmark " << SCENARIO_ADDORDER_THROUGHPUT << "\n";
}

int main(int argc, char* argv[]) {
    std::string scenario = (argc > 1) ? argv[1] : "all";

    const std::unordered_map<std::string, std::function<void()>> scenarios = {
        {SCENARIO_ADDORDER_THROUGHPUT, [&]() {benchmarkAddOrderThroughput(SEED);}},
        {SCENARIO_ADDORDER_LATENCY, [&]() {benchmarkAddOrderLatency(SEED);}},
        {SCENARIO_CANCELORDER_HIT_THROUGHPUT, [&]() {benchmarkCancelOrderHitThroughput(SEED);}},
        {SCENARIO_CANCELORDER_MISS_THROUGHPUT, [&]() {benchmarkCancelOrderMissThroughput(SEED);}},
        {SCENARIO_CANCELORDER_HIT_LATENCY, [&]() {benchmarkCancelOrderHitLatency(SEED);}},
        {SCENARIO_CANCELORDER_MISS_LATENCY, [&]() {benchmarkCancelOrderMissLatency(SEED);}},
    };

    if (scenario == "help" || scenario == "--help" || scenario == "-h") {
        printHelp(scenarios);
        return 0;
    }

    if (scenario == "all") {
        for (const auto& [name, fn] : scenarios) fn();
        return 0;
    }

    auto it = scenarios.find(scenario);
    if (it == scenarios.end()) {
        std::cerr << "Unknown scenario: '" << scenario << "'\n";
        std::cerr << "Run with 'help' to see available scenarios.\n";
        return 1;
    }

    it->second();
    return 0;
}

