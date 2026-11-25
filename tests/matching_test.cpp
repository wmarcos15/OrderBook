#include <gtest/gtest.h>
#include <cstddef>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// The incoming order is on side_ and the resting ones on the other side
class MatchingTest : public SidedBookTest {
protected:
    void restThreeLevels() {
        rest(other_, 102, 100);
        rest(other_, 100, 50);
        rest(other_, 101, 75);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(MatchingTest);

// --------------------------
// GTC
// --------------------------
TEST_P(MatchingTest, GTC_SameQuantity_BothConsumed) {
    OrderID resting = rest(other_, 100, 50);

    OrderResult result = add(OrderType::GTC, side_, 100, 50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 50);
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_EQ(book_.getOrder(resting), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectLevel(other_, 100, 0, 0);
    expectEmptyBook(book_);
}

TEST_P(MatchingTest, GTC_LargerThanTheResting_RemainderRests) {
    OrderID resting = rest(other_, 99, 100);

    OrderResult result = add(OrderType::GTC, side_, 99, 150);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 99, 100);
    EXPECT_EQ(best(other_), nullptr);
    expectBest(side_, result.orderID);
    EXPECT_EQ(best(side_)->getRemainingQuantity(), 50u);
    expectLevel(side_, 99, 50, 1);
}

TEST_P(MatchingTest, GTC_SmallerThanTheResting_RestingKeepsTheRest) {
    OrderID resting = rest(other_, 99, 150);

    OrderResult result = add(OrderType::GTC, side_, 99, 100);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 99, 100);
    EXPECT_EQ(best(side_), nullptr);
    expectBest(other_, resting);
    EXPECT_EQ(best(other_)->getRemainingQuantity(), 50u);
    EXPECT_EQ(best(other_)->getStartQuantity(), 150u);
    expectLevel(other_, 99, 50, 1);
}

TEST_P(MatchingTest, GTC_LimitBetterThanTheResting_TradesAtTheRestingPrice) {
    OrderID resting = rest(other_, 100, 50);

    OrderResult result = add(OrderType::GTC, side_, 105, 50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 50);
}

TEST_P(MatchingTest, GTC_LimitOneTickShort_BothRest) {
    rest(other_, 101, 50);

    OrderResult result = add(OrderType::GTC, side_, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    expectLevel(side_, 100, 50, 1);
    expectLevel(other_, 101, 50, 1);
}

TEST_P(MatchingTest, GTC_LevelBeyondTheLimit_IsUntouched) {
    rest(other_, 101, 125);
    OrderID near = rest(other_, 99, 100);

    OrderResult result = add(OrderType::GTC, side_, 100, 150);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, near, 99, 100);
    expectLevel(other_, 101, 125, 1);
    expectLevel(side_, 100, 50, 1);
}

// --------------------------
// Sweeps
// --------------------------
TEST_P(MatchingTest, Sweep_ThreeLevels_BestPriceFirst) {
    restThreeLevels();

    Trades trades = add(OrderType::GTC, side_, 102, 225).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {100, 101, 102}));
    EXPECT_EQ(tradeQuantities(trades), (Quantities {50, 75, 100}));
    expectEmptyBook(book_);
}

TEST_P(MatchingTest, Sweep_LimitInTheMiddle_LevelsBeyondItStay) {
    restThreeLevels();

    Trades trades = add(OrderType::GTC, side_, 101, 125).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {100, 101}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{102, 100, 1}}));
    EXPECT_EQ(best(side_), nullptr);
}

TEST_P(MatchingTest, Sweep_EndsInsideALevel_LevelKeepsTheRest) {
    restThreeLevels();

    Trades trades = add(OrderType::GTC, side_, 102, 100).trades;

    EXPECT_EQ(tradeQuantities(trades), (Quantities {50, 50}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{101, 25, 1}, {102, 100, 1}}));
}

TEST_P(MatchingTest, Sweep_MoreThanThereIs_RemainderRestsAtTheLimit) {
    restThreeLevels();

    Trades trades = add(OrderType::GTC, side_, 103, 300).trades;

    EXPECT_EQ(trades.size(), 3u);
    EXPECT_EQ(best(other_), nullptr);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{103, 75, 1}}));
}

// --------------------------
// Time priority
// --------------------------
TEST_P(MatchingTest, TimePriority_OlderFilledFirstAndNewerUntouched) {
    OrderID older = rest(other_, 100, 50);
    OrderID newer = rest(other_, 100, 50);

    OrderResult result = add(OrderType::GTC, side_, 100, 50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, older, 100, 50);
    expectBest(other_, newer);
    expectLevel(other_, 100, 50, 1);
}

TEST_P(MatchingTest, TimePriority_OneOrderAcrossTwo_OlderFilledFirst) {
    OrderID older = rest(other_, 100, 10);
    OrderID newer = rest(other_, 100, 10);

    OrderResult result = add(OrderType::GTC, side_, 100, 15);

    ASSERT_EQ(result.trades.size(), 2u);
    expectTrade(result.trades[0], result.orderID, older, 100, 10);
    expectTrade(result.trades[1], result.orderID, newer, 100, 5);
    expectLevel(other_, 100, 5, 1);
}

TEST_P(MatchingTest, TimePriority_PartiallyFilled_KeepsItsPlace) {
    OrderID older = rest(other_, 100, 50);
    rest(other_, 100, 50);
    add(OrderType::IOC, side_, 100, 20);

    OrderResult result = add(OrderType::IOC, side_, 100, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, older, 100, 10);
    expectBest(other_, older);
    expectLevel(other_, 100, 70, 2);
}

// --------------------------
// Owners
// --------------------------
TEST_P(MatchingTest, Owners_SweepOfDifferentMakers_EachTradeNamesItsOwn) {
    constexpr OwnerID MAKER_A = 10;
    constexpr OwnerID MAKER_B = 11;
    constexpr OwnerID MAKER_C = 12;
    constexpr OwnerID TAKER   = 20;
    book_.addOrder(MAKER_A, OrderType::GTC, other_, at(100), 10);
    book_.addOrder(MAKER_B, OrderType::GTC, other_, at(100), 10);
    book_.addOrder(MAKER_C, OrderType::GTC, other_, at(101), 10);

    Trades trades = book_.addOrder(TAKER, OrderType::GTC, side_, at(101), 30).trades;

    ASSERT_EQ(trades.size(), 3u);
    const OwnerID makers[] {MAKER_A, MAKER_B, MAKER_C};
    for (std::size_t i {0}; i < 3; ++i) {
        EXPECT_EQ((side_ == Side::buy) ? trades[i].askOwner : trades[i].bidOwner, makers[i]);
        EXPECT_EQ((side_ == Side::buy) ? trades[i].bidOwner : trades[i].askOwner, TAKER);
    }
}

// --------------------------
// IOC
// --------------------------
TEST_P(MatchingTest, IOC_SameQuantity_FilledAndNothingRests) {
    OrderID resting = rest(other_, 100, 50);

    OrderResult result = add(OrderType::IOC, side_, 100, 50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 50);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectEmptyBook(book_);
}

TEST_P(MatchingTest, IOC_LargerThanTheResting_RemainderDiscarded) {
    OrderID resting = rest(other_, 100, 30);

    OrderResult result = add(OrderType::IOC, side_, 100, 100);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 30);
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectEmptyBook(book_);
}

TEST_P(MatchingTest, IOC_LimitOneTickShort_DiscardedWithoutTrades) {
    rest(other_, 101, 50);
    Snapshot before = snapshotOf(book_);

    OrderResult result = add(OrderType::IOC, side_, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(MatchingTest, IOC_EmptyBook_DiscardedAndConsumesAnID) {
    OrderResult result = add(OrderType::IOC, side_, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    expectEmptyBook(book_);
    EXPECT_EQ(rest(side_, 100, 50), result.orderID + 1);
}

TEST_P(MatchingTest, IOC_LimitInTheMiddle_LevelsBeyondItStay) {
    restThreeLevels();

    OrderResult result = add(OrderType::IOC, side_, 101, 200);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {100, 101}));
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{102, 100, 1}}));
    EXPECT_EQ(best(side_), nullptr);
}
