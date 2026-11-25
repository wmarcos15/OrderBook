#pragma once
#include "types.hpp"

#include <algorithm>

namespace orderbook {

class Order {
    private:
        OrderID id_;
        OrderType type_;
        Side side_;
        Price price_;
        Quantity startQuantity_;
        Quantity remainingQuantity_;
        OwnerID ownerID_;

    public:
        Order(OrderID id, OwnerID owner, OrderType type, Side side, Price price, Quantity quantity);

        OrderID getID() const {return id_;}
        OrderType getType() const {return type_;}
        Side getSide() const {return side_;}
        Price getPrice() const {return price_;}
        Quantity getStartQuantity() const {return startQuantity_;}
        Quantity getRemainingQuantity() const {return remainingQuantity_;}
        OwnerID getOwnerID() const {return ownerID_;}
        bool isFilled() const {return remainingQuantity_ == 0;}
        void fill(Quantity qty) {remainingQuantity_ -= std::min(remainingQuantity_, qty);}
};

} // namespace orderbook
