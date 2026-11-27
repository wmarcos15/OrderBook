#pragma once
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
#include <memory_resource>

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
    // Behind a pointer so that moving never moves a level: a pmr map assigned
    // across pools would copy its nodes
    struct Outside {
        // Declared first: the maps use it
        std::pmr::unsynchronized_pool_resource memory;
        std::pmr::map<Price, Level, std::greater<Price>> bids {&memory};
        std::pmr::map<Price, Level> asks {&memory};
    };

    Window window_;
    std::unique_ptr<Outside> outside_;

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
        outside_(std::make_unique<Outside>()) {}
    PriceLevels(const PriceLevels&) = delete;
    PriceLevels& operator=(const PriceLevels&) = delete;
    PriceLevels(PriceLevels&&) = default;
    PriceLevels& operator=(PriceLevels&&) = default;

    // Only while the window is empty, and the caller keeps it inside the band
    void placeWindowAt(Price basePrice) {
        window_.placeAt(basePrice);
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

    void add(OrderNode* node) {
        assert(node != nullptr);
        Price price = node->order.getPrice();
        Side side = node->order.getSide();

        std::size_t position = window_.positionOf(price);
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

    void remove(OrderNode* node) {
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
