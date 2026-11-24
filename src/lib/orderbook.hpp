#pragma once
#include "types.hpp"
#include "trade.hpp"
#include "order.hpp" // so that I can later just import oderbook.hpp into main (less overhead)
#include "pool.hpp"
#include "order_queue.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace orderbook {

struct BookConfig {
    Price tickSize {1};
    Price minPrice {1};
    Price maxPrice {100'000};
    SelfTradePolicy defaultSelfTradePolicy {SelfTradePolicy::cancelNewest};
};

// One price level as getDepth reports it
struct LevelView {
    Price price;
    Quantity quantity;
    std::size_t count;
};

class OrderBook {
private:
    using LevelData = detail::LevelData;

    BookConfig config_;

    OrderID nextID_ {1};

    // Declared before the containers that point into it, so it is destroyed last
    detail::Pool<detail::OrderNode> pool_;

    std::map<Price, detail::OrderQueue, std::greater<Price>> bids_;
    std::map<Price, detail::OrderQueue, std::less<Price>> asks_;
    std::unordered_map<OrderID, detail::OrderNode*> orders_;

    std::unordered_map<Price, LevelData> bidsData_;
    std::unordered_map<Price, LevelData> asksData_;

    void validatePrice(Price price) const;

    bool match(Order& incoming, OrderResult& result);
    bool canFullyFill(const Order& order, OrderResult& result) const;

    using OrderIterator = std::unordered_map<OrderID, detail::OrderNode*>::iterator;
    OrderIterator findOrder(OrderID id);

    std::unordered_map<OwnerID, SelfTradePolicy> policies_;

    template<typename MapType>
    bool matchTop(Order& incoming, MapType& opposite, 
                  std::unordered_map<Price, LevelData>& oppositeData, 
                  OrderResult& result) {

        auto& [bestPrice, oppositeList] = *opposite.begin();
        detail::OrderNode* resting = oppositeList.head;

        if (resting->order.getOwnerID() == incoming.getOwnerID()) {
            if (getSelfTradePolicy(incoming.getOwnerID()) == SelfTradePolicy::cancelNewest) {
                result.status = OrderStatus::selfTradeCancelledNewest;
                return false;
            }

            OrderID restingID = resting->order.getID();
            cancelOrder(restingID);
            result.status = OrderStatus::selfTradeCancelledOldest;
            result.cancelledOrders.push_back(restingID);
            return true;
        }

        Quantity qty = std::min(incoming.getRemainingQuantity(), resting->order.getRemainingQuantity());
        incoming.fill(qty);
        resting->order.fill(qty);

        oppositeData[bestPrice].qty_ -= qty;

        bool incomingIsBuy = incoming.getSide() == Side::buy;
        OrderID bidID = incomingIsBuy ? incoming.getID() : resting->order.getID();
        OrderID askID = incomingIsBuy ? resting->order.getID() : incoming.getID();
        OwnerID bidOwner = incomingIsBuy ? incoming.getOwnerID() : resting->order.getOwnerID();
        OwnerID askOwner = incomingIsBuy ? resting->order.getOwnerID() : incoming.getOwnerID();
        result.trades.push_back({ bidID, askID, bestPrice, qty, bidOwner, askOwner });

        if (resting->order.isFilled()) {
            OrderID restingID = resting->order.getID();   // read before the slot is released
            oppositeData[bestPrice].count_--;
            oppositeList.popFront();
            orders_.erase(restingID);
            pool_.release(resting);   // last: nothing reads `resting` after this
        }
        if (oppositeList.empty()) {
            oppositeData.erase(bestPrice);
            opposite.erase(opposite.begin());
        }
        return true;
    }

    // Walks single orders, not level totals: what counts depends on who owns each one
    template<typename MapType>
    bool canFullyFillAgainst(const Order& order, const MapType& opposite, OrderResult& result) const {
        Quantity qty = order.getRemainingQuantity();
        OwnerID owner = order.getOwnerID();
        bool isBuy = order.getSide() == Side::buy;
        bool stopAtOwn = getSelfTradePolicy(owner) == SelfTradePolicy::cancelNewest;

        for (const auto& [price, list] : opposite) {
            if (isBuy ? price > order.getPrice() : price < order.getPrice()) break;

            for (const detail::OrderNode* node = list.head; node != nullptr; node = node->next) {
                const Order& resting = node->order;
                if (resting.getOwnerID() == owner) {
                    if (!stopAtOwn) continue;
                    result.status = OrderStatus::selfTradeCancelledNewest;
                    return false;
                }
                if (resting.getRemainingQuantity() >= qty) return true;
                qty -= resting.getRemainingQuantity();
            }
        }
        return false;
    }

    // Reads the level totals, so the cost does not depend on how many orders rest at each level
    template<typename MapType>
    std::vector<LevelView> depthOf(const MapType& side,
                                   const std::unordered_map<Price, LevelData>& sideData,
                                   std::size_t levels) const {

        std::vector<LevelView> depth;
        // min: levels can be far larger than the side, and reserve would try to allocate it all
        depth.reserve(std::min(levels, side.size()));

        for (const auto& [price, list] : side) {
            if (depth.size() == levels) break;

            const LevelData& level = sideData.at(price);
            depth.push_back({ price, level.qty_, level.count_ });
        }
        return depth;
    }

public:
    explicit OrderBook(BookConfig config = {});

    // Not copyable: every node points into this book's own levels and the pool cannot be copied. Moving is fine: the pool hands over its chunks and the containers keep their elements' addresses when moved.
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&&) = default;

    std::size_t size() const; // total order count
    const Order* getBestBid() const;
    const Order* getBestAsk() const;
    bool empty() const;
    const Order* getOrder(OrderID id) const;
    Quantity getLevelQuantity(Side side, Price price) const;
    std::size_t getLevelCount(Side side, Price price) const;
    std::vector<LevelView> getDepth(Side side, std::size_t levels) const; // best level first
    const BookConfig& getBookConfig() const;
    SelfTradePolicy getSelfTradePolicy(OwnerID ownerID) const;

    void setSelfTradePolicy(OwnerID ownerID, SelfTradePolicy policy);

    OrderResult addOrder(OwnerID owner, OrderType type, Side side, Price price, Quantity qty);
    OrderResult addMarketOrder(OwnerID owner, Side side, Quantity qty);
    void cancelOrder(OrderID orderID);
    std::optional<OrderResult> modifyOrder(OrderID orderID, Price newPrice, Quantity newQty);

};

} // namespace orderbook
