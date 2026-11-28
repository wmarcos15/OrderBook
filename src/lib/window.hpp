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

    static std::int64_t baseTickOf(Price tickSize, Price basePrice, [[maybe_unused]] std::size_t size) {
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

    // The nodes follow the level, and the old one is left empty
    static void relocate(Level& from, Level& to) noexcept {
        assert(to.queue.empty());
        to = from;
        for (OrderNode* node = to.queue.head; node != nullptr; node = node->next) {
            node->level = &to.queue;
            node->levelData = &to.data;
        }
        from = Level {};
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

    // Takes the level out of its slot but not out of the index: the move does that in one go
    void takeLevel(std::size_t position, Level& destination) noexcept {
        relocate(levels_[slotOf(position)], destination);
    }

    // The slot must be free, which the window leaves it when the level that was there goes out
    void putLevel(Side side, std::size_t position, Level& source) noexcept {
        assert(side == Side::buy ? !asks_.contains(position) : !bids_.contains(position));
        relocate(source, levels_[slotOf(position)]);
        addToIndex(side, position);
    }

    // The levels leaving have been taken. Slots are tied to the ticks, so none of the others moves
    void shiftTo(std::int64_t newBase) noexcept {
        std::int64_t distance = newBase - base_;
        if (distance > 0) {
            bids_.dropFront(static_cast<std::size_t>(distance));
            asks_.dropFront(static_cast<std::size_t>(distance));
        } else {
            bids_.dropBack(static_cast<std::size_t>(-distance));
            asks_.dropBack(static_cast<std::size_t>(-distance));
        }
        base_ = newBase;
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

    [[gnu::always_inline]] void add(std::size_t position, OrderNode* node) {
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

    [[gnu::always_inline]] void remove(OrderNode* node) {
        node->level->unlink(node);
        --node->levelData->count_;
        node->levelData->qty_ -= node->order.getRemainingQuantity();

        if (!node->level->empty()) return;
        // Slots wrap around the ring, so the position comes from the slot without dividing
        std::size_t offset = reinterpret_cast<const char*>(node->levelData) - reinterpret_cast<const char*>(&levels_.front().data);
        std::int64_t slot = static_cast<std::int64_t>(offset / sizeof(Level));
        std::size_t position = static_cast<std::size_t>(slot - base_) & (levels_.size() - 1);
        assert(position == positionOf(node->order.getPrice()));
        removeFromIndex(node->order.getSide(), position);
    }
};

} // namespace orderbook::detail
