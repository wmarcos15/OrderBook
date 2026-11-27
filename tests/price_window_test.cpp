#include <gtest/gtest.h>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <tuple>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

constexpr Price MAX_PRICE = std::numeric_limits<Price>::max();

// Every price from low to high with one order of one share, best first as a buy sees it
static Levels ladder(Price low, Price high) {
    Levels levels;
    for (Price price {high}; price >= low; --price)
        levels.push_back({price, 1, 1});
    return levels;
}

// --------------------------
// Default book, any price on the tick
// --------------------------
TEST(DefaultBandTest, Bids_AtLowestAndHighestPrice_RestCancelAndShowInDepth) {
    OrderBook book;
    OrderID low = book.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 5).orderID;
    OrderID high = book.addOrder(BUYER, OrderType::GTC, Side::buy, MAX_PRICE, 6).orderID;

    EXPECT_EQ(book.getBestBid()->getID(), high);
    EXPECT_EQ(levelsOf(book, Side::buy), (Levels {{MAX_PRICE, 6, 1}, {1, 5, 1}}));
    EXPECT_EQ(book.getLevelQuantity(Side::buy, 1), 5u);
    EXPECT_EQ(book.getLevelCount(Side::buy, MAX_PRICE), 1u);

    book.cancelOrder(high);

    EXPECT_EQ(book.getBestBid()->getID(), low);
    EXPECT_EQ(book.getLevelQuantity(Side::buy, MAX_PRICE), 0u);
    book.cancelOrder(low);
    expectEmptyBook(book);
}

TEST(DefaultBandTest, Asks_AtLowestAndHighestPrice_RestCancelAndShowInDepth) {
    OrderBook book;
    OrderID high = book.addOrder(SELLER, OrderType::GTC, Side::sell, MAX_PRICE, 6).orderID;
    OrderID low = book.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 5).orderID;

    EXPECT_EQ(book.getBestAsk()->getID(), low);
    EXPECT_EQ(levelsOf(book, Side::sell), (Levels {{1, 5, 1}, {MAX_PRICE, 6, 1}}));

    book.cancelOrder(low);

    EXPECT_EQ(book.getBestAsk()->getID(), high);
    EXPECT_EQ(book.getLevelCount(Side::sell, 1), 0u);
    book.cancelOrder(high);
    expectEmptyBook(book);
}

TEST(DefaultBandTest, BidAtOneAndAskAtHighestPrice_DoNotCross) {
    OrderBook book;
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 10);

    OrderResult result = book.addOrder(SELLER, OrderType::GTC, Side::sell, MAX_PRICE, 10);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book.size(), 2u);
}

TEST(DefaultBandTest, Match_AtLowestAndHighestPrice_TradesAtTheRestingPrice) {
    OrderBook book;
    book.addOrder(SELLER, OrderType::GTC, Side::sell, MAX_PRICE, 10);
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 10);

    OrderResult buy = book.addOrder(BUYER, OrderType::IOC, Side::buy, MAX_PRICE, 4);
    OrderResult sell = book.addOrder(SELLER, OrderType::IOC, Side::sell, 1, 4);

    EXPECT_EQ(tradePrices(buy.trades), (Prices {MAX_PRICE}));
    EXPECT_EQ(tradePrices(sell.trades), (Prices {1}));
    EXPECT_EQ(book.getLevelQuantity(Side::sell, MAX_PRICE), 6u);
    EXPECT_EQ(book.getLevelQuantity(Side::buy, 1), 6u);
}

TEST(DefaultBandTest, MarketBuy_AsksAcrossTheWholeRange_SweepsThemInPriceOrder) {
    OrderBook book;
    for (Price price : {MAX_PRICE, 2, 1'000'000'000})
        book.addOrder(SELLER, OrderType::GTC, Side::sell, price, 3);

    OrderResult result = book.addMarketOrder(BUYER, Side::buy, 9);

    EXPECT_EQ(tradePrices(result.trades), (Prices {2, 1'000'000'000, MAX_PRICE}));
    expectEmptyBook(book);
}

TEST(DefaultBandTest, MarketSell_BidsAcrossTheWholeRange_SweepsThemInPriceOrder) {
    OrderBook book;
    for (Price price : {1, 70'000, MAX_PRICE})
        book.addOrder(BUYER, OrderType::GTC, Side::buy, price, 3);

    OrderResult result = book.addMarketOrder(SELLER, Side::sell, 9);

    EXPECT_EQ(tradePrices(result.trades), (Prices {MAX_PRICE, 70'000, 1}));
    expectEmptyBook(book);
}

TEST(DefaultBandTest, TickOfFive_HighestPriceOnTheTick_RestsAndTheLargestPriceThrows) {
    OrderBook book {BookConfig {.tickSize = 5, .minPrice = 5}};
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 2'147'483'645, 2);
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 5, 2);
    Snapshot before = snapshotOf(book);

    EXPECT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, MAX_PRICE, 1), std::invalid_argument);
    EXPECT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 2'147'483'644, 1), std::invalid_argument);

    EXPECT_EQ(snapshotOf(book), before);
    OrderResult result = book.addMarketOrder(SELLER, Side::sell, 4);
    EXPECT_EQ(tradePrices(result.trades), (Prices {2'147'483'645, 5}));
}

// --------------------------
// Levels far from the market
// --------------------------
constexpr Price MARKET = 5'000'000;
constexpr Price FAR = 100'000;
constexpr BookConfig SMALL_WINDOW {1, 1, 10'000'000, SelfTradePolicy::cancelNewest, 64};

class PriceWindowTest : public SidedBookTest {
protected:
    PriceWindowTest() : SidedBookTest(SMALL_WINDOW) {}

    // The incoming order is on other_, so the resting one is the bid of a sell and the ask of a buy
    OrderID restingOf(const Trade& trade) const {
        return (side_ == Side::buy) ? trade.bidID : trade.askID;
    }

    // Orders of 5 at the market, 10 below it, 1'000 below it and FAR below it
    void restFourLevels() {
        for (Price price : {MARKET, MARKET - 10, MARKET - 1'000, MARKET - FAR})
            rest(side_, price, 5);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(PriceWindowTest);

TEST_P(PriceWindowTest, FarOrder_RestAndCancel_LevelComesAndGoes) {
    OrderID near = rest(side_, MARKET, 10);
    OrderID far = rest(side_, MARKET - FAR, 7);

    expectBest(side_, near);
    expectLevel(side_, MARKET - FAR, 7, 1);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET, 10, 1}, {MARKET - FAR, 7, 1}}));
    EXPECT_EQ(book_.getOrder(far)->getPrice(), at(MARKET - FAR));

    book_.cancelOrder(far);

    expectLevel(side_, MARKET - FAR, 0, 0);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET, 10, 1}}));
}

TEST_P(PriceWindowTest, FarLevel_SeveralOrders_KeepTimePriority) {
    rest(side_, MARKET, 10);
    OrderID first = rest(side_, MARKET - FAR, 3);
    OrderID second = rest(side_, MARKET - FAR, 4);
    rest(side_, MARKET - FAR, 5);

    expectLevel(side_, MARKET - FAR, 12, 3);
    book_.cancelOrder(first);
    expectLevel(side_, MARKET - FAR, 9, 2);

    OrderResult result = add(OrderType::IOC, other_, MARKET - FAR, 14);

    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(restingOf(result.trades[1]), second);
    EXPECT_EQ(result.trades[1].quantity, 4u);
    expectLevel(side_, MARKET - FAR, 5, 1);
}

TEST_P(PriceWindowTest, Sweep_StartsInTheWindowAndContinuesIntoFarLevels_InPriceOrder) {
    restFourLevels();

    OrderResult result = add(OrderType::IOC, other_, MARKET - FAR, 17);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {MARKET, MARKET - 10, MARKET - 1'000, MARKET - FAR}));
    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {5, 5, 5, 2}));
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET - FAR, 3, 1}}));
}

TEST_P(PriceWindowTest, Sweep_LimitStopsBeforeFarLevels_LeavesThemUntouched) {
    restFourLevels();

    OrderResult result = add(OrderType::IOC, other_, MARKET - 10, 100);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {MARKET, MARKET - 10}));
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET - 1'000, 5, 1}, {MARKET - FAR, 5, 1}}));
}

TEST_P(PriceWindowTest, FillOrKill_EnoughAcrossTheBoundary_Fills) {
    restFourLevels();

    OrderResult result = add(OrderType::FOK, other_, MARKET - FAR, 20);

    EXPECT_EQ(result.trades.size(), 4u);
    expectEmptyBook(book_);
}

TEST_P(PriceWindowTest, FillOrKill_NotEnoughAcrossTheBoundary_FailsAndChangesNothing) {
    restFourLevels();
    Snapshot before = snapshotOf(book_);

    OrderResult result = add(OrderType::FOK, other_, MARKET - FAR, 21);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(PriceWindowTest, FillOrKill_LimitStopsBeforeFarLevels_Fails) {
    restFourLevels();
    Snapshot before = snapshotOf(book_);

    OrderResult result = add(OrderType::FOK, other_, MARKET - 10, 11);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(PriceWindowTest, GetDepth_AcrossWindowAndFarLevels_BestFirst) {
    const Levels expected {
        {MARKET, 1, 1}, {MARKET - 10, 2, 1}, {MARKET - 20, 3, 1},
        {MARKET - 1'000, 4, 1}, {MARKET - FAR, 5, 1}, {MARKET - 2'000'000, 6, 1}
    };
    // Not in price order, so that the order of arrival cannot be what sorts them
    for (std::size_t index : {3, 0, 5, 2, 4, 1})
        rest(side_, std::get<0>(expected[index]), std::get<1>(expected[index]));

    EXPECT_EQ(levelsOf(book_, side_), at(expected));
    EXPECT_EQ(levelsOf(book_, side_, 2), at(Levels(expected.begin(), expected.begin() + 2)));
    EXPECT_EQ(levelsOf(book_, side_, 4), at(Levels(expected.begin(), expected.begin() + 4)));
}

TEST_P(PriceWindowTest, GetDepth_FarLevelsOfBothSides_BestFirstEach) {
    for (Price price : {MARKET, MARKET - FAR, MARKET - 2 * FAR})
        rest(side_, price, 1);
    for (Price price : {MARKET + 1'000, MARKET + FAR, MARKET + 2 * FAR})
        rest(other_, price, 2);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET, 1, 1}, {MARKET - FAR, 1, 1}, {MARKET - 2 * FAR, 1, 1}}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{MARKET + 1'000, 2, 1}, {MARKET + FAR, 2, 1}, {MARKET + 2 * FAR, 2, 1}}));
}

// --------------------------
// Modify
// --------------------------
TEST_P(PriceWindowTest, Modify_FromNearToFarAndBack_LevelFollows) {
    rest(side_, MARKET + 10, 1);
    OrderID id = rest(side_, MARKET, 10);

    auto toFar = book_.modifyOrder(id, at(MARKET - FAR), 10);

    ASSERT_TRUE(toFar.has_value());
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET + 10, 1, 1}, {MARKET - FAR, 10, 1}}));
    EXPECT_EQ(book_.getOrder(toFar->orderID)->getPrice(), at(MARKET - FAR));

    auto toNear = book_.modifyOrder(toFar->orderID, at(MARKET), 10);

    ASSERT_TRUE(toNear.has_value());
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET + 10, 1, 1}, {MARKET, 10, 1}}));
    EXPECT_EQ(book_.size(), 2u);
}

TEST_P(PriceWindowTest, Modify_QuantityDownOfAFarOrder_KeepsTheID) {
    rest(side_, MARKET, 1);
    OrderID id = rest(side_, MARKET - FAR, 10);
    rest(side_, MARKET - FAR, 5);

    auto result = book_.modifyOrder(id, at(MARKET - FAR), 4);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    EXPECT_EQ(book_.getOrder(id)->getRemainingQuantity(), 4u);
    expectLevel(side_, MARKET - FAR, 9, 2);
}

// --------------------------
// One side emptied from its best
// --------------------------
TEST_P(PriceWindowTest, BestCancelled_NextBestIsFarAway_StillShownAndTraded) {
    OrderID near = rest(side_, MARKET, 5);
    OrderID far = rest(side_, MARKET - FAR, 5);
    rest(side_, MARKET - 2 * FAR, 5);

    book_.cancelOrder(near);

    expectBest(side_, far);
    OrderResult result = add(OrderType::IOC, other_, MARKET - 2 * FAR, 8);
    EXPECT_EQ(tradePrices(result.trades), at(Prices {MARKET - FAR, MARKET - 2 * FAR}));
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET - 2 * FAR, 2, 1}}));
}

TEST_P(PriceWindowTest, BestCancelled_NewOrderBelowTheFarBest_RestsBehindIt) {
    OrderID near = rest(side_, MARKET, 5);
    OrderID far = rest(side_, MARKET - FAR, 5);
    book_.cancelOrder(near);

    OrderID behind = rest(side_, MARKET - FAR - 50, 5);

    expectBest(side_, far);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET - FAR, 5, 1}, {MARKET - FAR - 50, 5, 1}}));
    EXPECT_NE(book_.getOrder(behind), nullptr);
}

TEST_P(PriceWindowTest, Aggressor_TradesAgainstAFarBestAndRestsTheRest_FarFromTheWindow) {
    OrderID near = rest(side_, MARKET, 5);
    OrderID far = rest(side_, MARKET - FAR, 5);
    book_.cancelOrder(near);

    OrderResult result = add(OrderType::GTC, other_, MARKET - FAR, 8);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(restingOf(result.trades[0]), far);
    EXPECT_EQ(result.trades[0].price, at(MARKET - FAR));
    EXPECT_EQ(result.trades[0].quantity, 5u);
    EXPECT_EQ(book_.getOrder(far), nullptr);
    expectLevel(other_, MARKET - FAR, 3, 1);
    expectBest(other_, result.orderID);
}

// --------------------------
// The window follows the market
// --------------------------
TEST_P(PriceWindowTest, PricesMarchingFar_SomeOrdersLeftBehind_DepthKeepsThem) {
    constexpr Price START = 1'000'000;
    constexpr Price STEP = 100;
    constexpr int ROUNDS = 500;
    constexpr int KEEP_EVERY = 50;

    Levels kept;
    OrderID bid {0};
    OrderID ask {0};
    Price bidPrice {0};
    for (int round {0}; round < ROUNDS; ++round) {
        if (round > 0) {
            book_.cancelOrder(ask);
            if ((round - 1) % KEEP_EVERY == 0)
                kept.insert(kept.begin(), {bidPrice, 10, 1});
            else
                book_.cancelOrder(bid);
        }

        bidPrice = START + STEP * round;
        bid = rest(side_, bidPrice, 10);
        ask = rest(other_, bidPrice + STEP / 2, 10);
        ASSERT_NE(best(side_), nullptr) << "round " << round;
        ASSERT_EQ(best(side_)->getPrice(), at(bidPrice)) << "round " << round;
    }

    Levels expected {{bidPrice, 10, 1}};
    expected.insert(expected.end(), kept.begin(), kept.end());
    EXPECT_EQ(levelsOf(book_, side_), at(expected));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{bidPrice + STEP / 2, 10, 1}}));
    EXPECT_EQ(book_.size(), expected.size() + 1);
}

TEST_P(PriceWindowTest, PricesMarchingFar_ThenTradedAgainstFromTheTop_FillsInPriceOrder) {
    for (int round {0}; round < 300; ++round)
        rest(side_, MARKET + 100 * round, 1);

    OrderResult result = add(OrderType::IOC, other_, MARKET, 300);

    ASSERT_EQ(result.trades.size(), 300u);
    Prices expected;
    for (int round {299}; round >= 0; --round)
        expected.push_back(at(MARKET + 100 * round));
    EXPECT_EQ(tradePrices(result.trades), at(expected) == expected ? expected : expected);
    expectEmptyBook(book_);
}

TEST_P(PriceWindowTest, AscendingOneTickAtATime_EveryLevelStays) {
    for (Price price {MARKET}; price < MARKET + 200; ++price)
        rest(side_, price, 1);

    EXPECT_EQ(levelsOf(book_, side_), at(ladder(MARKET, MARKET + 199)));
}

TEST_P(PriceWindowTest, DescendingOneTickAtATime_EveryLevelStays) {
    for (Price price {MARKET + 199}; price >= MARKET; --price)
        rest(side_, price, 1);

    EXPECT_EQ(levelsOf(book_, side_), at(ladder(MARKET, MARKET + 199)));
}

TEST_P(PriceWindowTest, BestJumpsFarAwayAndBack_AllLevelsStay) {
    OrderID near = rest(side_, MARKET, 10);
    OrderID jump = rest(side_, MARKET + 3'000'000, 5);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET + 3'000'000, 5, 1}, {MARKET, 10, 1}}));

    book_.cancelOrder(jump);
    expectBest(side_, near);
    OrderID again = rest(side_, MARKET + 5, 2);

    expectBest(side_, again);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{MARKET + 5, 2, 1}, {MARKET, 10, 1}}));
}

TEST_P(PriceWindowTest, OtherSideFarAway_NewBestBetween_TradesOnlyAtTheNewBest) {
    OrderID near = rest(side_, MARKET, 10);
    rest(other_, MARKET + 1'000, 10);
    rest(side_, MARKET + 500, 10);

    OrderResult result = add(OrderType::IOC, other_, MARKET + 500, 15);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {MARKET + 500}));
    expectBest(side_, near);
    expectLevel(side_, MARKET + 500, 0, 0);
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{MARKET + 1'000, 10, 1}}));
}

TEST_P(PriceWindowTest, MarketOrder_LevelsInTheWindowAndFar_SweepsTheWholeChain) {
    restFourLevels();

    OrderResult result = book_.addMarketOrder(ownerOf(other_), other_, 20);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {MARKET, MARKET - 10, MARKET - 1'000, MARKET - FAR}));
    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {5, 5, 5, 5}));
    EXPECT_TRUE(levelsOf(book_, side_).empty());
}

TEST_P(PriceWindowTest, LevelQueries_FarLevelOfOneSide_OtherSideShowsNothing) {
    rest(side_, MARKET, 4);
    rest(side_, MARKET - FAR, 7);

    for (Price price : {MARKET - FAR, MARKET}) {
        EXPECT_EQ(book_.getLevelQuantity(other_, at(price)), 0u);
        EXPECT_EQ(book_.getLevelCount(other_, at(price)), 0u);
    }
    expectLevel(side_, MARKET - FAR, 7, 1);
}

TEST_P(PriceWindowTest, FarLevelEmptied_TheOtherSideRestsThereCleanly) {
    OrderID near = rest(side_, MARKET, 3);
    OrderID far = rest(side_, MARKET - FAR, 4);
    book_.cancelOrder(far);
    book_.cancelOrder(near);

    OrderID other = rest(other_, MARKET - FAR, 6);

    expectLevel(other_, MARKET - FAR, 6, 1);
    expectLevel(side_, MARKET - FAR, 0, 0);
    expectBest(other_, other);
}

TEST_P(PriceWindowTest, BandEdges_OrdersAtBothEnds_RestShowInDepthAndSweepInOrder) {
    for (Price price : {1, 10'000'000, 9'999'999, 2})
        rest(side_, price, 3);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{10'000'000, 3, 1}, {9'999'999, 3, 1}, {2, 3, 1}, {1, 3, 1}}));

    OrderResult result = add(OrderType::IOC, other_, 1, 100);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {10'000'000, 9'999'999, 2, 1}));
    EXPECT_TRUE(levelsOf(book_, side_).empty());
}

TEST_P(PriceWindowTest, BandEdges_OneOrderAtEachEnd_BothInTheMapsAndTradable) {
    rest(side_, 1, 5);
    rest(other_, 10'000'000, 5);
    EXPECT_EQ(book_.size(), 2u);

    OrderID middle = rest(side_, 5'000'000, 2);

    expectBest(side_, middle);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{5'000'000, 2, 1}, {1, 5, 1}}));

    OrderResult first = book_.addMarketOrder(ownerOf(side_), side_, 5);
    EXPECT_EQ(tradePrices(first.trades), at(Prices {10'000'000}));

    OrderResult second = book_.addMarketOrder(ownerOf(other_), other_, 7);
    EXPECT_EQ(tradePrices(second.trades), at(Prices {5'000'000, 1}));
    expectEmptyBook(book_);
}

// --------------------------
// Windows of other sizes
// --------------------------
class NarrowBandTest : public SidedBookTest {
protected:
    NarrowBandTest() : SidedBookTest(BookConfig {1, 10, 30, SelfTradePolicy::cancelNewest, 64}) {}
};

INSTANTIATE_FOR_BOTH_SIDES(NarrowBandTest);

TEST_P(NarrowBandTest, EveryPriceOfTheBand_RestsAndTradesInOrder) {
    for (Price price {10}; price <= 30; ++price)
        rest(side_, price, 1);

    EXPECT_EQ(levelsOf(book_, side_), at(ladder(10, 30)));

    OrderResult result = add(OrderType::IOC, other_, 10, 100);

    ASSERT_EQ(result.trades.size(), 21u);
    EXPECT_EQ(result.trades.front().price, at(30));
    EXPECT_EQ(result.trades.back().price, at(10));
    expectEmptyBook(book_);
}

TEST_P(NarrowBandTest, PricesPastTheBandInsideTheWindow_AreRejected) {
    rest(side_, 20, 5);
    Snapshot before = snapshotOf(book_);

    for (Price price : {31, 41, 9})
        EXPECT_THROW(add(OrderType::GTC, side_, price, 1), std::invalid_argument);

    EXPECT_EQ(snapshotOf(book_), before);
    EXPECT_EQ(book_.getLevelQuantity(side_, at(31)), 0u);
}

class WindowOfOneTest : public SidedBookTest {
protected:
    WindowOfOneTest() : SidedBookTest(BookConfig {1, 1, 1'000, SelfTradePolicy::cancelNewest, 1}) {}
};

INSTANTIATE_FOR_BOTH_SIDES(WindowOfOneTest);

TEST_P(WindowOfOneTest, SeveralLevels_ShowInDepthAndTradeInOrder) {
    rest(side_, 500, 5);
    OrderID best = rest(side_, 700, 5);
    rest(side_, 100, 5);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{700, 5, 1}, {500, 5, 1}, {100, 5, 1}}));

    book_.cancelOrder(best);
    OrderResult result = add(OrderType::IOC, other_, 100, 8);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {500, 100}));
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 2, 1}}));
}

TEST_P(WindowOfOneTest, PricesMarching_DepthKeepsEveryLevel) {
    Levels expected;
    for (Price price {1}; price <= 1'000; price += 7) {
        rest(side_, price, 1);
        expected.insert(expected.begin(), {price, 1, 1});
    }

    EXPECT_EQ(levelsOf(book_, side_), at(expected));
}

// --------------------------
// Next to the largest price
// --------------------------
class WholeRangeWindowTest : public ::testing::Test {
protected:
    OrderBook book_ {BookConfig {1, 1, MAX_PRICE, SelfTradePolicy::cancelNewest, 64}};
};

TEST_F(WholeRangeWindowTest, Bids_NextToTheLargestPrice_ShowInDepthAndSweepInOrder) {
    for (Price price : {MAX_PRICE - 5, MAX_PRICE - 200, MAX_PRICE, MAX_PRICE - 1})
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, 3);

    EXPECT_EQ(levelsOf(book_, Side::buy), (Levels {
        {MAX_PRICE, 3, 1}, {MAX_PRICE - 1, 3, 1}, {MAX_PRICE - 5, 3, 1}, {MAX_PRICE - 200, 3, 1}}));

    OrderResult result = book_.addOrder(SELLER, OrderType::IOC, Side::sell, MAX_PRICE - 200, 100);

    EXPECT_EQ(tradePrices(result.trades), (Prices {MAX_PRICE, MAX_PRICE - 1, MAX_PRICE - 5, MAX_PRICE - 200}));
    expectEmptyBook(book_);
}

TEST_F(WholeRangeWindowTest, Asks_NextToTheLargestPrice_ShowInDepthAndSweepInOrder) {
    for (Price price : {MAX_PRICE, MAX_PRICE - 100, MAX_PRICE - 3})
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, price, 3);

    EXPECT_EQ(levelsOf(book_, Side::sell), (Levels {
        {MAX_PRICE - 100, 3, 1}, {MAX_PRICE - 3, 3, 1}, {MAX_PRICE, 3, 1}}));

    OrderResult result = book_.addOrder(BUYER, OrderType::FOK, Side::buy, MAX_PRICE, 9);

    EXPECT_EQ(tradePrices(result.trades), (Prices {MAX_PRICE - 100, MAX_PRICE - 3, MAX_PRICE}));
    expectEmptyBook(book_);
}

TEST_F(WholeRangeWindowTest, NextToTheLowestPrice_BothEndsOfTheBookWork) {
    for (Price price : {2, 1, 100, 3})
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, 3);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, MAX_PRICE - 1, 3);

    EXPECT_EQ(levelsOf(book_, Side::buy), (Levels {{100, 3, 1}, {3, 3, 1}, {2, 3, 1}, {1, 3, 1}}));
    EXPECT_EQ(levelsOf(book_, Side::sell), (Levels {{MAX_PRICE - 1, 3, 1}}));
}

TEST(TickWindowTest, FarPrices_OnTheTick_RestAndOffTheTickThrow) {
    OrderBook book {BookConfig {10, 10, 1'000'000, SelfTradePolicy::cancelNewest, 64}};
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 500'000, 5);
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 100'000, 6);
    Snapshot before = snapshotOf(book);

    EXPECT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 100'005, 1), std::invalid_argument);

    EXPECT_EQ(snapshotOf(book), before);
    EXPECT_EQ(levelsOf(book, Side::buy), (Levels {{500'000, 5, 1}, {100'000, 6, 1}}));
}
