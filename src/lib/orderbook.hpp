#pragma once
#include "types.hpp"
#include "trade.hpp"
#include "order.hpp" // so that I can later just import oderbook.hpp into main (less overhead)
#include "pool.hpp"
#include "order_queue.hpp"
#include "order_table.hpp"
#include "ladder.hpp"

#include <algorithm>
#include <cstddef>
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
    BookConfig config_;

    OrderID nextID_ {1};

    // Declared before the ladder and the table that point into it, so it is destroyed last
    detail::Pool<detail::OrderNode> pool_;

    detail::Ladder ladder_;
    detail::OrderTable<> orders_;

    // The reserve of getDepth: covers the usual requests in one allocation, larger ones still work
    static constexpr std::size_t DEPTH_RESERVE = 32;

    // Runs before the ladder is built, which needs a valid config
    static BookConfig validated(const BookConfig& config);

    // Always throws: the price has no position
    [[noreturn]] void rejectPrice(Price price) const;

    // Whether `position` is beyond `limit` for someone trading against the `opposite` side
    template<Side opposite>
    static bool isBeyond(std::size_t position, std::size_t limit) {
        if constexpr (opposite == Side::sell) return position > limit;
        else return position < limit;
    }

    // The position of the level if that side has orders there, NONE otherwise
    std::size_t positionWithOrders(Side side, Price price) const;

    OrderResult addAt(std::size_t position, OwnerID owner, OrderType type, Side side, Price price, Quantity qty);

    bool match(Order& incoming, std::size_t limit, OrderResult& result);
    bool matchTop(Order& incoming, std::size_t best, OrderResult& result);
    bool canFullyFill(const Order& order, std::size_t limit, OrderResult& result) const;

    // Takes a resting order out of the book. The node is dead after this.
    void removeNode(detail::OrderNode* node);

    std::unordered_map<OwnerID, SelfTradePolicy> policies_;

    // Asks for the best level again for every order, since matching changes it
    template<Side opposite>
    bool matchAgainst(Order& incoming, std::size_t limit, OrderResult& result) {
        while (incoming.getRemainingQuantity() > 0) {
            std::size_t best = ladder_.best<opposite>();
            if (best == detail::Ladder::NONE || isBeyond<opposite>(best, limit)) break;
            if (!matchTop(incoming, best, result)) return false;
        }

        return true;
    }

    // Walks single orders, not level totals: what counts depends on who owns each one
    template<Side opposite>
    bool canFullyFillAgainst(const Order& order, std::size_t limit, OrderResult& result) const {
        Quantity qty = order.getRemainingQuantity();
        OwnerID owner = order.getOwnerID();
        bool stopAtOwn = getSelfTradePolicy(owner) == SelfTradePolicy::cancelNewest;

        for (std::size_t position = ladder_.best<opposite>();
             position != detail::Ladder::NONE && !isBeyond<opposite>(position, limit);
             position = ladder_.after<opposite>(position)) {

            for (const detail::OrderNode* node = ladder_.level(position).queue.head; node != nullptr; node = node->next) {
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
    template<Side side>
    std::vector<LevelView> depthOf(std::size_t levels) const {
        std::vector<LevelView> depth;
        // min: levels can be far larger than the side, and reserve would try to allocate it all
        depth.reserve(std::min(levels, DEPTH_RESERVE));

        for (std::size_t position = ladder_.best<side>();
             position != detail::Ladder::NONE && depth.size() < levels;
             position = ladder_.after<side>(position)) {

            const detail::LevelData& data = ladder_.level(position).data;
            depth.push_back({ ladder_.priceOf(position), data.qty_, data.count_ });
        }
        return depth;
    }

public:
    explicit OrderBook(BookConfig config = {});

    // Not copyable: every node points into this book's own ladder and the pool cannot be copied. Moving is fine: the pool hands over its chunks and the ladder's levels keep their addresses when moved. A moved-from book has no levels, so it throws std::logic_error when given an order.
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
