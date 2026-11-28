#pragma once

#if __has_include(<sanitizer/asan_interface.h>)
#  include <sanitizer/asan_interface.h>
#endif
#ifndef ASAN_POISON_MEMORY_REGION
#  define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#  define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

namespace orderbook::detail {

// Hands out the nodes of a std::map without calling malloc on every node, and
// without the exception safety problems of the standard pools: a failed
// allocation leaves it unchanged. Every block has the size of the first request.
// Memory comes in chunks that never move and go back only when the pool is
// destroyed. The first has FIRST_CHUNK_BLOCKS blocks and each next one twice the
// previous, up to MAX_CHUNK_BLOCKS, so that a small book stays small.
class NodePool {
public:
    static constexpr std::size_t FIRST_CHUNK_BLOCKS = 64;
    static constexpr std::size_t MAX_CHUNK_BLOCKS = 4096;

    static constexpr std::size_t blocksInChunk(std::size_t index) {
        std::size_t blocks = FIRST_CHUNK_BLOCKS;
        for (std::size_t i {0}; i < index && blocks < MAX_CHUNK_BLOCKS; ++i) blocks *= 2;
        return blocks;
    }

private:
    struct FreeBlock {
        FreeBlock* next;
    };

    std::vector<std::unique_ptr<std::byte[]>> chunks_;
    FreeBlock* free_ {nullptr};
    // 0 until the first request
    std::size_t blockSize_ {0};

    static std::size_t roundUp(std::size_t size) {
        std::size_t alignment = alignof(std::max_align_t);
        return (std::max(size, sizeof(FreeBlock)) + alignment - 1) / alignment * alignment;
    }

    void grow() {
        // so that the push_back at the end cannot throw after the chunk is poisoned
        chunks_.reserve(chunks_.size() + 1);

        std::size_t count = blocksInChunk(chunks_.size());
        auto chunk = std::make_unique_for_overwrite<std::byte[]>(count * blockSize_);

        for (std::size_t i = 0; i < count; ++i) {
            FreeBlock* block = reinterpret_cast<FreeBlock*>(chunk.get() + i * blockSize_);
            block->next = (i + 1 < count) ? reinterpret_cast<FreeBlock*>(chunk.get() + (i + 1) * blockSize_) : nullptr;
            ASAN_POISON_MEMORY_REGION(block, blockSize_);
        }

        free_ = reinterpret_cast<FreeBlock*>(chunk.get());
        chunks_.push_back(std::move(chunk));
    }

public:
    NodePool() = default;
    NodePool(const NodePool&) = delete;
    NodePool& operator=(const NodePool&) = delete;
    NodePool(NodePool&&) = delete;
    NodePool& operator=(NodePool&&) = delete;
    ~NodePool() {
        for (std::size_t i {0}; i < chunks_.size(); ++i)
            ASAN_UNPOISON_MEMORY_REGION(chunks_[i].get(), blocksInChunk(i) * blockSize_);
    }

    void* allocate(std::size_t size, [[maybe_unused]] std::size_t alignment) {
        assert(alignment <= alignof(std::max_align_t));
        assert(blockSize_ == 0 || roundUp(size) == blockSize_);
        // Set only once a chunk exists, so that a failed first request changes nothing
        std::size_t previousSize = blockSize_;
        if (blockSize_ == 0) blockSize_ = roundUp(size);

        if (free_ == nullptr) {
            try {
                grow();
            } catch (...) {
                blockSize_ = previousSize;
                throw;
            }
        }

        FreeBlock* block = free_;
        ASAN_UNPOISON_MEMORY_REGION(block, blockSize_);
        free_ = block->next;
        return block;
    }

    void deallocate(void* block) {
        FreeBlock* freed = static_cast<FreeBlock*>(block);
        freed->next = free_;
        free_ = freed;
        ASAN_POISON_MEMORY_REGION(freed, blockSize_);
    }
};

// A minimal allocator for the nodes of a std::map, which only ever allocates one at a time
template<typename T>
class NodeAllocator {
private:
    NodePool* pool_;

    template<typename U>
    friend class NodeAllocator;

public:
    using value_type = T;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;

    explicit NodeAllocator(NodePool* pool) : pool_(pool) {}
    template<typename U>
    NodeAllocator(const NodeAllocator<U>& other) : pool_(other.pool_) {}

    T* allocate([[maybe_unused]] std::size_t n) {
        assert(n == 1);
        return static_cast<T*>(pool_->allocate(sizeof(T), alignof(T)));
    }

    void deallocate(T* block, [[maybe_unused]] std::size_t n) {
        assert(n == 1);
        pool_->deallocate(block);
    }

    template<typename U>
    bool operator==(const NodeAllocator<U>& other) const {
        return pool_ == other.pool_;
    }
};

} // namespace orderbook::detail
