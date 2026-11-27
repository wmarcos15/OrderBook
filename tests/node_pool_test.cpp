#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "node_pool.hpp"

using orderbook::detail::NodeAllocator;
using orderbook::detail::NodePool;

constexpr std::size_t BLOCK = 48;
constexpr std::size_t FIRST = NodePool::FIRST_CHUNK_BLOCKS;
constexpr std::size_t MAX = NodePool::MAX_CHUNK_BLOCKS;
// Blocks in the first seven chunks, the last of them the first one at the cap
constexpr std::size_t CAPPED = FIRST * 127;

// --------------------------
// Allocate and deallocate
// --------------------------
TEST(NodePoolTest, Allocate_LiveBlocks_AreDistinct) {
    NodePool pool;
    std::set<void*> blocks;

    for (std::size_t i {0}; i < 10; ++i) blocks.insert(pool.allocate(BLOCK, 8));

    EXPECT_EQ(blocks.size(), 10u);
}

TEST(NodePoolTest, Allocate_Blocks_AreAligned) {
    NodePool pool;

    for (std::size_t i {0}; i < 10; ++i)
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(pool.allocate(BLOCK, alignof(std::max_align_t))) % alignof(std::max_align_t), 0u);
}

TEST(NodePoolTest, Deallocate_ThenAllocate_ReturnsTheLastOneFreed) {
    NodePool pool;
    void* x = pool.allocate(BLOCK, 8);
    void* y = pool.allocate(BLOCK, 8);

    pool.deallocate(x);
    pool.deallocate(y);

    EXPECT_EQ(pool.allocate(BLOCK, 8), y);
    EXPECT_EQ(pool.allocate(BLOCK, 8), x);
}

TEST(NodePoolTest, BlocksInChunk_StartSmallDoubleAndStopAtTheCap) {
    EXPECT_EQ(NodePool::blocksInChunk(0), 64u);
    EXPECT_EQ(NodePool::blocksInChunk(1), 128u);
    EXPECT_EQ(NodePool::blocksInChunk(2), 256u);
    EXPECT_EQ(NodePool::blocksInChunk(5), 2048u);
    EXPECT_EQ(NodePool::blocksInChunk(6), MAX);
    EXPECT_EQ(NodePool::blocksInChunk(7), MAX);
    EXPECT_EQ(NodePool::blocksInChunk(1000), MAX);
}

TEST(NodePoolTest, Allocate_AcrossChunksOfDifferentSizes_EarlierBlocksNeverMove) {
    NodePool pool;
    std::vector<std::uint64_t*> blocks;

    // Crosses the 64, 128 and 256 boundaries, reaches the cap and starts a second capped chunk
    for (std::size_t i {0}; i < CAPPED + MAX + 1; ++i) {
        auto* block = static_cast<std::uint64_t*>(pool.allocate(BLOCK, 8));
        *block = i;
        blocks.push_back(block);
    }

    EXPECT_EQ(std::set<void*>(blocks.begin(), blocks.end()).size(), blocks.size());
    for (std::size_t i {0}; i < blocks.size(); ++i) EXPECT_EQ(*blocks[i], i);
}

TEST(NodePoolTest, AllocateAndDeallocate_ManyRounds_ReusesTheSameBlocks) {
    NodePool pool;
    std::set<void*> seen;

    for (std::size_t round {0}; round < 100; ++round) {
        std::vector<void*> blocks;
        for (std::size_t i {0}; i < 3 * FIRST + 10; ++i) blocks.push_back(pool.allocate(BLOCK, 8));
        seen.insert(blocks.begin(), blocks.end());
        for (void* block : blocks) pool.deallocate(block);
    }

    EXPECT_EQ(seen.size(), 3 * FIRST + 10);
}

// --------------------------
// Allocator with a map
// --------------------------
TEST(NodeAllocatorTest, Map_InsertAndEraseMany_SurvivingEntriesNeverMove) {
    using Entry = std::pair<const int, int>;
    NodePool pool;
    std::map<int, int, std::less<int>, NodeAllocator<Entry>> map {NodeAllocator<Entry>(&pool)};
    constexpr int KEYS = static_cast<int>(CAPPED);

    std::map<int, const int*> addresses;
    for (int key {0}; key < KEYS; ++key) {
        addresses[key] = &map.try_emplace(key, key * 2).first->second;
        if (key % 3 == 0) {
            map.erase(key / 2);
            addresses.erase(key / 2);
        }
    }

    EXPECT_EQ(map.size(), addresses.size());
    for (const auto& [key, address] : addresses) {
        EXPECT_EQ(&map.at(key), address);
        EXPECT_EQ(*address, key * 2);
    }
}

TEST(NodeAllocatorTest, TwoMapsSharingAPool_DoNotOverlap) {
    using Entry = std::pair<const int, int>;
    NodePool pool;
    std::map<int, int, std::greater<int>, NodeAllocator<Entry>> bids {NodeAllocator<Entry>(&pool)};
    std::map<int, int, std::less<int>, NodeAllocator<Entry>> asks {NodeAllocator<Entry>(&pool)};

    for (int key {0}; key < 1000; ++key) {
        bids.try_emplace(key, key);
        asks.try_emplace(key, -key);
    }

    for (int key {0}; key < 1000; ++key) {
        EXPECT_EQ(bids.at(key), key);
        EXPECT_EQ(asks.at(key), -key);
    }
}

TEST(NodeAllocatorTest, Allocators_CompareByPool) {
    NodePool first;
    NodePool second;

    EXPECT_TRUE(NodeAllocator<int>(&first) == NodeAllocator<long>(&first));
    EXPECT_FALSE(NodeAllocator<int>(&first) == NodeAllocator<int>(&second));
}
