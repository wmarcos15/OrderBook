#pragma once

#if __has_include(<sanitizer/asan_interface.h>)
#  include <sanitizer/asan_interface.h>
#endif
#ifndef ASAN_POISON_MEMORY_REGION
#  define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

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

    void unpoisonAll() {
        for (auto& chunk : chunks_)
            ASAN_UNPOISON_MEMORY_REGION(chunk.get(), ChunkSize * sizeof(Slot));
    }

    void grow() {
        // so that the push_back at the end cannot throw after the chunk is poisoned
        chunks_.reserve(chunks_.size() + 1);

        auto chunk = std::make_unique<Slot[]>(ChunkSize);

        for (std::size_t i = 0; i + 1 < ChunkSize; ++i) {
            chunk[i].nextFree = &chunk[i + 1];
            ASAN_POISON_MEMORY_REGION(&chunk[i], sizeof(Slot));
        }
        chunk[ChunkSize - 1].nextFree = nullptr;
        ASAN_POISON_MEMORY_REGION(&chunk[ChunkSize - 1], sizeof(Slot));

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
            unpoisonAll();
            chunks_ = std::exchange(other.chunks_, {});
            free_ = std::exchange(other.free_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }
    ~Pool() { unpoisonAll(); }

    template<typename... Args>
    T* allocate(Args&&... args) {
        if (free_ == nullptr) grow();
        Slot* slot = free_;
        ASAN_UNPOISON_MEMORY_REGION(slot, sizeof(Slot));
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
        ASAN_POISON_MEMORY_REGION(slot, sizeof(Slot));
    }

    std::size_t size() const {
        return size_;
    }

    std::size_t capacity() const {
        return chunks_.size() * ChunkSize;
    }
};

} // namespace orderbook::detail
