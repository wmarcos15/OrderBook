#include <gtest/gtest.h>
#include <cstddef>
#include <stdexcept>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

class AddOrderTest : public SidedBookTest {};

INSTANTIATE_FOR_BOTH_SIDES(AddOrderTest);

// --------------------------
// Resting
// --------------------------
TEST_P(AddOrderTest, EmptyBook_RestsAsTheBestOfItsSide) {
    OrderResult result = add(OrderType::GTC, side_, 100, 250);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_TRUE(result.cancelledOrders.empty());
    EXPECT_FALSE(book_.empty());
    EXPECT_EQ(book_.size(), 1u);
    expectBest(side_, result.orderID);
    EXPECT_EQ(best(other_), nullptr);
    expectLevel(side_, 100, 250, 1);
}

TEST_P(AddOrderTest, EmptyBook_OrderCanBeReadBack) {
    OrderID id = rest(side_, 100, 250);

    const Order* order = book_.getOrder(id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getID(), id);
    EXPECT_EQ(order->getOwnerID(), ownerOf(side_));
    EXPECT_EQ(order->getType(), OrderType::GTC);
    EXPECT_EQ(order->getSide(), side_);
    EXPECT_EQ(order->getPrice(), at(100));
    EXPECT_EQ(order->getStartQuantity(), 250u);
    EXPECT_EQ(order->getRemainingQuantity(), 250u);
    EXPECT_FALSE(order->isFilled());
}

TEST_P(AddOrderTest, PartiallyFilled_OnlyTheRemainderRests) {
    rest(other_, 100, 30);

    OrderID id = rest(side_, 100, 50);

    const Order* order = book_.getOrder(id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getOwnerID(), ownerOf(side_));
    EXPECT_EQ(order->getType(), OrderType::GTC);
    EXPECT_EQ(order->getSide(), side_);
    EXPECT_EQ(order->getPrice(), at(100));
    EXPECT_EQ(order->getStartQuantity(), 50u);
    EXPECT_EQ(order->getRemainingQuantity(), 20u);
    expectLevel(side_, 100, 20, 1);
}

TEST_P(AddOrderTest, BetterPrice_BecomesBest) {
    rest(side_, 99, 50);

    OrderResult result = add(OrderType::GTC, side_, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    expectBest(side_, result.orderID);
}

TEST_P(AddOrderTest, WorsePrice_BestStays) {
    OrderID bestID = rest(side_, 101, 50);

    rest(side_, 100, 50);

    expectBest(side_, bestID);
}

TEST_P(AddOrderTest, SamePrice_OlderStaysBestAndTotalsAccumulate) {
    OrderID older = rest(side_, 100, 100);

    rest(side_, 100, 50);

    expectBest(side_, older);
    expectLevel(side_, 100, 150, 2);
}

TEST_P(AddOrderTest, DifferentPrices_TotalsAreIndependent) {
    rest(side_, 100, 100);
    rest(side_, 99, 50);

    expectLevel(side_, 100, 100, 1);
    expectLevel(side_, 99, 50, 1);
}

TEST_P(AddOrderTest, InsideTheSpread_RestsAndTheOtherSideIsUntouched) {
    rest(side_, 90, 100);
    OrderID otherID = rest(other_, 105, 100);

    OrderResult result = add(OrderType::GTC, side_, 99, 50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 3u);
    expectBest(side_, result.orderID);
    expectBest(other_, otherID);
    expectLevel(other_, 105, 100, 1);
}

// --------------------------
// Level queries
// --------------------------
TEST_P(AddOrderTest, LevelQueries_AskedForTheOtherSide_ReturnZero) {
    for (Price price : {1, 50, 5'000, 100'000})
        rest(side_, price, 8);

    for (Price price : {1, 50, 5'000, 100'000}) {
        expectLevel(side_, price, 8, 1);
        expectLevel(other_, price, 0, 0);
    }
}

// --------------------------
// Rejected
// --------------------------
TEST_P(AddOrderTest, QuantityOfZero_ThrowsAndLeavesNoTrace) {
    rest(side_, 100, 10);
    rest(other_, 101, 10);
    Snapshot before = snapshotOf(book_);

    for (OrderType type : {OrderType::GTC, OrderType::IOC, OrderType::FOK})
        EXPECT_THROW(add(type, side_, 101, 0), std::invalid_argument);

    EXPECT_EQ(snapshotOf(book_), before);
    EXPECT_EQ(rest(side_, 99, 10), 3u);
}

TEST_P(AddOrderTest, IllegalPrice_ThrowsAndLeavesNoTrace) {
    rest(side_, 100, 10);
    rest(other_, 101, 10);
    Snapshot before = snapshotOf(book_);

    for (OrderType type : {OrderType::GTC, OrderType::IOC, OrderType::FOK}) {
        EXPECT_THROW(add(type, side_, 0, 10), std::invalid_argument);
        EXPECT_THROW(add(type, side_, -1, 10), std::invalid_argument);
        EXPECT_THROW(add(type, side_, 100'001, 10), std::invalid_argument);
    }

    EXPECT_EQ(snapshotOf(book_), before);
    EXPECT_EQ(rest(side_, 99, 10), 3u);
}

// --------------------------
// Many orders
// --------------------------
// The pool and the table grow 4096 orders at a time
TEST_P(AddOrderTest, TenThousandLiveOrders_EachIsFoundAndCanBeCancelled) {
    constexpr OrderID ORDERS = 10'000;
    for (OrderID id {1}; id <= ORDERS; ++id)
        ASSERT_EQ(rest(side_, 1 + static_cast<Price>(id % 100), static_cast<Quantity>(id)), id);

    for (OrderID id {1}; id <= ORDERS; id += 2)
        book_.cancelOrder(id);

    EXPECT_EQ(book_.size(), ORDERS / 2);
    for (OrderID id {1}; id <= ORDERS; ++id) {
        const Order* order = book_.getOrder(id);
        ASSERT_EQ(order == nullptr, id % 2 == 1) << "order " << id;
        if (order == nullptr) continue;
        ASSERT_EQ(order->getID(), id);
        ASSERT_EQ(order->getRemainingQuantity(), id);
    }
}

TEST_P(AddOrderTest, TenThousandOrdersAtOnePrice_SweptInOrderOfArrival) {
    constexpr OrderID ORDERS = 10'000;
    for (OrderID id {1}; id <= ORDERS; ++id)
        rest(side_, 100, 1);

    OrderResult result = add(OrderType::IOC, other_, 100, ORDERS);

    ASSERT_EQ(result.trades.size(), ORDERS);
    for (OrderID id {1}; id <= ORDERS; ++id) {
        const Trade& trade = result.trades[id - 1];
        ASSERT_EQ((side_ == Side::buy) ? trade.bidID : trade.askID, id);
    }
    expectEmptyBook(book_);
}
