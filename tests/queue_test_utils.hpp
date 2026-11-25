#pragma once
#include "order_queue.hpp"
#include "types.hpp"

#include <vector>

namespace orderbook::test {

inline detail::OrderNode makeNode(OrderID id) {
    return detail::OrderNode(Order(id, 1, OrderType::GTC, Side::buy, 100, 10));
}

// Head to tail
inline std::vector<OrderID> forwardIDs(const detail::OrderQueue& queue) {
    std::vector<OrderID> ids;
    for (const detail::OrderNode* node = queue.head; node != nullptr; node = node->next)
        ids.push_back(node->order.getID());
    return ids;
}

// Tail to head
inline std::vector<OrderID> backwardIDs(const detail::OrderQueue& queue) {
    std::vector<OrderID> ids;
    for (const detail::OrderNode* node = queue.tail; node != nullptr; node = node->prev)
        ids.push_back(node->order.getID());
    return ids;
}

} // namespace orderbook::test
