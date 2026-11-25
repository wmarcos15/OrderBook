#include "order.hpp"

#include <algorithm>

namespace orderbook {

Order::Order(OrderID id, OwnerID owner, OrderType type, Side side, Price price, Quantity quantity)
    : id_ {id}, type_ {type}, side_ {side}, price_ {price}, startQuantity_ {quantity}, remainingQuantity_ {quantity}, ownerID_ {owner}
{}

void Order::fill(Quantity qty) {
    remainingQuantity_ -= std::min(remainingQuantity_, qty);
}

} // namespace orderbook
