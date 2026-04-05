#pragma once
#include <cstdint>
#include <vector>
#include "types.hpp"

struct OrderSpec {
    bool isMarket;
    OrderType type;
    Price price;
    Quantity qty;
    Side side;
};

typedef std::vector<OrderSpec> OrderSpecs;

OrderSpecs generateWorkload(uint32_t count, uint32_t seed);
