#include <gtest/gtest.h>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

constexpr Price MAX_PRICE = std::numeric_limits<Price>::max();
constexpr Price LOWEST_PRICE = std::numeric_limits<Price>::min();

// --------------------------
// First and last price
// --------------------------
constexpr Price FIRST = 10;
constexpr Price LAST  = 500;

class EdgePriceTest : public SidedBookTest {
protected:
    EdgePriceTest() : SidedBookTest(BookConfig {1, FIRST, LAST}) {}
};

INSTANTIATE_FOR_BOTH_SIDES(EdgePriceTest);

TEST_P(EdgePriceTest, RestAndCancel_EachEdge_LevelComesAndGoes) {
    for (Price price : {FIRST, LAST}) {
        OrderID id = rest(side_, price, 25);

        expectBest(side_, id);
        EXPECT_EQ(best(side_)->getPrice(), at(price));
        expectLevel(side_, price, 25, 1);

        book_.cancelOrder(id);

        expectLevel(side_, price, 0, 0);
        expectEmptyBook(book_);
    }
}

TEST_P(EdgePriceTest, RestAtBothEdges_LastIsBestAndFirstTakesOver) {
    OrderID first = rest(side_, FIRST, 5);
    OrderID last = rest(side_, LAST, 6);

    expectBest(side_, last);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{LAST, 6, 1}, {FIRST, 5, 1}}));

    book_.cancelOrder(last);

    expectBest(side_, first);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{FIRST, 5, 1}}));
}

TEST_P(EdgePriceTest, OneSideAtEachEdge_DoNotCross) {
    rest(other_, LAST, 10);

    OrderResult result = add(OrderType::GTC, side_, FIRST, 10);

    EXPECT_TRUE(result.trades.empty());
    expectLevel(side_, FIRST, 10, 1);
    expectLevel(other_, LAST, 10, 1);
}

TEST_P(EdgePriceTest, Match_EachPairOfEdgesThatCross_TradesAtTheRestingPrice) {
    const std::vector<std::pair<Price, Price>> crossing {{FIRST, FIRST}, {LAST, LAST}, {FIRST, LAST}};
    for (auto [resting, incoming] : crossing) {
        OrderID id = rest(other_, resting, 10);

        OrderResult result = add(OrderType::GTC, side_, incoming, 4);
        ASSERT_EQ(result.trades.size(), 1u);
        expectTrade(result.trades[0], result.orderID, id, resting, 4);
        expectLevel(other_, resting, 6, 1);

        add(OrderType::GTC, side_, incoming, 6);
        expectEmptyBook(book_);
    }
}

TEST_P(EdgePriceTest, Modify_FromEdgeToEdge_LevelMoves) {
    const std::vector<std::pair<Price, Price>> moves {{FIRST, LAST}, {LAST, FIRST}};
    for (auto [from, to] : moves) {
        OrderID id = rest(side_, from, 10);

        auto result = book_.modifyOrder(id, at(to), 10);

        ASSERT_TRUE(result.has_value());
        expectBest(side_, result->orderID);
        expectLevel(side_, from, 0, 0);
        expectLevel(side_, to, 10, 1);
        book_.cancelOrder(result->orderID);
    }
}

TEST_P(EdgePriceTest, Modify_QuantityDownAtEachEdge_KeepsTheID) {
    for (Price price : {FIRST, LAST}) {
        OrderID id = rest(side_, price, 50);

        auto result = book_.modifyOrder(id, at(price), 20);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->orderID, id);
        expectLevel(side_, price, 20, 1);
    }
}

TEST_P(EdgePriceTest, Modify_ToTheEdgeCrossesTheOtherSide_Trades) {
    OrderID resting = rest(other_, LAST, 10);
    OrderID id = rest(side_, FIRST, 10);

    auto result = book_.modifyOrder(id, at(LAST), 10);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->trades.size(), 1u);
    expectTrade(result->trades[0], result->orderID, resting, LAST, 10);
    expectEmptyBook(book_);
}

TEST_P(EdgePriceTest, LevelQueries_JustOutsideTheBand_ReturnZero) {
    rest(side_, FIRST, 5);
    rest(other_, LAST, 6);

    for (Price price : {FIRST - 1, LAST + 1}) {
        expectLevel(side_, price, 0, 0);
        expectLevel(other_, price, 0, 0);
    }
}

// --------------------------
// Band of one price
// --------------------------
constexpr Price ONLY = 50;

class OnePriceBandTest : public SidedBookTest {
protected:
    OnePriceBandTest() : SidedBookTest(BookConfig {1, ONLY, ONLY}) {}
};

INSTANTIATE_FOR_BOTH_SIDES(OnePriceBandTest);

TEST_P(OnePriceBandTest, RestAndCancel_LevelComesAndGoes) {
    OrderID id = rest(side_, ONLY, 10);

    expectBest(side_, id);
    expectLevel(side_, ONLY, 10, 1);
    EXPECT_EQ(levelsOf(book_, side_), (Levels {{ONLY, 10, 1}}));
    EXPECT_EQ(best(other_), nullptr);
    EXPECT_TRUE(levelsOf(book_, other_).empty());

    book_.cancelOrder(id);

    expectEmptyBook(book_);
}

TEST_P(OnePriceBandTest, SmallerThanTheResting_RestingKeepsTheRest) {
    OrderID resting = rest(other_, ONLY, 10);

    OrderResult result = add(OrderType::GTC, side_, ONLY, 4);

    ASSERT_EQ(result.trades.size(), 1u);
    expectTrade(result.trades[0], result.orderID, resting, ONLY, 4);
    expectLevel(other_, ONLY, 6, 1);
    expectLevel(side_, ONLY, 0, 0);
    EXPECT_EQ(best(side_), nullptr);
}

TEST_P(OnePriceBandTest, SameQuantity_BothConsumed) {
    rest(other_, ONLY, 10);

    EXPECT_EQ(add(OrderType::GTC, side_, ONLY, 10).trades.size(), 1u);

    expectEmptyBook(book_);
}

// The level empties and changes side within one call
TEST_P(OnePriceBandTest, LargerThanTheResting_RemainderRestsOnTheOtherSide) {
    rest(other_, ONLY, 4);

    OrderID id = rest(side_, ONLY, 10);

    expectBest(side_, id);
    expectLevel(side_, ONLY, 6, 1);
    expectLevel(other_, ONLY, 0, 0);
    EXPECT_EQ(best(other_), nullptr);
}

TEST_P(OnePriceBandTest, AcrossTwoOrders_OlderFilledFirst) {
    OrderID older = rest(other_, ONLY, 10);
    OrderID newer = rest(other_, ONLY, 10);

    OrderResult result = add(OrderType::GTC, side_, ONLY, 15);

    ASSERT_EQ(result.trades.size(), 2u);
    expectTrade(result.trades[0], result.orderID, older, ONLY, 10);
    expectTrade(result.trades[1], result.orderID, newer, ONLY, 5);
    expectLevel(other_, ONLY, 5, 1);
}

TEST_P(OnePriceBandTest, MarketOrder_TradesAtTheOnlyPrice) {
    rest(other_, ONLY, 10);

    Trades trades = book_.addMarketOrder(ownerOf(side_), side_, 30).trades;

    EXPECT_EQ(tradePrices(trades), (Prices {ONLY}));
    EXPECT_EQ(tradeQuantities(trades), (Quantities {10}));
    expectEmptyBook(book_);
}

TEST_P(OnePriceBandTest, PricesNextToIt_AreRejected) {
    for (Price price : {ONLY - 1, ONLY + 1})
        EXPECT_THROW(add(OrderType::GTC, side_, price, 10), std::invalid_argument) << "price " << price;
}

// --------------------------
// Tick above one
// --------------------------
class TickTest : public SidedBookTest {
protected:
    TickTest() : SidedBookTest(BookConfig {5, 10, 100}) {}
};

INSTANTIATE_FOR_BOTH_SIDES(TickTest);

TEST_P(TickTest, Depth_OnlyThePricesWithOrdersBestFirst) {
    rest(side_, 10, 1);
    rest(side_, 60, 2);
    rest(side_, 35, 3);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{60, 2, 1}, {35, 3, 1}, {10, 1, 1}}));
}

TEST_P(TickTest, BothEdges_RestAndCancel) {
    OrderID id = rest(side_, 10, 5);
    OrderID otherID = rest(other_, 100, 6);

    expectBest(side_, id);
    expectBest(other_, otherID);
    EXPECT_EQ(best(side_)->getPrice(), at(10));
    EXPECT_EQ(best(other_)->getPrice(), at(100));

    book_.cancelOrder(id);
    book_.cancelOrder(otherID);

    expectEmptyBook(book_);
}

TEST_P(TickTest, Sweep_TradesAtEachTick) {
    for (Price price : {15, 20, 25})
        rest(other_, price, 5);

    Trades trades = add(OrderType::GTC, side_, 25, 12).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {15, 20, 25}));
    EXPECT_EQ(tradeQuantities(trades), (Quantities {5, 5, 2}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{25, 3, 1}}));
}

TEST_P(TickTest, LimitOneTickShort_BothRest) {
    rest(other_, 50, 5);

    EXPECT_TRUE(add(OrderType::GTC, side_, 45, 5).trades.empty());

    EXPECT_EQ(book_.size(), 2u);
}

TEST_P(TickTest, MarketOrder_SweepsEveryTick) {
    rest(other_, 40, 5);
    rest(other_, 100, 5);

    Trades trades = book_.addMarketOrder(ownerOf(side_), side_, 100).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {40, 100}));
    expectEmptyBook(book_);
}

TEST_P(TickTest, Modify_AcrossTheBand_LevelMoves) {
    OrderID id = rest(side_, 10, 5);

    auto result = book_.modifyOrder(id, at(95), 5);

    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result->orderID, id);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{95, 5, 1}}));
}

TEST_P(TickTest, TickOfAThousand_EndsAndMiddleWork) {
    OrderBook book(BookConfig {1'000, 1'000, 1'000'000});
    for (Price price : {1'000, 500'000, 1'000'000})
        book.addOrder(SELLER, OrderType::GTC, Side::sell, price, 5);

    Trades trades = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500'000, 7).trades;

    EXPECT_EQ(tradePrices(trades), (Prices {1'000, 500'000}));
    EXPECT_EQ(levelsOf(book, Side::sell), (Levels {{500'000, 3, 1}, {1'000'000, 5, 1}}));
}

// --------------------------
// Band up to the largest price
// --------------------------
TEST(LargestPriceTest, BandEndsAtTheLargestPrice_BothEdgesWork) {
    OrderBook book(BookConfig {1, MAX_PRICE - 99, MAX_PRICE});
    book.addOrder(SELLER, OrderType::GTC, Side::sell, MAX_PRICE, 5);
    book.addOrder(BUYER, OrderType::GTC, Side::buy, MAX_PRICE - 99, 6);

    EXPECT_EQ(levelsOf(book, Side::sell), (Levels {{MAX_PRICE, 5, 1}}));
    EXPECT_EQ(levelsOf(book, Side::buy), (Levels {{MAX_PRICE - 99, 6, 1}}));
    EXPECT_EQ(tradePrices(book.addMarketOrder(BUYER, Side::buy, 5).trades), (Prices {MAX_PRICE}));
    EXPECT_EQ(tradePrices(book.addMarketOrder(SELLER, Side::sell, 6).trades), (Prices {MAX_PRICE - 99}));
    EXPECT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, MAX_PRICE - 100, 1), std::invalid_argument);
    EXPECT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, LOWEST_PRICE, 1), std::invalid_argument);
}

// --------------------------
// Prices the book cannot hold
// --------------------------
// Orders at the first and last two prices
TEST(IllegalPriceTest, LevelQueries_ReturnZeroForBothSides) {
    OrderBook book(BookConfig {5, 10, 100});
    for (Price price : {10, 15})
        book.addOrder(BUYER, OrderType::GTC, Side::buy, price, 5);
    for (Price price : {95, 100})
        book.addOrder(SELLER, OrderType::GTC, Side::sell, price, 6);

    const Prices illegal {
        0, -1, -5, -10, -90, -95, -100, LOWEST_PRICE, LOWEST_PRICE + 1, // zero and negative
        1, 5, 8, 9,                                                     // below the band
        101, 105, 110, 1'000, 100'000, MAX_PRICE - 1, MAX_PRICE,        // above it
        11, 12, 13, 14, 16, 94, 96, 97, 98, 99                          // off the tick
    };
    for (Price price : illegal) {
        for (Side side : {Side::buy, Side::sell}) {
            EXPECT_EQ(book.getLevelQuantity(side, price), 0u) << "price " << price;
            EXPECT_EQ(book.getLevelCount(side, price), 0u) << "price " << price;
        }
    }

    EXPECT_EQ(book.getLevelQuantity(Side::buy, 15), 5u);
    EXPECT_EQ(book.getLevelQuantity(Side::sell, 100), 6u);
}

TEST(IllegalPriceTest, LevelQueries_EmptyDefaultBook_ReturnZero) {
    OrderBook book;

    for (Price price : {0, -1, 100'001, MAX_PRICE, LOWEST_PRICE}) {
        for (Side side : {Side::buy, Side::sell}) {
            EXPECT_EQ(book.getLevelQuantity(side, price), 0u) << "price " << price;
            EXPECT_EQ(book.getLevelCount(side, price), 0u) << "price " << price;
        }
    }
}

// --------------------------
// Finding the next best price
// --------------------------
// Price 1 is position 0. The bitmap has a bit per position and a word per 64 of
// them, and its summary a word per 4096. Both edges survive the mirroring.
constexpr Price BAND = 4 * 4'096;

class NextBestTest : public SidedBookTest {
protected:
    NextBestTest() : SidedBookTest(BookConfig {1, 1, BAND}) {}

    // The best position and the one behind it
    std::vector<std::pair<Price, Price>> pairs() const {
        return {
            {1, 0},                  // same word
            {63, 0},                 // first and last bit of a word
            {64, 63},                // next word
            {64, 0},                 // first bit of two words
            {127, 0},                // last bit of the next word
            {640, 63},               // empty words in between
            {4'095, 0},              // last word of a summary word
            {4'096, 4'095},          // next summary word
            {4'096, 0},              // first bit of two summary words
            {8'192, 4'095},          // an empty summary word in between
            {BAND - 1, 4'095},       // two of them
            {BAND - 1, 0}            // the whole band
        };
    }

    // Removes the best order, of 10 units at that price
    template<typename Remove>
    void checkNextBest(Remove remove) {
        for (auto [near, far] : pairs()) {
            SCOPED_TRACE(::testing::Message() << "positions " << near << " and " << far);
            OrderID nearID = rest(side_, near + 1, 10);
            OrderID farID = rest(side_, far + 1, 20);
            expectBest(side_, nearID);

            remove(nearID, near + 1);

            expectBest(side_, farID);
            EXPECT_EQ(levelsOf(book_, side_), at(Levels {{far + 1, 20, 1}}));
            expectLevel(side_, near + 1, 0, 0);

            book_.cancelOrder(farID);
            expectEmptyBook(book_);
        }
    }
};

INSTANTIATE_FOR_BOTH_SIDES(NextBestTest);

TEST_P(NextBestTest, BestCancelled) {
    checkNextBest([&](OrderID id, Price) { book_.cancelOrder(id); });
}

TEST_P(NextBestTest, BestFilledByALimitOrder) {
    checkNextBest([&](OrderID, Price price) {
        EXPECT_EQ(add(OrderType::IOC, other_, price, 10).trades.size(), 1u);
    });
}

TEST_P(NextBestTest, BestFilledByAFillOrKill) {
    checkNextBest([&](OrderID, Price price) {
        EXPECT_EQ(add(OrderType::FOK, other_, price, 10).trades.size(), 1u);
    });
}

TEST_P(NextBestTest, BestFilledByAMarketOrder) {
    checkNextBest([&](OrderID, Price) {
        EXPECT_EQ(book_.addMarketOrder(ownerOf(other_), other_, 10).trades.size(), 1u);
    });
}

TEST_P(NextBestTest, BestCancelledAsASelfTrade) {
    book_.setSelfTradePolicy(ownerOf(side_), SelfTradePolicy::cancelOldest);

    checkNextBest([&](OrderID id, Price price) {
        OrderResult result = book_.addOrder(ownerOf(side_), OrderType::IOC, other_, at(price), 10);
        EXPECT_EQ(result.cancelledOrders, (IDs {id}));
    });
}

TEST_P(NextBestTest, BestMovedAwayByAModify) {
    for (auto [near, far] : pairs()) {
        if (far == 0) continue; // no worse price to move to
        OrderID nearID = rest(side_, near + 1, 10);
        OrderID farID = rest(side_, far + 1, 20);

        auto moved = book_.modifyOrder(nearID, at(1), 10);

        ASSERT_TRUE(moved.has_value());
        expectBest(side_, farID);
        EXPECT_EQ(levelsOf(book_, side_), at(Levels {{far + 1, 20, 1}, {1, 10, 1}}));
        book_.cancelOrder(farID);
        book_.cancelOrder(moved->orderID);
    }
}

TEST_P(NextBestTest, WorseLevelCancelled_BestStays) {
    for (auto [near, far] : pairs()) {
        OrderID nearID = rest(side_, near + 1, 10);
        OrderID farID = rest(side_, far + 1, 20);

        book_.cancelOrder(farID);

        expectBest(side_, nearID);
        EXPECT_EQ(levelsOf(book_, side_), at(Levels {{near + 1, 10, 1}}));
        book_.cancelOrder(nearID);
    }
}

TEST_P(NextBestTest, NewOrdersBetweenTheTwo_BestFollows) {
    OrderID nearID = rest(side_, BAND, 5);
    rest(side_, 1, 7);
    book_.cancelOrder(nearID);

    expectBest(side_, rest(side_, BAND / 2, 1));
    expectBest(side_, rest(side_, BAND - 10, 1));
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{BAND - 10, 1, 1}, {BAND / 2, 1, 1}, {1, 7, 1}}));
}

TEST_P(NextBestTest, EveryPriceOfTheBand_DrainedBestFirst) {
    for (Price price {1}; price <= BAND; ++price)
        rest(side_, price, 1);

    Trades trades = book_.addMarketOrder(ownerOf(other_), other_, BAND).trades;

    ASSERT_EQ(trades.size(), static_cast<std::size_t>(BAND));
    for (Price i {0}; i < BAND; ++i)
        ASSERT_EQ(trades[static_cast<std::size_t>(i)].price, at(BAND - i));
    expectEmptyBook(book_);
}

// --------------------------
// Band of a million prices
// --------------------------
constexpr Price MILLION = 1'000'000;

class WideBandTest : public SidedBookTest {
protected:
    WideBandTest() : SidedBookTest(BookConfig {1, 1, MILLION}) {}
};

INSTANTIATE_FOR_BOTH_SIDES(WideBandTest);

TEST_P(WideBandTest, FirstMiddleAndLastPrice_SweptBestFirst) {
    rest(other_, MILLION, 3);
    rest(other_, 1, 1);
    rest(other_, 500'000, 2);
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{1, 1, 1}, {500'000, 2, 1}, {MILLION, 3, 1}}));

    Trades trades = add(OrderType::GTC, side_, MILLION, 6).trades;

    EXPECT_EQ(tradePrices(trades), at(Prices {1, 500'000, MILLION}));
    expectEmptyBook(book_);
}

TEST_P(WideBandTest, AThousandLevels_CancelledBestFirst) {
    constexpr Price STEP = 997;
    Prices prices;
    IDs ids;
    for (Price price {MILLION}; price >= 1; price -= STEP) {
        prices.push_back(price);
        ids.push_back(rest(side_, price, 1));
    }
    ASSERT_EQ(book_.getDepth(side_, ALL_LEVELS).size(), ids.size());

    for (std::size_t i {0}; i < ids.size(); ++i) {
        ASSERT_NE(best(side_), nullptr);
        ASSERT_EQ(best(side_)->getPrice(), at(prices[i]));
        book_.cancelOrder(ids[i]);
    }

    expectEmptyBook(book_);
}

// --------------------------
// Levels that empty and change side
// --------------------------
class ReusedLevelTest : public SidedBookTest {};

INSTANTIATE_FOR_BOTH_SIDES(ReusedLevelTest);

TEST_P(ReusedLevelTest, CancelledByOneSideThenUsedByTheOther_OnlyTheCurrentSideShowsIt) {
    for (int round {0}; round < 100; ++round) {
        SCOPED_TRACE(round);
        OrderID id = rest(side_, 50'000, 7);
        expectLevel(side_, 50'000, 7, 1);
        expectLevel(other_, 50'000, 0, 0);
        EXPECT_EQ(best(other_), nullptr);
        book_.cancelOrder(id);

        OrderID otherID = rest(other_, 50'000, 9);
        expectLevel(other_, 50'000, 9, 1);
        expectLevel(side_, 50'000, 0, 0);
        EXPECT_EQ(best(side_), nullptr);
        book_.cancelOrder(otherID);

        expectEmptyBook(book_);
    }
}

TEST_P(ReusedLevelTest, RemainderRestsOnTheOtherSideEachTime_LevelFollowsIt) {
    rest(side_, 500, 10);

    for (int round {0}; round < 50; ++round) {
        SCOPED_TRACE(round);
        EXPECT_EQ(tradeQuantities(add(OrderType::GTC, other_, 500, 15).trades), (Quantities {10}));
        expectLevel(other_, 500, 5, 1);
        expectLevel(side_, 500, 0, 0);
        EXPECT_EQ(best(side_), nullptr);

        EXPECT_EQ(tradeQuantities(add(OrderType::GTC, side_, 500, 15).trades), (Quantities {5}));
        expectLevel(side_, 500, 10, 1);
        expectLevel(other_, 500, 0, 0);
        EXPECT_EQ(best(other_), nullptr);
    }
}

TEST_P(ReusedLevelTest, SideEmptiedByCancels_EveryRoundStartsClean) {
    for (Price round {0}; round < 200; ++round) {
        SCOPED_TRACE(round);
        OrderID high = rest(side_, 99'000 - round, 5);
        OrderID middle = rest(side_, 40'000 + round, 6);
        OrderID low = rest(side_, 100 + round, 7);
        EXPECT_EQ(levelsOf(book_, side_),
                  at(Levels {{99'000 - round, 5, 1}, {40'000 + round, 6, 1}, {100 + round, 7, 1}}));

        book_.cancelOrder(middle);
        book_.cancelOrder(high);
        expectBest(side_, low);
        book_.cancelOrder(low);

        expectEmptyBook(book_);
        expectLevel(side_, 100 + round, 0, 0);
    }
}

TEST_P(ReusedLevelTest, SideEmptiedByFills_EveryRoundStartsClean) {
    for (Price round {0}; round < 200; ++round) {
        SCOPED_TRACE(round);
        Price price = 1 + round * 450;
        rest(side_, price, 5);

        EXPECT_EQ(tradePrices(add(OrderType::IOC, other_, price, 5).trades), at(Prices {price}));

        expectEmptyBook(book_);
    }
}

TEST_P(ReusedLevelTest, BothSidesEmptiedOneAfterTheOther_EveryRoundStartsClean) {
    for (int round {0}; round < 100; ++round) {
        OrderID id = rest(side_, 30'000, 5);
        OrderID otherID = rest(other_, 70'000, 6);

        book_.cancelOrder(id);
        expectBest(other_, otherID);
        book_.cancelOrder(otherID);

        expectEmptyBook(book_);
    }
}
