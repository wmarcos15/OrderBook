#pragma once
#include "types.hpp"
#include "trade.hpp"
#include "order.hpp" // so that I can later just import oderbook.hpp into main (less overhead)

#include <cstddef>
#include <map>
#include <optional>

class OrderBook {
    private:
        struct OrderEntry {
            OrderPointer order_ {nullptr};
            OrderPointers::iterator location_;
        };

        struct LevelData {
            Quantity qty_ {0};
            Quantity count_ {0};
        };

        OrderID nextID_ {1};

        std::map<Price, OrderPointers, std::greater<Price>> bids_;
        std::map<Price, OrderPointers, std::less<Price>> asks_;
        std::unordered_map<OrderID, OrderEntry> orders_;

        std::unordered_map<Price, LevelData> bidsData_;
        std::unordered_map<Price, LevelData> asksData_;

        Trades match(OrderPointer incoming);
        bool canFullyFill(OrderPointer order) const;

        using OrderIterator = std::unordered_map<OrderID, OrderEntry>::iterator;
        OrderIterator findOrder(OrderID id);

        template<typename MapType>
            void matchTop(OrderPointer incoming, MapType& opposite, 
                    std::unordered_map<Price, LevelData>& oppositeData, 
                    Trades& trades) {

                auto& [bestPrice, oppositeList] = *opposite.begin();
                OrderPointer resting = oppositeList.front();

                Quantity qty = std::min(incoming->getRemainingQuantity(), resting->getRemainingQuantity());
                incoming->fill(qty);
                resting->fill(qty);

                oppositeData[bestPrice].qty_ -= qty;

                OrderID bidID = incoming->getSide() == Side::buy ? incoming->getID() : resting->getID();
                OrderID askID = incoming->getSide() == Side::buy ? resting->getID() : incoming->getID();
                trades.push_back({ bidID, askID, bestPrice, qty });

                if (resting->isFilled()) {
                    oppositeData[bestPrice].count_--;
                    oppositeList.pop_front();
                    orders_.erase(resting->getID());
                }
                if (oppositeList.empty()) {
                    opposite.erase(bestPrice);
                    oppositeData.erase(bestPrice);
                }
            }

    public:
        std::size_t size() const; // total order count
        const Order* getBestBid() const;
        const Order* getBestAsk() const;
        bool empty() const;
        const Order* getOrder(OrderID id) const;
        Quantity getLevelQuantity(Side side, Price price) const;
        std::size_t getLevelCount(Side side, Price price) const;

        OrderResult addOrder(OrderType type, Side side, Price price, Quantity qty);
        void cancelOrder(OrderID orderID);
        std::optional<OrderResult> modifyOrder(OrderID orderID, Price newPrice, Quantity newQty);
        void printState();

};
