#pragma once
#include "types.hpp"

class Order {
    private:
        OrderID id_;
        OrderType type_;
        Side side_;
        Price price_;
        Quantity startQuantity_;
        Quantity remainingQuantity_;

    public:
        Order(OrderID id, OrderType type, Side side, Price price, Quantity quantity);

        OrderID getID() const;
        OrderType getType() const;
        Side getSide() const;
        Price getPrice() const;
        Quantity getStartQuantity() const;
        Quantity getRemainingQuantity() const;
        bool isFilled() const;
        void fill(Quantity qty);
};
