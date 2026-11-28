#include <gtest/gtest.h>
#include <stdexcept>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// The market order is on side_ and the resting ones on the other side
class MarketOrderTest : public SidedBookTest {
protected:
    OrderResult market(Quantity quantity) {
        return book_.addMarketOrder(ownerOf(side_), side_, quantity);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(MarketOrderTest);

TEST_P(MarketOrderTest, SameQuantity_FilledAndNothingRests) {
    OrderID resting = rest(other_, 100, 50);

    OrderResult result = market(50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 50);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectEmptyBook(book_);
}

TEST_P(MarketOrderTest, MoreThanThereIs_RemainderDiscarded) {
    OrderID resting = rest(other_, 100, 30);

    OrderResult result = market(100);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 30);
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectEmptyBook(book_);
}

TEST_P(MarketOrderTest, LessThanThereIs_LevelKeepsTheRest) {
    rest(other_, 100, 50);

    market(20);

    expectLevel(other_, 100, 30, 1);
    EXPECT_EQ(best(side_), nullptr);
}

TEST_P(MarketOrderTest, SeveralLevels_BestPriceFirst) {
    rest(other_, 102, 5);
    rest(other_, 100, 5);
    rest(other_, 101, 5);

    Trades trades = market(15).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {100, 101, 102}));
    EXPECT_EQ(tradeQuantities(trades), (Quantities {5, 5, 5}));
    expectEmptyBook(book_);
}

TEST_P(MarketOrderTest, FarLevels_LastOneKeepsTheRest) {
    rest(other_, 10, 5);
    rest(other_, 90'010, 5);
    rest(other_, 99'990, 5);

    Trades trades = market(12).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {10, 90'010, 99'990}));
    EXPECT_EQ(tradeQuantities(trades), (Quantities {5, 5, 2}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{99'990, 3, 1}}));
}

// The limit of a market order is the edge of the band
TEST_P(MarketOrderTest, OnlyTheWorstPriceOfTheBand_TradesThere) {
    rest(other_, 100'000, 5);

    Trades trades = market(3).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {100'000}));
    expectLevel(other_, 100'000, 2, 1);
}

TEST_P(MarketOrderTest, BothEdgesOfTheBand_TradesAtBoth) {
    rest(other_, 1, 5);
    rest(other_, 100'000, 5);

    Trades trades = market(10).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {1, 100'000}));
    expectEmptyBook(book_);
}

// --------------------------
// Nothing to trade with
// --------------------------
TEST_P(MarketOrderTest, EmptyBook_NoTradesAndConsumesAnID) {
    OrderResult result = market(50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectEmptyBook(book_);
    EXPECT_EQ(rest(side_, 100, 50), result.orderID + 1);
}

TEST_P(MarketOrderTest, OnlyItsOwnSideHasOrders_NoTradesAndBookUntouched) {
    rest(side_, 100, 50);
    Snapshot before = snapshotOf(book_);

    OrderResult result = market(50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(snapshotOf(book_), before);
}

// --------------------------
// Rejected
// --------------------------
TEST_P(MarketOrderTest, QuantityOfZeroOnAnEmptyBook_ThrowsAndConsumesNoID) {
    expectInvalidArgument([&] { market(0); }, "quantity must be greater than zero");

    EXPECT_EQ(rest(side_, 100, 50), 1u);
}

TEST_P(MarketOrderTest, QuantityOfZeroWithOrdersToTake_ThrowsAndLeavesNoTrace) {
    OrderID resting = rest(other_, 100, 50);
    Snapshot before = snapshotOf(book_);

    expectInvalidArgument([&] { market(0); }, "quantity must be greater than zero");

    EXPECT_EQ(snapshotOf(book_), before);
    EXPECT_EQ(rest(other_, 100, 50), resting + 1);
}
