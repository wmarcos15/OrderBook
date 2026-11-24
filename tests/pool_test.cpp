#include <gtest/gtest.h>
#include <cstddef>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>
#include "pool.hpp"

using orderbook::detail::Pool;

struct Dummy {
    int a;
    int b;
    Dummy(int a, int b) : a(a), b(b) {}
};

using DummyPool = Pool<Dummy, 4>;

// --------------------------
// Allocate and release
// --------------------------
TEST(PoolTest, Allocate_BuildsObjectFromArguments) {
    DummyPool pool;

    Dummy* d = pool.allocate(3, 4);

    EXPECT_EQ(d->a, 3);
    EXPECT_EQ(d->b, 4);
}

TEST(PoolTest, Allocate_TwoLiveObjects_HaveDifferentAddresses) {
    DummyPool pool;

    Dummy* x = pool.allocate(1, 2);
    Dummy* y = pool.allocate(3, 4);

    EXPECT_NE(x, y);
}

TEST(PoolTest, Release_ThenAllocate_ReturnsSameAddress) {
    DummyPool pool;
    Dummy* x = pool.allocate(1, 2);

    pool.release(x);
    Dummy* y = pool.allocate(5, 6);

    EXPECT_EQ(x, y);
    EXPECT_EQ(y->a, 5);
    EXPECT_EQ(y->b, 6);
}

TEST(PoolTest, Release_TwoSlots_ReusedLastInFirstOut) {
    DummyPool pool;
    Dummy* x = pool.allocate(1, 1);
    Dummy* y = pool.allocate(2, 2);

    pool.release(x);
    pool.release(y);

    EXPECT_EQ(pool.allocate(3, 3), y);
    EXPECT_EQ(pool.allocate(4, 4), x);
}

// --------------------------
// Growth
// --------------------------
TEST(PoolTest, Allocate_PastOneChunk_AllAddressesDistinctAndValuesIntact) {
    DummyPool pool;
    std::vector<Dummy*> objects;

    for (int i = 0; i < 10; ++i) {
        objects.push_back(pool.allocate(i, i * 10));
        if (i == 4) {
            // The second chunk now exists; the first one must not have moved.
            for (int j = 0; j < 4; ++j) {
                EXPECT_EQ(objects[j]->a, j);
                EXPECT_EQ(objects[j]->b, j * 10);
            }
        }
    }

    std::set<Dummy*> addresses(objects.begin(), objects.end());
    EXPECT_EQ(addresses.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(objects[i]->a, i);
        EXPECT_EQ(objects[i]->b, i * 10);
    }
}

TEST(PoolTest, Size_FollowsAllocateAndRelease) {
    DummyPool pool;
    EXPECT_EQ(pool.size(), 0u);

    Dummy* x = pool.allocate(1, 1);
    EXPECT_EQ(pool.size(), 1u);
    Dummy* y = pool.allocate(2, 2);
    EXPECT_EQ(pool.size(), 2u);

    pool.release(x);
    EXPECT_EQ(pool.size(), 1u);
    pool.release(y);
    EXPECT_EQ(pool.size(), 0u);
}

TEST(PoolTest, Capacity_StartsAtZeroAndGrowsByChunkSize) {
    DummyPool pool;
    EXPECT_EQ(pool.capacity(), 0u);

    pool.allocate(0, 0);
    EXPECT_EQ(pool.capacity(), 4u);

    pool.allocate(1, 1);
    pool.allocate(2, 2);
    pool.allocate(3, 3);
    EXPECT_EQ(pool.capacity(), 4u);

    pool.allocate(4, 4);
    EXPECT_EQ(pool.capacity(), 8u);
}

TEST(PoolTest, Capacity_AfterReleasingEverything_DoesNotShrink) {
    DummyPool pool;
    std::vector<Dummy*> objects;
    for (int i = 0; i < 5; ++i)
        objects.push_back(pool.allocate(i, i));

    for (Dummy* d : objects)
        pool.release(d);

    EXPECT_EQ(pool.size(), 0u);
    EXPECT_EQ(pool.capacity(), 8u);
}

TEST(PoolTest, SteadyAllocateReleaseCycles_DoNotGrowCapacity) {
    DummyPool pool;
    for (int i = 0; i < 3; ++i)
        pool.allocate(i, i);

    for (int i = 0; i < 1000; ++i) {
        Dummy* d = pool.allocate(i, i);
        pool.release(d);
    }

    EXPECT_EQ(pool.capacity(), 4u);
    EXPECT_EQ(pool.size(), 3u);
}

TEST(PoolTest, Release_AfterGrowth_SlotsOfAllChunksAreReused) {
    DummyPool pool;
    std::vector<Dummy*> first;
    for (int i = 0; i < 8; ++i)
        first.push_back(pool.allocate(i, i));
    for (Dummy* d : first)
        pool.release(d);

    std::set<Dummy*> second;
    for (int i = 0; i < 8; ++i)
        second.insert(pool.allocate(i, i));

    EXPECT_EQ(pool.capacity(), 8u);
    EXPECT_EQ(second, std::set<Dummy*>(first.begin(), first.end()));
}

// --------------------------
// Move semantics
// --------------------------
TEST(PoolTest, MoveConstructed_ObjectsStayIntactAndPoolWorks) {
    DummyPool source;
    std::vector<Dummy*> objects;
    for (int i = 0; i < 5; ++i)
        objects.push_back(source.allocate(i, i + 100));
    std::vector<Dummy*> addresses = objects;

    DummyPool dest(std::move(source));

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(objects[i], addresses[i]);
        EXPECT_EQ(objects[i]->a, i);
        EXPECT_EQ(objects[i]->b, i + 100);
    }
    EXPECT_EQ(dest.size(), 5u);
    EXPECT_EQ(dest.capacity(), 8u);

    Dummy* extra = dest.allocate(7, 8);
    EXPECT_EQ(extra->a, 7);
    EXPECT_EQ(dest.size(), 6u);
    dest.release(extra);
    EXPECT_EQ(dest.size(), 5u);

    // Objects born in the source can be released into the destination.
    for (Dummy* d : objects)
        dest.release(d);
    EXPECT_EQ(dest.size(), 0u);
}

TEST(PoolTest, MoveConstructed_SourceIsEmptyAndReusable) {
    DummyPool source;
    std::vector<Dummy*> objects;
    for (int i = 0; i < 5; ++i)
        objects.push_back(source.allocate(i, i));

    DummyPool dest(std::move(source));

    EXPECT_EQ(source.size(), 0u);
    EXPECT_EQ(source.capacity(), 0u);

    Dummy* fresh = source.allocate(9, 9);
    EXPECT_EQ(source.capacity(), 4u);
    EXPECT_EQ(fresh->a, 9);
    for (Dummy* d : objects)
        EXPECT_NE(fresh, d);
}

TEST(PoolTest, MoveAssigned_TargetTakesSourceAndSourceIsEmpty) {
    DummyPool source;
    std::vector<Dummy*> objects;
    for (int i = 0; i < 5; ++i)
        objects.push_back(source.allocate(i, i + 100));

    DummyPool target;
    target.allocate(50, 50);
    target.allocate(51, 51);
    ASSERT_EQ(target.capacity(), 4u);

    target = std::move(source);

    EXPECT_EQ(target.size(), 5u);
    EXPECT_EQ(target.capacity(), 8u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(objects[i]->a, i);
        EXPECT_EQ(objects[i]->b, i + 100);
    }

    EXPECT_EQ(source.size(), 0u);
    EXPECT_EQ(source.capacity(), 0u);
    Dummy* fresh = source.allocate(9, 9);
    EXPECT_EQ(source.capacity(), 4u);
    EXPECT_EQ(fresh->a, 9);
}

TEST(PoolTest, MoveAssigned_SelfAssignment_LeavesPoolUnchanged) {
    DummyPool pool;
    std::vector<Dummy*> objects;
    for (int i = 0; i < 5; ++i)
        objects.push_back(pool.allocate(i, i + 100));

    DummyPool& same = pool;
    pool = std::move(same);

    EXPECT_EQ(pool.size(), 5u);
    EXPECT_EQ(pool.capacity(), 8u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(objects[i]->a, i);
        EXPECT_EQ(objects[i]->b, i + 100);
    }

    Dummy* extra = pool.allocate(7, 8);
    EXPECT_EQ(extra->a, 7);
    EXPECT_EQ(pool.size(), 6u);
}

// --------------------------
// Type traits
// --------------------------
TEST(PoolTest, Pool_Copy_IsNotAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<DummyPool>);
    EXPECT_FALSE(std::is_copy_assignable_v<DummyPool>);
}

TEST(PoolTest, Pool_Move_IsAllowedAndNoexcept) {
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<DummyPool>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<DummyPool>);
}
