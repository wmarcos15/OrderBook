#pragma once
#include <cstdint>

constexpr const char* SCENARIO_CANCELORDER_HIT_THROUGHPUT = "cancelorder_hit_throughput";
constexpr const char* SCENARIO_CANCELORDER_MISS_THROUGHPUT = "cancelorder_miss_throughput";
constexpr const char* SCENARIO_CANCELORDER_HIT_LATENCY = "cancelorder_hit_latency";
constexpr const char* SCENARIO_CANCELORDER_MISS_LATENCY = "cancelorder_miss_latency";

void benchmarkCancelOrderHitThroughput(uint32_t seed);
void benchmarkCancelOrderMissThroughput(uint32_t seed);
void benchmarkCancelOrderHitLatency(uint32_t seed);
void benchmarkCancelOrderMissLatency(uint32_t seed);

