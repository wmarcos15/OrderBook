#pragma once
#include <cstdint>

constexpr const char* SCENARIO_ADDORDER_THROUGHPUT = "addorder_throughput";
constexpr const char* SCENARIO_ADDORDER_LATENCY = "addorder_latency";

void benchmarkAddOrderThroughput(uint32_t seed);
void benchmarkAddOrderLatency(uint32_t seed);
