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
    SelfTradePolicy defaultSelfTradePolicy {SelfTradePolicy::cancelNewest};
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

        bool match(OrderPointer incoming, OrderResult& result);
        bool canFullyFill(OrderPointer order) const;

        using OrderIterator = std::unordered_map<OrderID, OrderEntry>::iterator;
        OrderIterator findOrder(OrderID id);

        std::unordered_map<OwnerID, SelfTradePolicy> policies_;

        template<typename MapType>
            bool matchTop(OrderPointer incoming, MapType& opposite, 
                    std::unordered_map<Price, LevelData>& oppositeData, 
                    OrderResult& result) {

                auto& [bestPrice, oppositeList] = *opposite.begin();
                OrderPointer resting = oppositeList.front();

                if (resting->getOwnerID() == incoming->getOwnerID()) {
                    result.status = OrderStatus::selfTradePrevented;
                    if (getSelfTradePolicy(incoming->getOwnerID()) == SelfTradePolicy::cancelNewest)
                        return false;

                    OrderID restingID = resting->getID();
                    cancelOrder(restingID);
                    result.cancelledOrders.push_back(restingID);
                    return true;
                }

                Quantity qty = std::min(incoming->getRemainingQuantity(), resting->getRemainingQuantity());
                incoming->fill(qty);
                resting->fill(qty);

                oppositeData[bestPrice].qty_ -= qty;

                bool incomingIsBuy = incoming->getSide() == Side::buy;
                OrderID bidID = incomingIsBuy ? incoming->getID() : resting->getID();
                OrderID askID = incomingIsBuy ? resting->getID() : incoming->getID();
                OwnerID bidOwner = incomingIsBuy ? incoming->getOwnerID() : resting->getOwnerID();
                OwnerID askOwner = incomingIsBuy ? resting->getOwnerID() : incoming->getOwnerID();
                result.trades.push_back({ bidID, askID, bestPrice, qty, bidOwner, askOwner });

                if (resting->isFilled()) {
                    oppositeData[bestPrice].count_--;
                    oppositeList.pop_front();
                    orders_.erase(resting->getID());
                }
                if (oppositeList.empty()) {
                    opposite.erase(bestPrice);
                    oppositeData.erase(bestPrice);
                }
                return true;
            }

        // Walks single orders, not level totals: what counts depends on who owns each one
        template<typename MapType>
            bool canFullyFillAgainst(OrderPointer order, const MapType& opposite) const {
                Quantity qty = order->getRemainingQuantity();
                OwnerID owner = order->getOwnerID();
                bool isBuy = order->getSide() == Side::buy;
                bool stopAtOwn = getSelfTradePolicy(owner) == SelfTradePolicy::cancelNewest;

                for (const auto& [price, list] : opposite) {
                    if (isBuy ? price > order->getPrice() : price < order->getPrice()) break;

                    for (const auto& resting : list) {
                        if (resting->getOwnerID() == owner) {
                            if (stopAtOwn) return false;
                            continue;
                        }
                        if (resting->getRemainingQuantity() >= qty) return true;
                        qty -= resting->getRemainingQuantity();
                    }
                }
                return false;
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
        SelfTradePolicy getSelfTradePolicy(OwnerID ownerID) const;

        void setSelfTradePolicy(OwnerID ownerID, SelfTradePolicy policy);

        OrderResult addOrder(OwnerID owner, OrderType type, Side side, Price price, Quantity qty);
        OrderResult addMarketOrder(OwnerID owner, Side side, Quantity qty);
        void cancelOrder(OrderID orderID);
        std::optional<OrderResult> modifyOrder(OrderID orderID, Price newPrice, Quantity newQty);

};
