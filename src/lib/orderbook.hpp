#pragma once
#include "types.hpp"
#include "trade.hpp"
#include "order.hpp" // so that I can later just import oderbook.hpp into main (less overhead)
#include "pool.hpp"
#include "order_queue.hpp"
#include "order_table.hpp"
#include "price_levels.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace orderbook {

struct BookConfig {
    Price tickSize {1};
    Price minPrice {1};
    Price maxPrice {std::numeric_limits<Price>::max()};
    SelfTradePolicy defaultSelfTradePolicy {SelfTradePolicy::cancelNewest};
    std::size_t windowSize {65'536};
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

    // Declared before the levels and the table that point into it, so it is destroyed last
    detail::Pool<detail::OrderNode> pool_;

    detail::PriceLevels levels_;
    detail::OrderTable<> orders_;

    static constexpr std::size_t DEPTH_RESERVE = 32;

    // Runs before the levels are built
    static BookConfig validated(BookConfig config);

    [[noreturn]] void rejectPrice(Price price) const;

    bool isLegal(Price price) const;
    void requireLegal(Price price) const;

    template<Side opposite>
    static bool isBeyond(Price price, Price limit) {
        if constexpr (opposite == Side::sell) return price > limit;
        else return price < limit;
    }

    OrderResult addAt(OwnerID owner, OrderType type, Side side, Price price, Quantity qty);

    bool match(Order& incoming, Price limit, OrderResult& result);
    bool matchTop(Order& incoming, const detail::PriceLevels::Cursor& best, OrderResult& result);
    bool canFullyFill(const Order& order, Price limit, OrderResult& result) const;

    // Gives the order a slot and an entry in the table. If it throws, nothing has changed.
    detail::OrderNode* store(const Order& order);

    // Undoes store
    void unstore(detail::OrderNode* node);

    // Puts the order in the book. If it throws, nothing has changed.
    void rest(const Order& order);

    // Takes a resting order out of the book. The node is dead after this.
    void removeNode(detail::OrderNode* node);

    std::unordered_map<OwnerID, SelfTradePolicy> policies_;

    template<Side opposite>
    [[gnu::always_inline]] bool matchAgainst(Order& incoming, Price limit, OrderResult& result) {
        detail::PriceLevels::Cursor best = levels_.best<opposite>();
        while (incoming.getRemainingQuantity() > 0) {
            if (best.level == nullptr || isBeyond<opposite>(best.price, limit)) break;

            bool isLastOrder = best.level->queue.head->next == nullptr;
            if (!matchTop(incoming, best, result)) return false;

            // Only an emptied level has a new best, and it can be gone from its map
            if (isLastOrder) best = levels_.best<opposite>();
        }

        return true;
    }

    // Walks single orders, not level totals: what counts depends on who owns each one
    template<Side opposite>
    bool canFullyFillAgainst(const Order& order, Price limit, OrderResult& result) const {
        Quantity qty = order.getRemainingQuantity();
        OwnerID owner = order.getOwnerID();
        bool stopAtOwn = getSelfTradePolicy(owner) == SelfTradePolicy::cancelNewest;

        for (detail::PriceLevels::Cursor cursor = levels_.best<opposite>();
             cursor.level != nullptr && !isBeyond<opposite>(cursor.price, limit);
             cursor = levels_.after<opposite>(cursor)) {

            for (const detail::OrderNode* node = cursor.level->queue.head; node != nullptr; node = node->next) {
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

        for (detail::PriceLevels::Cursor cursor = levels_.best<side>();
             cursor.level != nullptr && depth.size() < levels;
             cursor = levels_.after<side>(cursor)) {

            const detail::LevelData& data = cursor.level->data;
            depth.push_back({ cursor.price, data.qty_, data.count_ });
        }
        return depth;
    }

public:
    explicit OrderBook(BookConfig config = {});

    // Not copyable: the nodes point into this book. A moved-from book throws std::logic_error when given an order
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = default;
    OrderBook& operator=(OrderBook&& other) noexcept;

    std::size_t size() const;
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
