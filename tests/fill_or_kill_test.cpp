#include <gtest/gtest.h>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// The order to fill is on side_ and the resting ones on the other side
class FillOrKillTest : public SidedBookTest {
protected:
    // 10 units in all, over levels far from each other
    void restFarLevels() {
        rest(other_, 10, 3);
        rest(other_, 45'000, 3);
        rest(other_, 90'010, 4);
    }

    void expectKilled(Price price, Quantity quantity) {
        Snapshot before = snapshotOf(book_);

        OrderResult result = add(OrderType::FOK, side_, price, quantity);

        EXPECT_TRUE(result.trades.empty());
        EXPECT_EQ(result.status, OrderStatus::ok);
        EXPECT_TRUE(result.cancelledOrders.empty());
        EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
        EXPECT_EQ(snapshotOf(book_), before);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(FillOrKillTest);

// --------------------------
// Filled
// --------------------------
TEST_P(FillOrKillTest, ExactlyEnough_Filled) {
    OrderID resting = rest(other_, 100, 50);

    OrderResult result = add(OrderType::FOK, side_, 100, 50);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, 100, 50);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    expectEmptyBook(book_);
}

TEST_P(FillOrKillTest, MoreThanEnough_LevelKeepsTheRest) {
    rest(other_, 100, 50);

    Trades trades = add(OrderType::FOK, side_, 100, 20).trades;

    EXPECT_EQ(tradeQuantities(trades), (Quantities {20}));
    expectLevel(other_, 100, 30, 1);
}

TEST_P(FillOrKillTest, EnoughOverSeveralOrdersOfALevel_Filled) {
    for (Quantity quantity : {10, 20, 30})
        rest(other_, 100, quantity);

    Trades trades = add(OrderType::FOK, side_, 100, 60).trades;

    EXPECT_EQ(tradeQuantities(trades), (Quantities {10, 20, 30}));
    expectEmptyBook(book_);
}

TEST_P(FillOrKillTest, ExactlyEnoughOverSeveralLevels_Filled) {
    rest(other_, 102, 50);
    rest(other_, 100, 50);
    rest(other_, 101, 50);

    Trades trades = add(OrderType::FOK, side_, 102, 150).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {100, 101, 102}));
    expectEmptyBook(book_);
}

TEST_P(FillOrKillTest, ExactlyEnoughOverFarLevels_Filled) {
    restFarLevels();

    Trades trades = add(OrderType::FOK, side_, 90'010, 10).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {10, 45'000, 90'010}));
    EXPECT_EQ(tradeQuantities(trades), (Quantities {3, 3, 4}));
    expectEmptyBook(book_);
}

TEST_P(FillOrKillTest, EnoughBeforeTheFarthestLevel_ItStaysUntouched) {
    restFarLevels();

    Trades trades = add(OrderType::FOK, side_, 50'000, 6).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {10, 45'000}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{90'010, 4, 1}}));
}

// --------------------------
// Killed
// --------------------------
TEST_P(FillOrKillTest, EmptyBook_Killed) {
    expectKilled(100, 1);
}

TEST_P(FillOrKillTest, OneShortAtALevel_Killed) {
    rest(other_, 100, 50);
    rest(side_, 98, 50);

    expectKilled(100, 51);
}

TEST_P(FillOrKillTest, OneShortOverSeveralOrdersOfALevel_Killed) {
    for (Quantity quantity : {10, 20, 30})
        rest(other_, 100, quantity);

    expectKilled(100, 61);
}

TEST_P(FillOrKillTest, OneShortOverFarLevels_Killed) {
    restFarLevels();

    expectKilled(90'010, 11);
}

TEST_P(FillOrKillTest, EnoughOnlyBeyondTheLimit_Killed) {
    restFarLevels();

    expectKilled(50'000, 10);
    expectKilled(90'009, 10);
    expectKilled(9, 1);
}

TEST_P(FillOrKillTest, EnoughOnlyOnItsOwnSide_Killed) {
    rest(side_, 100, 50);

    expectKilled(100, 50);
}

TEST_P(FillOrKillTest, Killed_ConsumesAnID) {
    OrderID killed = add(OrderType::FOK, side_, 100, 50).orderID;

    EXPECT_EQ(rest(side_, 100, 50), killed + 1);
}
