#pragma once
#include "types.hpp"
#include <cstdint>
#include <vector>

struct OrderSpec {
    bool      isMarket;
    OrderType type;
    Price     price;
    Quantity  qty;
    Side      side;
};

using OrderSpecs = std::vector<OrderSpec>;

OrderSpecs generateWorkload(uint64_t count, uint32_t seed);
