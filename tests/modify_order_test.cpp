#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

class ModifyOrderTest : public SidedBookTest {
protected:
    std::optional<OrderResult> modify(OrderID id, Price price, Quantity quantity) {
        return book_.modifyOrder(id, at(price), quantity);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(ModifyOrderTest);

// --------------------------
// Nothing to modify
// --------------------------
TEST_P(ModifyOrderTest, UnknownID_ReturnsNullopt) {
    rest(side_, 98, 250);

    EXPECT_EQ(modify(2, 20, 20), std::nullopt);
    EXPECT_EQ(modify(999, 20, 20), std::nullopt);
}

// The ID is looked up before the price and the quantity are checked
TEST_P(ModifyOrderTest, UnknownIDWithIllegalPriceAndQuantity_ReturnsNullopt) {
    EXPECT_EQ(modify(42, 0, 100), std::nullopt);
    EXPECT_EQ(modify(42, 98, 0), std::nullopt);
}

TEST_P(ModifyOrderTest, SamePriceAndQuantity_NothingChanges) {
    OrderID id = rest(side_, 98, 250);
    rest(side_, 98, 75);
    Snapshot before = snapshotOf(book_);

    auto result = modify(id, 98, 250);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    EXPECT_TRUE(result->trades.empty());
    EXPECT_EQ(result->status, OrderStatus::ok);
    EXPECT_EQ(snapshotOf(book_), before);
    expectBest(side_, id);
}

// --------------------------
// Rejected
// --------------------------
TEST_P(ModifyOrderTest, QuantityOfZero_ThrowsAndLeavesTheOrderAlone) {
    OrderID id = rest(side_, 98, 100);
    Snapshot before = snapshotOf(book_);

    EXPECT_THROW(modify(id, 98, 0), std::invalid_argument);
    EXPECT_THROW(modify(id, 99, 0), std::invalid_argument);

    ASSERT_NE(book_.getOrder(id), nullptr);
    EXPECT_EQ(book_.getOrder(id)->getRemainingQuantity(), 100u);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(ModifyOrderTest, IllegalPrice_ThrowsAndLeavesTheOrderAlone) {
    OrderID id = rest(side_, 98, 100);
    Snapshot before = snapshotOf(book_);

    for (Price price : {0, -1, 100'001})
        EXPECT_THROW(modify(id, price, 100), std::invalid_argument) << "price " << price;

    ASSERT_NE(book_.getOrder(id), nullptr);
    EXPECT_EQ(book_.getOrder(id)->getPrice(), at(98));
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(ModifyOrderTest, Rejected_ConsumesNoID) {
    OrderID id = rest(side_, 98, 100);

    EXPECT_THROW(modify(id, 98, 0), std::invalid_argument);
    EXPECT_THROW(modify(id, 0, 100), std::invalid_argument);

    EXPECT_EQ(rest(side_, 98, 100), id + 1);
}

// --------------------------
// Quantity down
// --------------------------
TEST_P(ModifyOrderTest, QuantityDown_KeepsTheID) {
    OrderID id = rest(side_, 98, 250);

    auto result = modify(id, 98, 100);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    EXPECT_TRUE(result->trades.empty());
    EXPECT_EQ(result->status, OrderStatus::ok);
    EXPECT_EQ(book_.size(), 1u);
}

TEST_P(ModifyOrderTest, QuantityDown_OnlyTheRemainingQuantityChanges) {
    OrderID id = rest(side_, 98, 250);
    rest(side_, 98, 75);

    modify(id, 98, 100);

    const Order* order = book_.getOrder(id);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getRemainingQuantity(), 100u);
    EXPECT_EQ(order->getStartQuantity(), 250u);
    EXPECT_EQ(order->getOwnerID(), ownerOf(side_));
    EXPECT_EQ(order->getPrice(), at(98));
    expectLevel(side_, 98, 175, 2);
}

TEST_P(ModifyOrderTest, QuantityDown_KeepsTimePriority) {
    OrderID id = rest(side_, 98, 250);
    rest(side_, 98, 75);

    modify(id, 98, 100);

    expectBest(side_, id);
}

// Compared with what is left of the order, not with what it started with
TEST_P(ModifyOrderTest, PartiallyFilledQuantityBelowTheRemaining_KeepsTheID) {
    OrderID id = rest(side_, 98, 100);
    add(OrderType::IOC, other_, 98, 40);

    auto result = modify(id, 98, 50);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    expectLevel(side_, 98, 50, 1);
}

TEST_P(ModifyOrderTest, PartiallyFilledQuantityEqualToTheRemaining_NothingChanges) {
    OrderID id = rest(side_, 98, 100);
    rest(side_, 98, 10);
    add(OrderType::IOC, other_, 98, 40);

    auto result = modify(id, 98, 60);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    expectBest(side_, id);
    expectLevel(side_, 98, 70, 2);
}

TEST_P(ModifyOrderTest, PartiallyFilledQuantityBetweenRemainingAndStart_GetsANewID) {
    OrderID id = rest(side_, 98, 100);
    OrderID behind = rest(side_, 98, 10);
    add(OrderType::IOC, other_, 98, 40);

    auto result = modify(id, 98, 80);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->orderID, id);
    expectBest(side_, behind);
    expectLevel(side_, 98, 90, 2);
}

// --------------------------
// Quantity up
// --------------------------
TEST_P(ModifyOrderTest, QuantityUp_GetsANewID) {
    OrderID id = rest(side_, 99, 100);

    auto result = modify(id, 99, 150);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->orderID, id);
    EXPECT_TRUE(result->trades.empty());
    EXPECT_EQ(book_.getOrder(id), nullptr);
    ASSERT_NE(book_.getOrder(result->orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result->orderID)->getOwnerID(), ownerOf(side_));
    expectLevel(side_, 99, 150, 1);
}

TEST_P(ModifyOrderTest, QuantityUp_LosesTimePriority) {
    OrderID id = rest(side_, 99, 100);
    OrderID behind = rest(side_, 99, 100);

    modify(id, 99, 150);

    expectBest(side_, behind);
}

// --------------------------
// Price change
// --------------------------
TEST_P(ModifyOrderTest, PriceChange_OrderMovesWithANewID) {
    OrderID id = rest(side_, 98, 100);
    rest(side_, 98, 30);

    auto result = modify(id, 99, 100);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->orderID, id);
    EXPECT_TRUE(result->trades.empty());
    EXPECT_EQ(book_.getOrder(id), nullptr);
    expectBest(side_, result->orderID);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{99, 100, 1}, {98, 30, 1}}));

    const Order* order = book_.getOrder(result->orderID);
    ASSERT_NE(order, nullptr);
    EXPECT_EQ(order->getOwnerID(), ownerOf(side_));
    EXPECT_EQ(order->getType(), OrderType::GTC);
    EXPECT_EQ(order->getSide(), side_);
    EXPECT_EQ(order->getPrice(), at(99));
}

TEST_P(ModifyOrderTest, PriceChangeWithLessQuantity_OrderMovesWithANewID) {
    OrderID id = rest(side_, 98, 100);

    auto result = modify(id, 97, 40);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->orderID, id);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{97, 40, 1}}));
}

TEST_P(ModifyOrderTest, PriceChangeToAnOccupiedLevel_GoesBehindItsOrders) {
    OrderID id = rest(side_, 98, 100);
    OrderID ahead = rest(side_, 99, 100);

    modify(id, 99, 100);

    expectBest(side_, ahead);
    expectLevel(side_, 99, 200, 2);
}

TEST_P(ModifyOrderTest, PriceChangeAwayAndBack_LosesTimePriority) {
    OrderID id = rest(side_, 99, 100);
    OrderID behind = rest(side_, 99, 100);

    auto moved = modify(id, 98, 100);
    ASSERT_TRUE(moved.has_value());
    modify(moved->orderID, 99, 100);

    expectBest(side_, behind);
}

TEST_P(ModifyOrderTest, BestMovedToAWorsePrice_NextOneBecomesBest) {
    OrderID id = rest(side_, 99, 100);
    OrderID nextID = rest(side_, 95, 100);

    modify(id, 90, 100);

    expectBest(side_, nextID);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{95, 100, 1}, {90, 100, 1}}));
}

// --------------------------
// Crossing the spread
// --------------------------
TEST_P(ModifyOrderTest, PriceChangeCrossesTheSpread_TradesAndTheRemainderRests) {
    OrderID resting = rest(other_, 100, 50);
    OrderID id = rest(side_, 98, 100);

    auto result = modify(id, 100, 100);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->trades.size(), 1u);
    expectTrade(result->trades[0], result->orderID, resting, 100, 50);
    expectBest(side_, result->orderID);
    expectLevel(side_, 100, 50, 1);
    expectLevel(side_, 98, 0, 0);
    EXPECT_EQ(best(other_), nullptr);
}

TEST_P(ModifyOrderTest, PriceAndQuantityChangeCrossTheSpread_FullyFilled) {
    OrderID resting = rest(other_, 101, 200);
    OrderID id = rest(side_, 99, 50);

    auto result = modify(id, 101, 200);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->trades.size(), 1u);
    expectTrade(result->trades[0], result->orderID, resting, 101, 200);
    EXPECT_EQ(book_.getOrder(result->orderID), nullptr);
    expectEmptyBook(book_);
}

TEST_P(ModifyOrderTest, ModifiedOrderFilledLater_TradeCarriesItsOwner) {
    OrderID id = rest(other_, 105, 50);
    auto moved = modify(id, 100, 50);
    ASSERT_TRUE(moved.has_value());

    OrderResult result = add(OrderType::GTC, side_, 100, 50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, moved->orderID, 100, 50);
}
