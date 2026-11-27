#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>
#include "order_queue.hpp"
#include "node_pool.hpp"
#include "orderbook.hpp"
#include "price_levels.hpp"
#include "queue_test_utils.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// --------------------------
// Counting and failing allocations
// --------------------------
static bool counting {false};
static std::size_t allocations {0};
static std::size_t failAt {0}; // counted from 1, 0 for never

static bool shouldFail() {
    return counting && ++allocations == failAt;
}

static void* allocate(std::size_t size) noexcept {
    if (shouldFail()) return nullptr;
    return std::malloc(size == 0 ? 1 : size);
}

static void* allocateOrThrow(std::size_t size) {
    void* memory = allocate(size);
    if (memory == nullptr) throw std::bad_alloc();
    return memory;
}

// Every version, so that all the memory comes from malloc and goes back with free
void* operator new(std::size_t size) { return allocateOrThrow(size); }
void* operator new[](std::size_t size) { return allocateOrThrow(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }

// The aligned versions too: some standard libraries allocate through them (libstdc++'s memory pools)
static void* allocateAligned(std::size_t size, std::align_val_t alignment) noexcept {
    if (shouldFail()) return nullptr;
    std::size_t align = static_cast<std::size_t>(alignment);
    // aligned_alloc wants a multiple of the alignment
    std::size_t rounded = (size == 0) ? align : (size + align - 1) / align * align;
    return std::aligned_alloc(align, rounded);
}

static void* allocateAlignedOrThrow(std::size_t size, std::align_val_t alignment) {
    void* memory = allocateAligned(size, alignment);
    if (memory == nullptr) throw std::bad_alloc();
    return memory;
}

void* operator new(std::size_t size, std::align_val_t alignment) { return allocateAlignedOrThrow(size, alignment); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return allocateAlignedOrThrow(size, alignment); }
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocateAligned(size, alignment); }
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept { return allocateAligned(size, alignment); }
void operator delete(void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept { std::free(memory); }
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept { std::free(memory); }

// Returns whether the allocation was reached, so whether it failed
template<typename Action>
static bool failsAt(std::size_t allocation, Action action) {
    allocations = 0;
    failAt = allocation;
    counting = true;
    bool failed {false};
    try {
        action();
    } catch (const std::bad_alloc&) {
        failed = true;
    }
    counting = false;
    return failed;
}

template<typename Action>
static std::size_t allocationsOf(Action action) {
    failsAt(0, action);
    return allocations;
}

// The table has a page per 4096 IDs, so this ID needs a new one and the list to grow
constexpr OrderID FIRST_OF_A_PAGE = 4'096;

// Small windows, so that a few orders reach the levels outside them
constexpr BookConfig WINDOW_OF_64 {1, 1, 100'000, SelfTradePolicy::cancelNewest, 64};
constexpr BookConfig WINDOW_OF_256 {1, 1, 100'000, SelfTradePolicy::cancelNewest, 256};

class AllocationTest : public SidedBookTest {
protected:
    static constexpr OwnerID OWNER = 30;

    OrderResult send(OrderBook& book, OrderType type, Price price, Quantity quantity) const {
        return book.addOrder(OWNER, type, side_, at(price), quantity);
    }

    OrderID restOther(OrderBook& book, Price price, Quantity quantity) const {
        return book.addOrder(ownerOf(other_), OrderType::GTC, other_, at(price), quantity).orderID;
    }

    // Orders that do not rest, until the next one gets that ID
    void burnIDsUpTo(OrderBook& book, OrderID next) const {
        while (send(book, OrderType::IOC, 1, 1).orderID + 1 < next) {}
    }

    // Every order of the book is found by its ID, and the levels add up to them
    void expectConsistent(const OrderBook& book, OrderID lastID) const {
        std::map<std::pair<Side, Price>, std::pair<Quantity, std::size_t>> totals;
        std::size_t found {0};
        for (OrderID id {1}; id <= lastID; ++id) {
            const Order* order = book.getOrder(id);
            if (order == nullptr) continue;
            EXPECT_FALSE(order->isFilled()) << "order " << id;
            auto& [quantity, count] = totals[{order->getSide(), order->getPrice()}];
            quantity += order->getRemainingQuantity();
            ++count;
            ++found;
        }

        EXPECT_EQ(book.size(), found);
        std::size_t levels {0};
        for (Side side : {Side::buy, Side::sell}) {
            for (const auto& [price, quantity, count] : levelsOf(book, side)) {
                EXPECT_EQ(std::make_pair(quantity, count), (totals[{side, price}])) << "price " << price;
                ++levels;
            }
        }
        EXPECT_EQ(levels, totals.size());
    }

    void expectCanBeEmptied(OrderBook& book) const {
        for (Side side : {Side::buy, Side::sell})
            book.addMarketOrder(999, side, 1'000'000);
        expectEmptyBook(book);
    }

    // Runs the action on a new book for every allocation it makes, failing each in turn
    template<typename Prepare, typename Action, typename Check>
    void failEachAllocation(Prepare prepare, Action action, Check check, const BookConfig& config = {}) {
        std::size_t failures {0};
        for (std::size_t allocation {1}; ; ++allocation) {
            SCOPED_TRACE(::testing::Message() << "allocation " << allocation);
            OrderBook book(config);
            prepare(book);

            if (!failsAt(allocation, [&] { action(book); })) break;

            ++failures;
            check(book);
            OrderID lastID = send(book, OrderType::IOC, 1, 1).orderID;
            expectConsistent(book, lastID);
            expectCanBeEmptied(book);
        }
        EXPECT_GT(failures, 0u);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(AllocationTest);

// --------------------------
// Allocations that fail
// --------------------------
TEST_P(AllocationTest, AddOrderThatRests_Fails_NothingChanges) {
    std::optional<Snapshot> before;
    failEachAllocation(
        [&](OrderBook& book) {
            send(book, OrderType::GTC, 90, 5);
            burnIDsUpTo(book, FIRST_OF_A_PAGE);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { send(book, OrderType::GTC, 100, 10); },
        [&](const OrderBook& book) { EXPECT_EQ(snapshotOf(book), before); });
}

// Like the modify above: the order is not the best, so the window stays and the map takes it
TEST_P(AllocationTest, AddOrderThatRestsFar_Fails_NothingChanges) {
    std::optional<Snapshot> before;
    constexpr OrderID FAR = 3;
    failEachAllocation(
        [&](OrderBook& book) {
            send(book, OrderType::GTC, 90, 5);
            send(book, OrderType::GTC, 80, 5);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { send(book, OrderType::GTC, 20, 10); },
        [&](const OrderBook& book) {
            EXPECT_EQ(snapshotOf(book), before);
            EXPECT_EQ(book.getOrder(FAR), nullptr);
        },
        BookConfig {1, 1, 100'000, SelfTradePolicy::cancelNewest, 64});
}

// The first order takes the first chunk of the pool and the first page
TEST_P(AllocationTest, FirstOrder_Fails_BookStaysEmpty) {
    failEachAllocation(
        [](OrderBook&) {},
        [&](OrderBook& book) { send(book, OrderType::GTC, 100, 10); },
        [](const OrderBook& book) { expectEmptyBook(book); });
}

TEST_P(AllocationTest, AddOrderThatTradesAndRests_Fails_BookIsConsistent) {
    failEachAllocation(
        [&](OrderBook& book) {
            send(book, OrderType::GTC, 90, 5);
            for (Price price : {100, 101, 102, 103, 104})
                restOther(book, price, 10);
            burnIDsUpTo(book, FIRST_OF_A_PAGE);
        },
        [&](OrderBook& book) { send(book, OrderType::GTC, 104, 100); },
        [](const OrderBook&) {});
}

TEST_P(AllocationTest, FirstTradeOfAnOrder_Fails_NothingChanges) {
    std::optional<Snapshot> before;
    failEachAllocation(
        [&](OrderBook& book) {
            restOther(book, 100, 10);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { send(book, OrderType::IOC, 100, 10); },
        [&](const OrderBook& book) { EXPECT_EQ(snapshotOf(book), before); });
}

TEST_P(AllocationTest, FirstSelfTradeCancelled_Fails_NothingChanges) {
    std::optional<Snapshot> before;
    failEachAllocation(
        [&](OrderBook& book) {
            book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
            book.addOrder(OWNER, OrderType::GTC, other_, at(100), 10);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { send(book, OrderType::IOC, 100, 10); },
        [&](const OrderBook& book) { EXPECT_EQ(snapshotOf(book), before); });
}

TEST_P(AllocationTest, ModifyThatMovesAnOrder_Fails_OriginalStays) {
    std::optional<Snapshot> before;
    OrderID id {0};
    failEachAllocation(
        [&](OrderBook& book) {
            id = send(book, OrderType::GTC, 90, 5).orderID;
            burnIDsUpTo(book, FIRST_OF_A_PAGE);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { book.modifyOrder(id, at(95), 5); },
        [&](const OrderBook& book) {
            EXPECT_EQ(snapshotOf(book), before);
            EXPECT_NE(book.getOrder(id), nullptr);
        });
}

// A small window and a best inside it, so that the far price rests in the map. The slot and the page are warm,
// so what fails is the entry of the map
TEST_P(AllocationTest, ModifyThatMovesAnOrderFar_Fails_OriginalStays) {
    std::optional<Snapshot> before;
    OrderID id {0};
    constexpr OrderID REPLACEMENT = 3;
    failEachAllocation(
        [&](OrderBook& book) {
            send(book, OrderType::GTC, 90, 5);
            id = send(book, OrderType::GTC, 80, 5).orderID;
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { book.modifyOrder(id, at(20), 5); },
        [&](const OrderBook& book) {
            EXPECT_EQ(snapshotOf(book), before);
            EXPECT_NE(book.getOrder(id), nullptr);
            EXPECT_EQ(book.getOrder(REPLACEMENT), nullptr);
        },
        BookConfig {1, 1, 100'000, SelfTradePolicy::cancelNewest, 64});
}

// The failures can only come from the move: the order itself lands inside the window, and the pool of the
// map has no chunk yet, so the two allocations of the first one are the move's (see the one that allocates nothing)
TEST_P(AllocationTest, OrderThatMovesTheWindow_Fails_NothingChanges) {
    std::optional<Snapshot> before;
    constexpr OrderID MOVING = 4;
    failEachAllocation(
        [&](OrderBook& book) {
            send(book, OrderType::GTC, 50'000, 5);
            send(book, OrderType::GTC, 49'990, 5);
            send(book, OrderType::GTC, 49'980, 5);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { send(book, OrderType::GTC, 50'028, 10); },
        [&](const OrderBook& book) {
            EXPECT_EQ(snapshotOf(book), before);
            EXPECT_EQ(book.getOrder(MOVING), nullptr);
        },
        WINDOW_OF_64);
}

TEST_P(AllocationTest, OrderThatMovesTheWindowWithNothingLeaving_AllocatesNothing) {
    OrderBook book(WINDOW_OF_64);
    send(book, OrderType::GTC, 50'000, 5);

    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 50'028, 10); }), 0u);
}

// More levels leave than the first chunk of the pool holds, so the failures after it must undo the entries made
TEST_P(AllocationTest, OrderThatMovesTheWindowAndPushesOutManyLevels_Fails_NothingChanges) {
    constexpr OrderID LEAVING = 112;
    static_assert(LEAVING > detail::NodePool::FIRST_CHUNK_BLOCKS);
    std::optional<Snapshot> before;
    constexpr OrderID MOVING = 122;
    failEachAllocation(
        [&](OrderBook& book) {
            send(book, OrderType::GTC, 50'000, 5);
            for (Price price {49'999}; price >= 49'880; --price)
                send(book, OrderType::GTC, price, 1);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { send(book, OrderType::GTC, 50'120, 10); },
        [&](const OrderBook& book) {
            EXPECT_EQ(snapshotOf(book), before);
            EXPECT_EQ(book.getOrder(MOVING), nullptr);
        },
        WINDOW_OF_256);
}

TEST_P(AllocationTest, ModifyThatMovesTheWindow_Fails_OriginalStays) {
    std::optional<Snapshot> before;
    OrderID id {0};
    constexpr OrderID REPLACEMENT = 4;
    failEachAllocation(
        [&](OrderBook& book) {
            id = send(book, OrderType::GTC, 50'000, 5).orderID;
            send(book, OrderType::GTC, 49'990, 5);
            send(book, OrderType::GTC, 49'980, 5);
            before = snapshotOf(book);
        },
        [&](OrderBook& book) { book.modifyOrder(id, at(50'028), 5); },
        [&](const OrderBook& book) {
            EXPECT_EQ(snapshotOf(book), before);
            EXPECT_NE(book.getOrder(id), nullptr);
            EXPECT_EQ(book.getOrder(REPLACEMENT), nullptr);
        },
        WINDOW_OF_64);
}

// With the original still there, its owner could trade more than was asked for
TEST_P(AllocationTest, ModifyThatCrosses_Fails_OriginalStaysOnlyIfNothingWasTraded) {
    OrderID id {0};
    std::size_t kept {0};
    std::size_t gone {0};
    failEachAllocation(
        [&](OrderBook& book) {
            id = send(book, OrderType::GTC, 90, 60).orderID;
            for (Price price : {100, 101, 102, 103, 104})
                restOther(book, price, 10);
            burnIDsUpTo(book, FIRST_OF_A_PAGE);
        },
        [&](OrderBook& book) { book.modifyOrder(id, at(104), 60); },
        [&](const OrderBook& book) {
            bool untouched = levelsOf(book, other_).size() == 5;
            EXPECT_EQ(book.getOrder(id) != nullptr, untouched);
            ++(untouched ? kept : gone);
        });

    EXPECT_GT(kept, 0u);
    EXPECT_GT(gone, 0u);
}

TEST_P(AllocationTest, ModifyThatCancelsOwnOrders_Fails_OriginalStaysOnlyIfNothingWasCancelled) {
    OrderID id {0};
    std::size_t kept {0};
    std::size_t gone {0};
    failEachAllocation(
        [&](OrderBook& book) {
            book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
            id = send(book, OrderType::GTC, 90, 60).orderID;
            for (Price price : {100, 101, 102, 103, 104})
                book.addOrder(OWNER, OrderType::GTC, other_, at(price), 10);
        },
        [&](OrderBook& book) { book.modifyOrder(id, at(104), 60); },
        [&](const OrderBook& book) {
            bool untouched = levelsOf(book, other_).size() == 5;
            EXPECT_EQ(book.getOrder(id) != nullptr, untouched);
            ++(untouched ? kept : gone);
        });

    EXPECT_GT(kept, 0u);
    EXPECT_GT(gone, 0u);
}

// --------------------------
// Allocations that are not made
// --------------------------
TEST_P(AllocationTest, OrdersThatNeitherRestNorTrade_AllocateNothing) {
    OrderBook book;

    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::IOC, 100, 10); }), 0u);
    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::FOK, 100, 10); }), 0u);
    EXPECT_EQ(allocationsOf([&] { book.addMarketOrder(OWNER, side_, 10); }), 0u);
}

TEST_P(AllocationTest, RestCancelAndModify_WithRoomInThePoolAndTable_AllocateNothing) {
    OrderBook book;
    OrderID id = send(book, OrderType::GTC, 90, 10).orderID;

    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 91, 10); }), 0u);
    EXPECT_EQ(allocationsOf([&] { book.modifyOrder(id, at(95), 10); }), 0u);
    EXPECT_EQ(allocationsOf([&] { book.cancelOrder(id + 1); }), 0u);
}

// The first level outside the window makes the first chunk of the pool of the map
TEST_P(AllocationTest, OrderAtANewFarPrice_Allocates) {
    OrderBook book(WINDOW_OF_64);
    send(book, OrderType::GTC, 90, 5);

    EXPECT_GT(allocationsOf([&] { send(book, OrderType::GTC, 20, 7); }), 0u);
}

TEST_P(AllocationTest, OrderAtAFarPriceThatExists_AllocatesNothing) {
    OrderBook book(WINDOW_OF_64);
    send(book, OrderType::GTC, 90, 5);
    send(book, OrderType::GTC, 20, 5);

    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 20, 7); }), 0u);
}

// The nodes of the map come from the pool: a new price takes a free block, and a cancelled one gives it back
TEST_P(AllocationTest, OrderAtANewFarPrice_WithRoomInTheMapPool_AllocatesNothing) {
    OrderBook book(WINDOW_OF_64);
    send(book, OrderType::GTC, 90, 5);
    send(book, OrderType::GTC, 20, 5);

    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 30, 7); }), 0u);
    book.cancelOrder(3);
    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 40, 7); }), 0u);
}

TEST_P(AllocationTest, RestInsideTheWindow_WithFarLevelsAround_AllocatesNothing) {
    OrderBook book(WINDOW_OF_64);
    send(book, OrderType::GTC, 90, 5);
    send(book, OrderType::GTC, 20, 5);
    send(book, OrderType::GTC, 10, 5);

    // Not a best, and then a new best that is far from the edges, so the window stays
    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 80, 5); }), 0u);
    EXPECT_EQ(allocationsOf([&] { send(book, OrderType::GTC, 95, 5); }), 0u);
}

// --------------------------
// Moving the window
// --------------------------
namespace {

struct LevelState {
    Side side;
    Price price;
    Quantity quantity;
    Quantity count;
    std::vector<OrderID> ids;

    bool operator==(const LevelState&) const = default;
};

using Cursor = detail::PriceLevels::Cursor;

// Every level of both sides, and every order of them must point to the level that holds it
std::vector<LevelState> stateOf(const detail::PriceLevels& levels) {
    std::vector<LevelState> state;
    for (Side side : {Side::buy, Side::sell}) {
        Cursor cursor = (side == Side::buy) ? levels.best<Side::buy>() : levels.best<Side::sell>();
        for (; cursor.level != nullptr; cursor = (side == Side::buy) ? levels.after<Side::buy>(cursor) : levels.after<Side::sell>(cursor)) {
            EXPECT_EQ(cursor.level, levels.levelOf(side, cursor.price)) << "price " << cursor.price;
            for (const detail::OrderNode* node = cursor.level->queue.head; node != nullptr; node = node->next) {
                EXPECT_EQ(node->level, &cursor.level->queue) << "order " << node->order.getID();
                EXPECT_EQ(node->levelData, &cursor.level->data) << "order " << node->order.getID();
            }
            state.push_back({side, cursor.price, cursor.level->data.qty_, cursor.level->data.count_, forwardIDs(cursor.level->queue)});
        }
    }
    return state;
}

// Enough levels leave to need more than one chunk of the pool, so that a failure can come after some are made
void failEachAllocationOfAMove(Side leaving) {
    constexpr std::int64_t BASE = 20'000;
    constexpr std::int64_t SIZE = 8'192;
    constexpr std::int64_t DISTANCE = 6'000;
    bool isUp = leaving == Side::buy;
    std::int64_t newBase = isUp ? BASE + DISTANCE : BASE - DISTANCE;

    std::size_t failures {0};
    for (std::size_t allocation {1}; ; ++allocation) {
        SCOPED_TRACE(::testing::Message() << "allocation " << allocation);
        detail::PriceLevels levels(1, 1, 100'000, SIZE);
        levels.placeWindowAt(BASE);
        std::deque<detail::OrderNode> nodes;
        OrderID nextID {1};

        auto rest = [&](Side side, std::int64_t tick) {
            for (Quantity quantity : {10, 20}) {
                nodes.emplace_back(Order(nextID++, 1, OrderType::GTC, side, static_cast<Price>(tick), quantity));
                levels.add(&nodes.back());
            }
        };
        // The ones that stay are on the other side of the window from the ones that leave
        for (std::int64_t tick {0}; tick < DISTANCE; ++tick)
            rest(leaving, isUp ? BASE + tick : BASE + SIZE - 1 - tick);
        for (std::int64_t tick {0}; tick < 5; ++tick)
            rest(otherSide(leaving), isUp ? BASE + SIZE - 5 + tick : BASE + 4 - tick);
        rest(leaving, isUp ? 100 : 90'000);
        std::vector<LevelState> before = stateOf(levels);

        if (!failsAt(allocation, [&] { levels.moveTo(newBase); })) break;

        ++failures;
        EXPECT_EQ(levels.windowBaseTick(), BASE);
        EXPECT_EQ(stateOf(levels), before);
        EXPECT_TRUE(levels.outsideRuleHolds());

        levels.moveTo(newBase);
        EXPECT_EQ(levels.windowBaseTick(), newBase);
        EXPECT_EQ(stateOf(levels), before);
        EXPECT_TRUE(levels.outsideRuleHolds());
    }
    EXPECT_GT(failures, 0u);
}

} // namespace

TEST(MoveAllocationTest, MoveThatPushesBuysOut_Fails_NothingChanges) {
    failEachAllocationOfAMove(Side::buy);
}

TEST(MoveAllocationTest, MoveThatPushesSellsOut_Fails_NothingChanges) {
    failEachAllocationOfAMove(Side::sell);
}

TEST(MoveAllocationTest, MoveWithNothingToPushOut_AllocatesNothing) {
    detail::PriceLevels levels(1, 1, 10'000, 64);
    levels.placeWindowAt(5'000);
    std::deque<detail::OrderNode> nodes;
    nodes.emplace_back(Order(1, 1, OrderType::GTC, Side::buy, 5'040, 10));
    levels.add(&nodes.back());

    EXPECT_EQ(allocationsOf([&] { levels.moveTo(5'020); }), 0u);
    EXPECT_EQ(allocationsOf([&] { levels.moveTo(4'990); }), 0u);
}

// --------------------------
// Node pool
// --------------------------
TEST(NodePoolAllocationTest, ChunkAllocationFails_PoolStaysUnchangedAndUsable) {
    constexpr std::size_t BLOCK = 48;
    // Reaches the cap and starts a second capped chunk
    constexpr std::size_t BLOCKS = detail::NodePool::FIRST_CHUNK_BLOCKS * 127 + detail::NodePool::MAX_CHUNK_BLOCKS + 1;

    std::size_t failures {0};
    for (std::size_t allocation {1}; ; ++allocation) {
        SCOPED_TRACE(::testing::Message() << "allocation " << allocation);
        detail::NodePool pool;
        std::vector<void*> blocks;

        bool failed = failsAt(allocation, [&] {
            for (std::size_t i {0}; i < BLOCKS; ++i) blocks.push_back(pool.allocate(BLOCK, 8));
        });
        if (!failed) break;

        ++failures;
        std::set<void*> live(blocks.begin(), blocks.end());
        EXPECT_EQ(live.size(), blocks.size());

        // Usable: the rest of the run goes through
        for (std::size_t i {blocks.size()}; i < BLOCKS; ++i) live.insert(pool.allocate(BLOCK, 8));
        EXPECT_EQ(live.size(), BLOCKS);
    }
    EXPECT_GT(failures, 0u);
}
