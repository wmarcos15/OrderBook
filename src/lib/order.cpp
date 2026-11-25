#include "order.hpp"

namespace orderbook {

Order::Order(OrderID id, OwnerID owner, OrderType type, Side side, Price price, Quantity quantity)
    : id_ {id}, type_ {type}, side_ {side}, price_ {price}, startQuantity_ {quantity}, remainingQuantity_ {quantity}, ownerID_ {owner}
{}

} // namespace orderbook
