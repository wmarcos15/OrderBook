#pragma once
#include "types.hpp"
#include <cstdint>
#include <vector>

struct OrderSpec {
    bool                 isMarket;
    orderbook::OrderType type;
    orderbook::Price     price;
    orderbook::Quantity  qty;
    orderbook::Side      side;
    orderbook::OwnerID   owner;
};

using OrderSpecs = std::vector<OrderSpec>;

OrderSpecs generateWorkload(uint64_t count, uint32_t seed);
