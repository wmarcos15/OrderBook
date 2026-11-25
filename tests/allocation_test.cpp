#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <new>
#include <optional>
#include <tuple>
#include <utility>
#include "orderbook.hpp"
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
    void failEachAllocation(Prepare prepare, Action action, Check check) {
        std::size_t failures {0};
        for (std::size_t allocation {1}; ; ++allocation) {
            SCOPED_TRACE(::testing::Message() << "allocation " << allocation);
            OrderBook book;
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
