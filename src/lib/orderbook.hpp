#pragma once
#include "types.hpp"
#include "trade.hpp"
#include "order.hpp" // so that I can later just import oderbook.hpp into main (less overhead)

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>

struct BookConfig {
    Price tickSize {1};
    Price minPrice {1};
    Price maxPrice {100'000};
};

class OrderBook {
    private:
        struct LevelData {
            Quantity qty_ {0};
            Quantity count_ {0};
        };

        // level_ and levelData_ point into bids_/asks_ and bidsData_/asksData_.
        // Both std::map and std::unordered_map keep element addresses stable
        // until that element is erased, and a level is only erased once its
        // last order leaves, so the pointers stay valid for as long as the
        // entry exists. They let cancelOrder skip the tree descent and the
        // hash lookup it would otherwise need to find the level by price.
        struct OrderEntry {
            OrderPointer order_ {nullptr};
            OrderPointers::iterator location_;
            OrderPointers* level_ {nullptr};
            LevelData* levelData_ {nullptr};
        };

        BookConfig config_;

        OrderID nextID_ {1};

        std::map<Price, OrderPointers, std::greater<Price>> bids_;
        std::map<Price, OrderPointers, std::less<Price>> asks_;
        std::unordered_map<OrderID, OrderEntry> orders_;

        std::unordered_map<Price, LevelData> bidsData_;
        std::unordered_map<Price, LevelData> asksData_;

        void validatePrice(Price price) const;

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

                bool incomingIsBuy = incoming->getSide() == Side::buy;
                OrderID bidID = incomingIsBuy ? incoming->getID() : resting->getID();
                OrderID askID = incomingIsBuy ? resting->getID() : incoming->getID();
                OwnerID bidOwner = incomingIsBuy ? incoming->getOwnerID() : resting->getOwnerID();
                OwnerID askOwner = incomingIsBuy ? resting->getOwnerID() : incoming->getOwnerID();
                trades.push_back({ bidID, askID, bestPrice, qty, bidOwner, askOwner });

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
        explicit OrderBook(BookConfig config = {});

        std::size_t size() const; // total order count
        const Order* getBestBid() const;
        const Order* getBestAsk() const;
        bool empty() const;
        const Order* getOrder(OrderID id) const;
        Quantity getLevelQuantity(Side side, Price price) const;
        std::size_t getLevelCount(Side side, Price price) const;
        const BookConfig& getBookConfig() const;

        OrderResult addOrder(OwnerID owner, OrderType type, Side side, Price price, Quantity qty);
        OrderResult addMarketOrder(OwnerID owner, Side side, Quantity qty);
        void cancelOrder(OrderID orderID);
        std::optional<OrderResult> modifyOrder(OrderID orderID, Price newPrice, Quantity newQty);

};
