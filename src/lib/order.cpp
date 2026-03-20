#include "order.hpp"

Order::Order(OrderID id, OrderType type, Side side, Price price, Quantity quantity)
    : id_ {id}, type_ {type}, side_ {side}, price_ {price}, startQuantity_ {quantity}, remainingQuantity_ {quantity}
{}

OrderID Order::getID() const {return id_;}
OrderType Order::getType() const {return type_;}
Side Order::getSide() const {return side_;}
Price Order::getPrice() const {return price_;}
Quantity Order::getStartQuantity() const {return startQuantity_;}
Quantity Order::getRemainingQuantity() const {return remainingQuantity_;}

bool Order::isFilled() const {
    return remainingQuantity_ == 0;
}

void Order::fill(Quantity qty) {
    remainingQuantity_ -= std::min(remainingQuantity_, qty);
}
