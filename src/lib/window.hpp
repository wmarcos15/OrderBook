#pragma once
#include "bitmap.hpp"
#include "order_queue.hpp"
#include "side_index.hpp"
#include "types.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace orderbook::detail {

// Positions follow the price order, slots wrap around the ring.
// Both sides share the levels: a price never holds buys and sells at once
class Window {
private:
    Price tickSize_;
    // The tick of position 0
    std::int64_t base_;
    // Never grows, so the nodes can point into it
    std::vector<Level> levels_;
    SideIndex<Side::buy> bids_;
    SideIndex<Side::sell> asks_;

    static std::int64_t baseTickOf(Price tickSize, Price basePrice, std::size_t size) {
        assert(tickSize > 0);
        assert(basePrice > 0 && basePrice % tickSize == 0);
        assert(std::has_single_bit(size));
        return basePrice / tickSize;
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

    std::size_t slotOf(std::size_t position) const {
        assert(position < levels_.size());
        return static_cast<std::size_t>(base_ + static_cast<std::int64_t>(position)) & (levels_.size() - 1);
    }

public:
    static constexpr std::size_t NONE = Bitmap::NONE;

    Window(Price tickSize, Price basePrice, std::size_t size)
        : tickSize_ {tickSize}, base_ {baseTickOf(tickSize, basePrice, size)},
        levels_(size), bids_(size), asks_(size) {}
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = default;
    Window& operator=(Window&&) = default;

    // Only while empty: the levels keep their slots, so nothing is allocated or moved
    void placeAt(Price basePrice) {
        assert(bids_.best() == NONE && asks_.best() == NONE);
        base_ = baseTickOf(tickSize_, basePrice, levels_.size());
    }

    std::size_t size() const {
        return levels_.size();
    }

    Price tickSize() const {
        return tickSize_;
    }

    // Can pass the largest Price, unlike the prices
    std::int64_t baseTick() const {
        return base_;
    }

    // Tells the window from the outside without dividing, as the hot paths need
    bool holds(const OrderNode* node) const {
        if (levels_.empty()) return false;
        const LevelData* data = node->levelData;
        return std::greater_equal<const LevelData*>()(data, &levels_.front().data)
            && std::less_equal<const LevelData*>()(data, &levels_.back().data);
    }

    Price basePrice() const {
        return static_cast<Price>(base_ * tickSize_);
    }

    std::size_t positionOf(Price price) const {
        // A moved-from window has no slots
        if (levels_.empty()) return NONE;
        // Checked first: the division would round it down to a tick
        if (price % tickSize_ != 0) return NONE;

        // base_ + size can pass the largest Price
        std::int64_t tick = price / tickSize_;
        if (tick < base_ || tick >= base_ + static_cast<std::int64_t>(levels_.size())) return NONE;
        return static_cast<std::size_t>(tick - base_);
    }

    Price priceOf(std::size_t position) const {
        assert(position < levels_.size());
        return static_cast<Price>((base_ + static_cast<std::int64_t>(position)) * tickSize_);
    }

    const Level& level(std::size_t position) const {
        return levels_[slotOf(position)];
    }

    template<Side side>
    bool contains(std::size_t position) const {
        return index<side>().contains(position);
    }

    template<Side side>
    std::size_t best() const {
        return index<side>().best();
    }

    template<Side side>
    std::size_t after(std::size_t position) const {
        return index<side>().after(position);
    }

    void add(std::size_t position, OrderNode* node) {
        assert(node != nullptr);
        assert(position == positionOf(node->order.getPrice()));
        Side side = node->order.getSide();
        assert(side == Side::buy ? !asks_.contains(position) : !bids_.contains(position));

        Level& level = levels_[slotOf(position)];
        bool wasEmpty = level.queue.empty();

        node->level = &level.queue;
        node->levelData = &level.data;
        level.queue.pushBack(node);
        ++level.data.count_;
        level.data.qty_ += node->order.getRemainingQuantity();

        if (wasEmpty) addToIndex(side, position);
    }

    static void fill(OrderNode* node, Quantity quantity) {
        assert(quantity <= node->order.getRemainingQuantity());
        node->order.fill(quantity);
        node->levelData->qty_ -= quantity;
    }

    void remove(OrderNode* node) {
        node->level->unlink(node);
        --node->levelData->count_;
        node->levelData->qty_ -= node->order.getRemainingQuantity();

        if (!node->level->empty()) return;
        removeFromIndex(node->order.getSide(), positionOf(node->order.getPrice()));
    }
};

} // namespace orderbook::detail
