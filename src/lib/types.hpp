#pragma once
#include <cstdint>
#include <list>
#include <memory>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderID = std::uint64_t;

enum class Side {
    buy,
    sell
};

enum class OrderType {
    GTC,
    FOK,
    IOC
};

class Order; // forward declaration
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

