#pragma once
#include "bitmap.hpp"
#include "order_queue.hpp"
#include "side_index.hpp"
#include "types.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

namespace orderbook::detail {

// The orders resting at one price, and their totals
struct Level {
    OrderQueue queue;
    LevelData data;
};

// One level for every legal price, from minPrice to maxPrice in steps of tickSize.
// A price never holds buys and sells at once, they would have traded, so both sides
// share the levels and each has its own index of which ones are in use.
// The ladder owns the levels but not the nodes, which belong to the book.
class Ladder {
private:
    Price minPrice_;
    Price maxPrice_;
    Price tickSize_;
    // Never grows, so a Level never moves in memory and the nodes can point into it
    std::vector<Level> levels_;
    SideIndex<Side::buy> bids_;
    SideIndex<Side::sell> asks_;

    // Checked here and not in the constructor body, because the vector is sized first
    static std::size_t levelsFor(Price minPrice, Price maxPrice, Price tickSize) {
        assert(tickSize > 0);
        assert(minPrice > 0 && minPrice <= maxPrice);
        assert(minPrice % tickSize == 0 && maxPrice % tickSize == 0);
        return static_cast<std::size_t>((maxPrice - minPrice) / tickSize) + 1;
    }

    template<Side side>
    const auto& index() const {
        if constexpr (side == Side::buy) return bids_;
        else return asks_;
    }

    void addToIndex(Side side, std::size_t position) {
        if (side == Side::buy) bids_.add(position);
        else asks_.add(position);
    }

    void removeFromIndex(Side side, std::size_t position) {
        if (side == Side::buy) bids_.remove(position);
        else asks_.remove(position);
    }

public:
    // Returned when there is no position to find
    static constexpr std::size_t NONE = Bitmap::NONE;

    // The book has already checked that the arguments are valid
    Ladder(Price minPrice, Price maxPrice, Price tickSize)
        : minPrice_ {minPrice}, maxPrice_ {maxPrice}, tickSize_ {tickSize},
        levels_(levelsFor(minPrice, maxPrice, tickSize)),
        bids_(levels_.size()), asks_(levels_.size()) {}
    Ladder(const Ladder&) = delete;
    Ladder& operator=(const Ladder&) = delete;
    Ladder(Ladder&&) = default;
    Ladder& operator=(Ladder&&) = default;

    // NONE if the price is not one of the levels
    std::size_t positionOf(Price price) const {
        // Checked before the subtraction, which would overflow for a very negative price
        if (price < minPrice_ || price > maxPrice_) return NONE;
        if ((price - minPrice_) % tickSize_ != 0) return NONE;

        std::size_t position = static_cast<std::size_t>((price - minPrice_) / tickSize_);
        // A moved-from ladder has no levels
        if (position >= levels_.size()) return NONE;
        return position;
    }

    Price priceOf(std::size_t position) const {
        assert(position < levels_.size());
        return minPrice_ + static_cast<Price>(position) * tickSize_;
    }

    const Level& level(std::size_t position) const {
        assert(position < levels_.size());
        return levels_[position];
    }

    template<Side side>
    bool contains(std::size_t position) const {
        return index<side>().contains(position);
    }

    // NONE if the side has no orders
    template<Side side>
    std::size_t best() const {
        return index<side>().best();
    }

    // The next position with orders strictly worse than `position`, NONE if there is none
    template<Side side>
    std::size_t after(std::size_t position) const {
        return index<side>().after(position);
    }

    void add(std::size_t position, OrderNode* node) {
        assert(node != nullptr);
        assert(position == positionOf(node->order.getPrice()));
        Side side = node->order.getSide();
        assert(side == Side::buy ? !asks_.contains(position) : !bids_.contains(position));

        Level& level = levels_[position];
        bool wasEmpty = level.queue.empty();

        node->level = &level.queue;
        node->levelData = &level.data;
        level.queue.pushBack(node);
        ++level.data.count_;
        level.data.qty_ += node->order.getRemainingQuantity();

        if (wasEmpty) addToIndex(side, position);
    }

    // The node stays in its queue, the book removes it when it has no quantity left
    static void fill(OrderNode* node, Quantity quantity) {
        assert(quantity <= node->order.getRemainingQuantity());
        node->order.fill(quantity);
        node->levelData->qty_ -= quantity;
    }

    // Does not free the node, the book does
    void remove(OrderNode* node) {
        node->level->unlink(node);
        --node->levelData->count_;
        node->levelData->qty_ -= node->order.getRemainingQuantity();

        if (!node->level->empty()) return;
        // Only looked up here, so that the node does not have to store its position
        removeFromIndex(node->order.getSide(), positionOf(node->order.getPrice()));
    }
};

} // namespace orderbook::detail
