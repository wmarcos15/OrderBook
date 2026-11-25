#pragma once
#include "bitmap.hpp"
#include "types.hpp"

#include <cassert>
#include <cstddef>
#include <utility>

namespace orderbook::detail {

// Which positions have orders on one side, and the best one.
// Higher positions are better for buys, lower ones for sells.
template<Side side>
class SideIndex {
private:
    Bitmap bitmap_;
    std::size_t best_ {NONE};

    static bool isBetter(std::size_t a, std::size_t b) {
        if constexpr (side == Side::buy) return a > b;
        else return a < b;
    }

    std::size_t search(std::size_t position) const {
        if constexpr (side == Side::buy) return bitmap_.previous(position);
        else return bitmap_.next(position);
    }

public:
    static constexpr std::size_t NONE = Bitmap::NONE;

    explicit SideIndex(std::size_t positions) : bitmap_(positions) {}
    SideIndex(const SideIndex&) = delete;
    SideIndex& operator=(const SideIndex&) = delete;
    // Moving only copies a number, so best_ is reset by hand
    SideIndex(SideIndex&& other) noexcept
        : bitmap_(std::move(other.bitmap_)),
        best_(std::exchange(other.best_, NONE)) {}
    SideIndex& operator=(SideIndex&& other) noexcept {
        if (this != &other) {
            bitmap_ = std::move(other.bitmap_);
            best_ = std::exchange(other.best_, NONE);
        }
        return *this;
    }

    bool contains(std::size_t position) const {
        return bitmap_.test(position);
    }

    std::size_t best() const {
        return best_;
    }

    std::size_t after(std::size_t position) const {
        if constexpr (side == Side::buy) {
            // Checked before the subtraction: position is unsigned and would wrap around
            if (position == 0) return NONE;
            return search(position - 1);
        } else {
            return search(position + 1);
        }
    }

    void add(std::size_t position) {
        assert(!contains(position));
        bitmap_.set(position);

        // The NONE check is needed for buys, since NONE is the largest number
        if (best_ == NONE || isBetter(position, best_)) best_ = position;
    }

    void remove(std::size_t position) {
        assert(contains(position));
        bitmap_.clear(position);

        if (position == best_) best_ = search(position);
    }
};

} // namespace orderbook::detail
