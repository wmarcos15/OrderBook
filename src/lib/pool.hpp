#pragma once
#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace orderbook::detail {

// Hands out slots for objects of type T without calling malloc on every object.
// Memory comes in chunks of ChunkSize slots that never move once created.
template<typename T, std::size_t ChunkSize = 4096>
class Pool {
    static_assert(std::is_trivially_destructible_v<T>, "Pool<T> requires a trivially destructible T");
    static_assert(ChunkSize > 0);

private:
    union Slot {
        T value;
        Slot* nextFree;
        Slot() {}
    };

    std::vector<std::unique_ptr<Slot[]>> chunks_;
    Slot* free_ {nullptr};
    std::size_t size_ {0};

    void grow() {
        auto chunk = std::make_unique<Slot[]>(ChunkSize);

        for (std::size_t i = 0; i + 1 < ChunkSize; ++i)
            chunk[i].nextFree = &chunk[i + 1];
        chunk[ChunkSize - 1].nextFree = nullptr;

        free_ = &chunk[0];
        chunks_.push_back(std::move(chunk));
    }

public:
    Pool() = default;
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&& other) noexcept
        : chunks_(std::exchange(other.chunks_, {})),
        free_(std::exchange(other.free_, nullptr)),
        size_(std::exchange(other.size_, 0)) {}
    Pool& operator=(Pool&& other) noexcept {
        if (this != &other) {
            chunks_ = std::exchange(other.chunks_, {});
            free_ = std::exchange(other.free_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    template<typename... Args>
    T* allocate(Args&&... args) {
        if (free_ == nullptr) grow();
        Slot* slot = free_;
        free_ = slot->nextFree;
        T* object = new (&slot->value) T(std::forward<Args>(args)...);
        ++size_;
        return object;
    }

    void release(T* object) {
        Slot* slot = reinterpret_cast<Slot*>(object);
        slot->nextFree = free_;
        free_ = slot;
        --size_;
    }

    std::size_t size() const {
        return size_;
    }

    std::size_t capacity() const {
        return chunks_.size() * ChunkSize;
    }
};

} // namespace orderbook::detail
