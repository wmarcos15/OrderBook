#pragma once
#include <cstdint>

namespace orderbook {

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderID = std::uint64_t;

using OwnerID = std::uint32_t;

enum class Side {
    buy,
    sell
};

enum class OrderType {
    GTC,
    FOK,
    IOC
};

enum class SelfTradePolicy {
    cancelOldest,
    cancelNewest
};

enum class OrderStatus {
    ok,
    selfTradeCancelledNewest,
    selfTradeCancelledOldest
};

} // namespace orderbook
