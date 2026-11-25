#include <gtest/gtest.h>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

class CancelOrderTest : public SidedBookTest {};

INSTANTIATE_FOR_BOTH_SIDES(CancelOrderTest);

// --------------------------
// IDs that are not resting
// --------------------------
TEST_P(CancelOrderTest, UnknownID_HasNoEffect) {
    rest(side_, 100, 200);
    Snapshot before = snapshotOf(book_);

    for (OrderID id : {0, 2, 999})
        book_.cancelOrder(id);

    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(CancelOrderTest, EmptyBook_HasNoEffect) {
    book_.cancelOrder(1);

    expectEmptyBook(book_);
}

TEST_P(CancelOrderTest, Twice_SecondHasNoEffect) {
    OrderID id = rest(side_, 100, 50);
    rest(side_, 100, 30);
    book_.cancelOrder(id);

    book_.cancelOrder(id);

    expectLevel(side_, 100, 30, 1);
    EXPECT_EQ(book_.size(), 1u);
}

TEST_P(CancelOrderTest, OrderAlreadyFilled_HasNoEffect) {
    OrderID id = rest(side_, 100, 50);
    rest(side_, 100, 30);
    add(OrderType::IOC, other_, 100, 50);

    book_.cancelOrder(id);

    EXPECT_EQ(book_.getOrder(id), nullptr);
    expectLevel(side_, 100, 30, 1);
}

// --------------------------
// One level
// --------------------------
TEST_P(CancelOrderTest, OnlyOrder_BookIsEmpty) {
    OrderID id = rest(side_, 100, 50);

    book_.cancelOrder(id);

    EXPECT_EQ(book_.getOrder(id), nullptr);
    expectLevel(side_, 100, 0, 0);
    expectEmptyBook(book_);
}

TEST_P(CancelOrderTest, OneOfThreeAtALevel_TheOthersKeepTheirOrder) {
    for (std::size_t cancelled {0}; cancelled < 3; ++cancelled) {
        IDs ids;
        for (Quantity quantity : {10, 20, 30})
            ids.push_back(rest(side_, 100, quantity));

        book_.cancelOrder(ids[cancelled]);

        Quantity left = 60 - 10 * static_cast<Quantity>(cancelled + 1);
        expectLevel(side_, 100, left, 2);
        EXPECT_EQ(book_.getOrder(ids[cancelled]), nullptr);

        ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(cancelled));
        Trades trades = add(OrderType::IOC, other_, 100, 60).trades;
        ASSERT_EQ(trades.size(), 2u);
        for (std::size_t i {0}; i < 2; ++i)
            EXPECT_EQ((side_ == Side::buy) ? trades[i].bidID : trades[i].askID, ids[i]);
        EXPECT_TRUE(book_.empty());
    }
}

TEST_P(CancelOrderTest, RemainderOfAnIncomingOrder_OnlyTheRemainderLeavesTheLevel) {
    rest(other_, 100, 30);
    OrderID id = rest(side_, 100, 100);
    rest(side_, 100, 50);

    book_.cancelOrder(id);

    expectLevel(side_, 100, 50, 1);
}

TEST_P(CancelOrderTest, RestingOrderPartiallyFilled_OnlyTheRemainderLeavesTheLevel) {
    OrderID id = rest(side_, 100, 100);
    rest(side_, 100, 50);
    add(OrderType::IOC, other_, 100, 30);
    expectLevel(side_, 100, 120, 2);

    book_.cancelOrder(id);

    expectLevel(side_, 100, 50, 1);
}

// --------------------------
// Best price
// --------------------------
TEST_P(CancelOrderTest, BestLevel_NextOneBecomesBest) {
    OrderID bestID = rest(side_, 105, 200);
    OrderID nextID = rest(side_, 100, 50);
    rest(side_, 95, 50);

    book_.cancelOrder(bestID);

    expectBest(side_, nextID);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 50, 1}, {95, 50, 1}}));
}

TEST_P(CancelOrderTest, MiddleLevel_BestStays) {
    OrderID bestID = rest(side_, 105, 200);
    OrderID middleID = rest(side_, 100, 50);
    rest(side_, 95, 50);

    book_.cancelOrder(middleID);

    expectBest(side_, bestID);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{105, 200, 1}, {95, 50, 1}}));
}

TEST_P(CancelOrderTest, WorstLevel_BestStays) {
    OrderID bestID = rest(side_, 105, 200);
    OrderID worstID = rest(side_, 100, 50);

    book_.cancelOrder(worstID);

    expectBest(side_, bestID);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{105, 200, 1}}));
}

TEST_P(CancelOrderTest, OneOfTwoAtTheBestLevel_LevelStaysBest) {
    OrderID older = rest(side_, 105, 200);
    OrderID newer = rest(side_, 105, 10);
    rest(side_, 100, 50);

    book_.cancelOrder(older);

    expectBest(side_, newer);
}

TEST_P(CancelOrderTest, OneSide_TheOtherIsUntouched) {
    OrderID id = rest(side_, 100, 50);
    OrderID otherID = rest(other_, 102, 75);

    book_.cancelOrder(id);

    EXPECT_EQ(best(side_), nullptr);
    expectBest(other_, otherID);
    expectLevel(other_, 102, 75, 1);
}
