#pragma once
#include "node_pool.hpp"
#include "order_queue.hpp"
#include "types.hpp"
#include "window.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace orderbook::detail {

// The window plus the levels outside it. Asks outside are above the window and
// bids outside are below it, so a side walks the window first and then its map
class PriceLevels {
public:
    struct Cursor {
        Price price;
        const Level* level;
        // Window::NONE outside the window
        std::size_t position;
    };

private:
    // Behind a pointer so that moving never moves a level: a map assigned
    // from another one with a different pool would copy its nodes
    struct Outside {
        using Entry = std::pair<const Price, Level>;
        template<typename Compare>
        using Map = std::map<Price, Level, Compare, NodeAllocator<Entry>>;

        // Declared first: the maps use it, and they share it because their nodes have the same size
        NodePool nodes;
        Map<std::greater<Price>> bids {NodeAllocator<Entry>(&nodes)};
        Map<std::less<Price>> asks {NodeAllocator<Entry>(&nodes)};
    };

    Window window_;
    std::unique_ptr<Outside> outside_;
    // The ends of the band, which the window never leaves
    std::int64_t minTick_;
    std::int64_t maxTick_;

    static std::size_t windowSizeFor(Price tickSize, Price minPrice, Price maxPrice, std::size_t windowSize) {
        assert(tickSize > 0 && minPrice > 0 && minPrice <= maxPrice);
        // In 64 bits: the band can hold more prices than a Price
        std::uint64_t prices = static_cast<std::uint64_t>((static_cast<std::int64_t>(maxPrice) - minPrice) / tickSize) + 1;
        return static_cast<std::size_t>(std::min<std::uint64_t>(windowSize, std::bit_ceil(prices)));
    }

    template<Side side>
    const auto& mapOf() const {
        if constexpr (side == Side::buy) return outside_->bids;
        else return outside_->asks;
    }

    static Cursor cursorAt(const auto& map, const auto& it) {
        if (it == map.end()) return {0, nullptr, Window::NONE};
        return {it->first, &it->second, Window::NONE};
    }

    Cursor bestOf(Side side) const {
        return (side == Side::buy) ? best<Side::buy>() : best<Side::sell>();
    }

    bool leaves(std::size_t position, std::int64_t newBase) const {
        std::int64_t tick = window_.baseTick() + static_cast<std::int64_t>(position);
        return tick < newBase || tick >= newBase + static_cast<std::int64_t>(window_.size());
    }

    // The levels with orders that the new base leaves outside, buys first
    template<class Action>
    void forEachLeaving(std::int64_t newBase, Action action) const {
        for (std::size_t position = window_.best<Side::buy>(); position != Window::NONE; position = window_.after<Side::buy>(position))
            if (leaves(position, newBase)) action(Side::buy, position);
        for (std::size_t position = window_.best<Side::sell>(); position != Window::NONE; position = window_.after<Side::sell>(position))
            if (leaves(position, newBase)) action(Side::sell, position);
    }

    // The entries at the front are the ones closest to the window
    template<class Map>
    void enter(Side side, Map& map) {
        for (auto it = map.begin(); it != map.end(); it = map.erase(it)) {
            std::size_t position = window_.positionOf(it->first);
            if (position == Window::NONE) return;
            window_.putLevel(side, position, it->second);
        }
    }

    template<class Map>
    static void addOutside(Map& map, OrderNode* node) {
        // Can throw, and nothing has changed until it returns
        Level& level = map.try_emplace(node->order.getPrice()).first->second;

        node->level = &level.queue;
        node->levelData = &level.data;
        level.queue.pushBack(node);
        ++level.data.count_;
        level.data.qty_ += node->order.getRemainingQuantity();
    }

    template<class Map>
    static void removeOutside(Map& map, OrderNode* node) {
        node->level->unlink(node);
        --node->levelData->count_;
        node->levelData->qty_ -= node->order.getRemainingQuantity();

        if (node->level->empty()) map.erase(node->order.getPrice());
    }

    template<class Map>
    static const Level* find(const Map& map, Price price) {
        auto it = map.find(price);
        if (it == map.end()) return nullptr;
        return &it->second;
    }

public:
    PriceLevels(Price tickSize, Price minPrice, Price maxPrice, std::size_t windowSize)
        : window_(tickSize, minPrice, windowSizeFor(tickSize, minPrice, maxPrice, windowSize)),
        outside_(std::make_unique<Outside>()),
        minTick_ {minPrice / tickSize}, maxTick_ {maxPrice / tickSize} {}
    PriceLevels(const PriceLevels&) = delete;
    PriceLevels& operator=(const PriceLevels&) = delete;
    PriceLevels(PriceLevels&&) = default;
    PriceLevels& operator=(PriceLevels&&) = default;

    // Only while the window is empty, and the caller keeps it inside the band
    void placeWindowAt(Price basePrice) {
        window_.placeAt(basePrice);
    }

    // Called just before an order rests at price. Only a new best near the edge or outside moves the window,
    // so that it follows the market without moving for every order
    // Returns where the price is in the window afterwards, Window::NONE if outside, so that adding does not divide again
    [[gnu::always_inline]] std::size_t prepareFor(Side side, Price price) {
        std::size_t position = window_.positionOf(price);
        Cursor ownBest = bestOf(side);
        if (ownBest.level != nullptr && (side == Side::buy ? price < ownBest.price : price > ownBest.price)) return position;

        std::size_t margin = window_.size() / 8;
        if (position != Window::NONE && position >= margin && position < window_.size() - margin) return position;

        return followMarket(side, price, position);
    }

    // The rare part of prepareFor, kept out of line so that the rest can be inlined
    [[gnu::noinline]] std::size_t followMarket(Side side, Price price, std::size_t position) {
        std::int64_t tickSize = window_.tickSize();
        std::int64_t centre = price / tickSize;
        Cursor otherBest = bestOf(side == Side::buy ? Side::sell : Side::buy);
        if (otherBest.level != nullptr) centre = (centre + otherBest.price / tickSize) / 2;

        std::int64_t size = static_cast<std::int64_t>(window_.size());
        std::int64_t newBase = minTick_;
        if (maxTick_ - minTick_ + 1 >= size) newBase = std::clamp(centre - size / 2, minTick_, maxTick_ - size + 1);
        if (newBase == window_.baseTick()) return position;

        moveTo(newBase);
        return window_.positionOf(price);
    }

    // The caller keeps the outside rule: no bid above the new window and no ask below it
    void moveTo(std::int64_t newBase) {
        // Room for every level that leaves, so that nothing can fail after this
        std::size_t reserved {0};
        try {
            forEachLeaving(newBase, [&](Side side, std::size_t position) {
                Price price = window_.priceOf(position);
                if (side == Side::buy) outside_->bids.try_emplace(price);
                else outside_->asks.try_emplace(price);
                ++reserved;
            });
        } catch (...) {
            forEachLeaving(newBase, [&](Side side, std::size_t position) {
                if (reserved == 0) return;
                --reserved;
                if (side == Side::buy) outside_->bids.erase(window_.priceOf(position));
                else outside_->asks.erase(window_.priceOf(position));
            });
            throw;
        }

        forEachLeaving(newBase, [&](Side side, std::size_t position) {
            Price price = window_.priceOf(position);
            if (side == Side::buy) window_.takeLevel(position, outside_->bids.find(price)->second);
            else window_.takeLevel(position, outside_->asks.find(price)->second);
        });
        window_.shiftTo(newBase);
        enter(Side::buy, outside_->bids);
        enter(Side::sell, outside_->asks);

        assert(outsideRuleHolds());
    }

    std::size_t windowSize() const {
        return window_.size();
    }

    // Can pass the largest Price, like the window's
    std::int64_t windowBaseTick() const {
        return window_.baseTick();
    }

    template<Side side>
    Cursor best() const {
        std::size_t position = window_.best<side>();
        if (position != Window::NONE) return {window_.priceOf(position), &window_.level(position), position};

        // A moved-from book has no maps
        if (outside_ == nullptr) return {0, nullptr, Window::NONE};
        return cursorAt(mapOf<side>(), mapOf<side>().begin());
    }

    template<Side side>
    Cursor after(const Cursor& cursor) const {
        if (cursor.position != Window::NONE) {
            std::size_t position = window_.after<side>(cursor.position);
            if (position != Window::NONE) return {window_.priceOf(position), &window_.level(position), position};
            return cursorAt(mapOf<side>(), mapOf<side>().begin());
        }
        return cursorAt(mapOf<side>(), mapOf<side>().upper_bound(cursor.price));
    }

    // False for a moved-from one
    bool hasSlots() const {
        return window_.size() > 0;
    }

    void add(OrderNode* node) {
        add(node, window_.positionOf(node->order.getPrice()));
    }

    // The position is the one prepareFor returned
    [[gnu::always_inline]] void add(OrderNode* node, std::size_t position) {
        assert(node != nullptr);
        assert(position == window_.positionOf(node->order.getPrice()));
        [[maybe_unused]] Price price = node->order.getPrice();
        Side side = node->order.getSide();

        if (position != Window::NONE) {
            window_.add(position, node);
        } else {
            assert(levelOf(side == Side::buy ? Side::sell : Side::buy, price) == nullptr);
            if (side == Side::buy) addOutside(outside_->bids, node);
            else addOutside(outside_->asks, node);
        }
        assert(outsideRuleHolds());
    }

    static void fill(OrderNode* node, Quantity quantity) {
        Window::fill(node, quantity);
    }

    [[gnu::always_inline]] void remove(OrderNode* node) {
        if (window_.holds(node)) {
            window_.remove(node);
        } else if (node->order.getSide() == Side::buy) {
            removeOutside(outside_->bids, node);
        } else {
            removeOutside(outside_->asks, node);
        }
    }

    const Level* levelOf(Side side, Price price) const {
        std::size_t position = window_.positionOf(price);
        if (position != Window::NONE) {
            bool hasOrders = (side == Side::buy) ? window_.contains<Side::buy>(position) : window_.contains<Side::sell>(position);
            return hasOrders ? &window_.level(position) : nullptr;
        }

        if (outside_ == nullptr) return nullptr;
        return (side == Side::buy) ? find(outside_->bids, price) : find(outside_->asks, price);
    }

    bool outsideRuleHolds() const {
        if (outside_ == nullptr) return true;
        // In ticks: the window can end past the largest Price
        std::int64_t tickSize = window_.tickSize();
        std::int64_t end = window_.baseTick() + static_cast<std::int64_t>(window_.size());
        if (!outside_->asks.empty() && outside_->asks.begin()->first / tickSize < end) return false;
        if (!outside_->bids.empty() && outside_->bids.begin()->first / tickSize >= window_.baseTick()) return false;
        return true;
    }
};

} // namespace orderbook::detail
