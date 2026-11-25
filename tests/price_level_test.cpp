#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include "orderbook.hpp"
#include "types.hpp"

using namespace orderbook;

// Buys belong to BUYER and sells to SELLER, so nothing trades with itself
constexpr OwnerID BUYER  = 1;
constexpr OwnerID SELLER = 2;

constexpr Price DEFAULT_MAX = 100'000;

// A far gap is 90'000 ticks wide
constexpr Price NEAR_ASK = 10;
constexpr Price FAR_ASK  = 90'010;
constexpr Price NEAR_BID = 99'990;
constexpr Price FAR_BID  = 9'990;

using Prices = std::vector<Price>;

static Prices pricesOf(const std::vector<LevelView>& depth) {
    Prices prices;
    for (const LevelView& level : depth)
        prices.push_back(level.price);
    return prices;
}

static Quantity totalTraded(const Trades& trades) {
    Quantity total {0};
    for (const Trade& trade : trades)
        total += trade.quantity;
    return total;
}

static Prices tradePrices(const Trades& trades) {
    Prices prices;
    for (const Trade& trade : trades)
        prices.push_back(trade.price);
    return prices;
}

static void expectEmptyBook(const OrderBook& book) {
    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.size(), 0u);
    EXPECT_EQ(book.getBestBid(), nullptr);
    EXPECT_EQ(book.getBestAsk(), nullptr);
    EXPECT_TRUE(book.getDepth(Side::buy, 10).empty());
    EXPECT_TRUE(book.getDepth(Side::sell, 10).empty());
}

class PriceLevelTest : public ::testing::Test {
protected:
    OrderBook book_;
};

// --------------------------
// First and last legal price
// --------------------------
constexpr Price FIRST_PRICE = 10;
constexpr Price LAST_PRICE  = 500;

class EdgePriceTest : public ::testing::Test {
protected:
    OrderBook book_ {BookConfig {1, FIRST_PRICE, LAST_PRICE}};
};

TEST_F(EdgePriceTest, Rest_BuyAtFirstPrice_BestBidAndLevelReported) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 25).orderID;

    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), id);
    EXPECT_EQ(book_.getBestBid()->getPrice(), FIRST_PRICE);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FIRST_PRICE), 25u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, FIRST_PRICE), 1u);
}

TEST_F(EdgePriceTest, Rest_BuyAtLastPrice_BestBidAndLevelReported) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 25).orderID;

    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), id);
    EXPECT_EQ(book_.getBestBid()->getPrice(), LAST_PRICE);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, LAST_PRICE), 25u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, LAST_PRICE), 1u);
}

TEST_F(EdgePriceTest, Rest_SellAtFirstPrice_BestAskAndLevelReported) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 25).orderID;

    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), id);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), FIRST_PRICE);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FIRST_PRICE), 25u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, FIRST_PRICE), 1u);
}

TEST_F(EdgePriceTest, Rest_SellAtLastPrice_BestAskAndLevelReported) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 25).orderID;

    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), id);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), LAST_PRICE);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, LAST_PRICE), 25u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, LAST_PRICE), 1u);
}

TEST_F(EdgePriceTest, Rest_BuyAtFirstAndSellAtLastPrice_BothSidesReported) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 6);

    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getPrice(), FIRST_PRICE);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), LAST_PRICE);
    EXPECT_EQ(book_.size(), 2u);
}

TEST_F(EdgePriceTest, Cancel_BuyAtFirstPrice_BookIsEmpty) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 25).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FIRST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, FIRST_PRICE), 0u);
}

TEST_F(EdgePriceTest, Cancel_BuyAtLastPrice_BookIsEmpty) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 25).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, LAST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, LAST_PRICE), 0u);
}

TEST_F(EdgePriceTest, Cancel_SellAtFirstPrice_BookIsEmpty) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 25).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FIRST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, FIRST_PRICE), 0u);
}

TEST_F(EdgePriceTest, Cancel_SellAtLastPrice_BookIsEmpty) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 25).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, LAST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, LAST_PRICE), 0u);
}

TEST_F(EdgePriceTest, Cancel_BuyAtLastPriceWithBuyAtFirstPrice_FirstPriceBecomesBest) {
    OrderID last = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 5).orderID;
    OrderID first = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 6).orderID;

    book_.cancelOrder(last);

    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), first);
    EXPECT_EQ(book_.getDepth(Side::buy, 5).size(), 1u);
}

TEST_F(EdgePriceTest, Cancel_SellAtFirstPriceWithSellAtLastPrice_LastPriceBecomesBest) {
    OrderID first = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 5).orderID;
    OrderID last = book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 6).orderID;

    book_.cancelOrder(first);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), last);
    EXPECT_EQ(book_.getDepth(Side::sell, 5).size(), 1u);
}

TEST_F(EdgePriceTest, Match_BuyAtLastPriceTakesSellAtFirstPrice_TradesAtTheRestingPrice) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 10);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, FIRST_PRICE);
    EXPECT_EQ(result.trades[0].quantity, 10u);
    expectEmptyBook(book_);
}

TEST_F(EdgePriceTest, Match_SellAtFirstPriceTakesBuyAtLastPrice_TradesAtTheRestingPrice) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 10);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, LAST_PRICE);
    EXPECT_EQ(result.trades[0].quantity, 10u);
    expectEmptyBook(book_);
}

TEST_F(EdgePriceTest, Match_BuyAtLastPriceHitsSellAtLastPrice_Trades) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 10);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 4);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, LAST_PRICE);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, LAST_PRICE), 6u);
}

TEST_F(EdgePriceTest, Match_SellAtFirstPriceHitsBuyAtFirstPrice_Trades) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 10);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 4);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, FIRST_PRICE);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FIRST_PRICE), 6u);
}

TEST_F(EdgePriceTest, Match_BuyAtFirstPriceDoesNotCrossSellAtLastPrice_BothRest) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 10);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 10);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 2u);
}

TEST_F(EdgePriceTest, Match_SellAtLastPriceDoesNotCrossBuyAtFirstPrice_BothRest) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 10);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 10);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 2u);
}

TEST_F(EdgePriceTest, Modify_BuyFromFirstToLastPrice_LevelMoves) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 10).orderID;

    auto result = book_.modifyOrder(id, LAST_PRICE, 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FIRST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, LAST_PRICE), 10u);
    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), result->orderID);
    EXPECT_EQ(book_.getBestBid()->getPrice(), LAST_PRICE);
}

TEST_F(EdgePriceTest, Modify_BuyFromLastToFirstPrice_LevelMoves) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 10).orderID;

    auto result = book_.modifyOrder(id, FIRST_PRICE, 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, LAST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FIRST_PRICE), 10u);
    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getPrice(), FIRST_PRICE);
}

TEST_F(EdgePriceTest, Modify_SellFromFirstToLastPrice_LevelMoves) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 10).orderID;

    auto result = book_.modifyOrder(id, LAST_PRICE, 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FIRST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, LAST_PRICE), 10u);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), LAST_PRICE);
}

TEST_F(EdgePriceTest, Modify_SellFromLastToFirstPrice_LevelMoves) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 10).orderID;

    auto result = book_.modifyOrder(id, FIRST_PRICE, 10);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, LAST_PRICE), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FIRST_PRICE), 10u);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), FIRST_PRICE);
}

TEST_F(EdgePriceTest, Modify_BuyAtLastPriceQuantityDown_LevelQuantityDrops) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 50).orderID;

    auto result = book_.modifyOrder(id, LAST_PRICE, 20);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, LAST_PRICE), 20u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, LAST_PRICE), 1u);
}

TEST_F(EdgePriceTest, Modify_SellAtFirstPriceQuantityDown_LevelQuantityDrops) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 50).orderID;

    auto result = book_.modifyOrder(id, FIRST_PRICE, 20);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->orderID, id);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FIRST_PRICE), 20u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, FIRST_PRICE), 1u);
}

TEST_F(EdgePriceTest, Modify_BuyToLastPriceCrossingSellAtLastPrice_Trades) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 10);
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 10).orderID;

    auto result = book_.modifyOrder(id, LAST_PRICE, 10);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->trades.size(), 1u);
    EXPECT_EQ(result->trades[0].price, LAST_PRICE);
    expectEmptyBook(book_);
}

TEST_F(EdgePriceTest, Modify_SellToFirstPriceCrossingBuyAtFirstPrice_Trades) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 10);
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 10).orderID;

    auto result = book_.modifyOrder(id, FIRST_PRICE, 10);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->trades.size(), 1u);
    EXPECT_EQ(result->trades[0].price, FIRST_PRICE);
    expectEmptyBook(book_);
}

TEST_F(EdgePriceTest, GetDepth_BuysAtFirstAndLastPrice_LastPriceFirst) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, LAST_PRICE, 6);

    auto depth = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 2u);
    EXPECT_EQ(depth[0].price, LAST_PRICE);
    EXPECT_EQ(depth[0].quantity, 6u);
    EXPECT_EQ(depth[1].price, FIRST_PRICE);
    EXPECT_EQ(depth[1].quantity, 5u);
}

TEST_F(EdgePriceTest, GetDepth_SellsAtFirstAndLastPrice_FirstPriceFirst) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 6);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FIRST_PRICE, 5);

    auto depth = book_.getDepth(Side::sell, 5);

    ASSERT_EQ(depth.size(), 2u);
    EXPECT_EQ(depth[0].price, FIRST_PRICE);
    EXPECT_EQ(depth[0].quantity, 5u);
    EXPECT_EQ(depth[1].price, LAST_PRICE);
    EXPECT_EQ(depth[1].quantity, 6u);
}

TEST_F(EdgePriceTest, GetLevelQuantity_PricesJustOutsideTheBand_ReturnZero) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FIRST_PRICE, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, LAST_PRICE, 6);

    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FIRST_PRICE - 1), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, LAST_PRICE + 1), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, FIRST_PRICE - 1), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, LAST_PRICE + 1), 0u);
}

// --------------------------
// Band of one price
// --------------------------
constexpr Price ONLY_PRICE = 50;

class OnePriceBandTest : public ::testing::Test {
protected:
    OrderBook book_ {BookConfig {1, ONLY_PRICE, ONLY_PRICE}};
};

TEST_F(OnePriceBandTest, Rest_Buy_BestBidAtTheOnlyPrice) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10).orderID;

    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), id);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 10u);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(OnePriceBandTest, Rest_Sell_BestAskAtTheOnlyPrice) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10).orderID;

    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), id);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 10u);
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(OnePriceBandTest, Cancel_Buy_BookIsEmpty) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
}

TEST_F(OnePriceBandTest, Cancel_Sell_BookIsEmpty) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
}

TEST_F(OnePriceBandTest, Match_SellHitsRestingBuy_TradesAtTheOnlyPrice) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, ONLY_PRICE);
    expectEmptyBook(book_);
}

TEST_F(OnePriceBandTest, Match_BuyHitsRestingSell_TradesAtTheOnlyPrice) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, ONLY_PRICE);
    expectEmptyBook(book_);
}

TEST_F(OnePriceBandTest, Match_SellSmallerThanRestingBuy_RemainderStaysAsBuy) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10);

    book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 4);

    EXPECT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 6u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 0u);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(OnePriceBandTest, Match_BuyLargerThanRestingSell_RemainderRestsAsBuy) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 4);

    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10);

    EXPECT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 6u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 0u);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(OnePriceBandTest, Match_SellLargerThanRestingBuy_RemainderRestsAsSell) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 4);

    book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10);

    EXPECT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 6u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 0u);
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(OnePriceBandTest, Match_SellAcrossTwoRestingBuys_FillsTheOlderFirst) {
    OrderID older = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10).orderID;
    OrderID newer = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10).orderID;

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 15);

    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[0].bidID, older);
    EXPECT_EQ(result.trades[0].quantity, 10u);
    EXPECT_EQ(result.trades[1].bidID, newer);
    EXPECT_EQ(result.trades[1].quantity, 5u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 5u);
}

TEST_F(OnePriceBandTest, Match_BuyAcrossTwoRestingSells_FillsTheOlderFirst) {
    OrderID older = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10).orderID;
    OrderID newer = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10).orderID;

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 15);

    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[0].askID, older);
    EXPECT_EQ(result.trades[1].askID, newer);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 5u);
}

TEST_F(OnePriceBandTest, Market_SellHitsRestingBuy_TradesAtTheOnlyPrice) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 30);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, ONLY_PRICE);
    EXPECT_EQ(result.trades[0].quantity, 10u);
    expectEmptyBook(book_);
}

TEST_F(OnePriceBandTest, Market_BuyHitsRestingSell_TradesAtTheOnlyPrice) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 10);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 30);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, ONLY_PRICE);
    EXPECT_EQ(result.trades[0].quantity, 10u);
    expectEmptyBook(book_);
}

TEST_F(OnePriceBandTest, GetDepth_OneLevelOnEachSide_ReturnsIt) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 10);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 5);

    auto depth = book_.getDepth(Side::buy, 3);

    ASSERT_EQ(depth.size(), 1u);
    EXPECT_EQ(depth[0].price, ONLY_PRICE);
    EXPECT_EQ(depth[0].quantity, 15u);
    EXPECT_EQ(depth[0].count, 2u);
    EXPECT_TRUE(book_.getDepth(Side::sell, 3).empty());
}

TEST_F(OnePriceBandTest, PriceUsedByBothSidesOneAfterTheOther_EachRoundIsClean) {
    for (int round {0}; round < 20; ++round) {
        OrderID buyID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, ONLY_PRICE, 7).orderID;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 7u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 0u) << "round " << round;
        book_.cancelOrder(buyID);

        OrderID sellID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, ONLY_PRICE, 9).orderID;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, ONLY_PRICE), 9u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, ONLY_PRICE), 0u) << "round " << round;
        book_.cancelOrder(sellID);

        ASSERT_TRUE(book_.empty()) << "round " << round;
    }
}

// --------------------------
// Tick above one
// --------------------------
class TickBookTest : public ::testing::Test {
protected:
    OrderBook book_ {BookConfig {5, 10, 100}};
};

TEST_F(TickBookTest, GetDepth_BuysOnTicks_HighestPriceFirst) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 10, 1);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 60, 2);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 35, 3);

    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 10)), (Prices {60, 35, 10}));
}

TEST_F(TickBookTest, GetDepth_SellsOnTicks_LowestPriceFirst) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 1);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 45, 2);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 70, 3);

    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 10)), (Prices {45, 70, 100}));
}

TEST_F(TickBookTest, Rest_BuyAtFirstAndSellAtLastPrice_BothSidesReported) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 10, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 6);

    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getPrice(), 10);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), 100);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 10), 5u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 6u);
}

TEST_F(TickBookTest, Cancel_BuyAtLastPrice_BookIsEmpty) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 5).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 100), 0u);
}

TEST_F(TickBookTest, Cancel_SellAtFirstPrice_BookIsEmpty) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 10, 5).orderID;

    book_.cancelOrder(id);

    expectEmptyBook(book_);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 10), 0u);
}

TEST_F(TickBookTest, Match_BuySweepsSellsOnTicks_TradesAtEachRestingTick) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 15, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 20, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 25, 5);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 25, 12);

    EXPECT_EQ(tradePrices(result.trades), (Prices {15, 20, 25}));
    EXPECT_EQ(totalTraded(result.trades), 12u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 25), 3u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 15), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 20), 0u);
}

TEST_F(TickBookTest, Match_SellSweepsBuysOnTicks_TradesAtEachRestingTick) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 85, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 80, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 75, 5);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 75, 12);

    EXPECT_EQ(tradePrices(result.trades), (Prices {85, 80, 75}));
    EXPECT_EQ(totalTraded(result.trades), 12u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 75), 3u);
}

TEST_F(TickBookTest, Match_BuyOneTickBelowTheAsk_DoesNotTrade) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 50, 5);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 45, 5);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 2u);
}

TEST_F(TickBookTest, Match_SellOneTickAboveTheBid_DoesNotTrade) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 50, 5);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 55, 5);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 2u);
}

TEST_F(TickBookTest, Market_BuySweepsSellsOnTicks_TradesAtEachRestingTick) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 40, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 95, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 100);

    EXPECT_EQ(tradePrices(result.trades), (Prices {40, 95}));
    expectEmptyBook(book_);
}

TEST_F(TickBookTest, Market_SellSweepsBuysOnTicks_TradesAtEachRestingTick) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 65, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 15, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 100);

    EXPECT_EQ(tradePrices(result.trades), (Prices {65, 15}));
    expectEmptyBook(book_);
}

TEST_F(TickBookTest, Modify_BuyFromFirstToLastTick_LevelMoves) {
    OrderID id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 10, 5).orderID;

    book_.modifyOrder(id, 95, 5);

    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 10), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 95), 5u);
    EXPECT_EQ(book_.getBestBid()->getPrice(), 95);
}

TEST_F(TickBookTest, Modify_SellFromLastToFirstTick_LevelMoves) {
    OrderID id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 5).orderID;

    book_.modifyOrder(id, 15, 5);

    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 15), 5u);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), 15);
}

TEST_F(TickBookTest, GetLevelQuantity_PricesBetweenTicks_ReturnZeroForBothSides) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 15, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 25, 6);

    for (Price price : {11, 12, 13, 14, 16, 17, 19, 21, 24, 26}) {
        EXPECT_EQ(book_.getLevelQuantity(Side::buy, price), 0u) << "price " << price;
        EXPECT_EQ(book_.getLevelQuantity(Side::sell, price), 0u) << "price " << price;
        EXPECT_EQ(book_.getLevelCount(Side::buy, price), 0u) << "price " << price;
        EXPECT_EQ(book_.getLevelCount(Side::sell, price), 0u) << "price " << price;
    }
}

TEST(WideTickTest, Book_TickOfAThousand_EndsAndMiddleWork) {
    OrderBook book(BookConfig {1'000, 1'000, 1'000'000});
    book.addOrder(SELLER, OrderType::GTC, Side::sell, 1'000, 5);
    book.addOrder(SELLER, OrderType::GTC, Side::sell, 500'000, 5);
    book.addOrder(SELLER, OrderType::GTC, Side::sell, 1'000'000, 5);

    auto result = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500'000, 7);

    EXPECT_EQ(tradePrices(result.trades), (Prices {1'000, 500'000}));
    EXPECT_EQ(pricesOf(book.getDepth(Side::sell, 5)), (Prices {500'000, 1'000'000}));
    EXPECT_EQ(book.getLevelQuantity(Side::sell, 500'000), 3u);
}

// --------------------------
// Level queries off the grid
// --------------------------
// The book holds buys at 10 and 15 and sells at 95 and 100
class IllegalPriceTest : public ::testing::Test {
protected:
    OrderBook book_ {BookConfig {5, 10, 100}};

    void SetUp() override {
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, 10, 4);
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, 15, 5);
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, 95, 6);
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 7);
    }

    void expectNothingAt(Price price) {
        for (Side side : {Side::buy, Side::sell}) {
            EXPECT_EQ(book_.getLevelQuantity(side, price), 0u) << "price " << price;
            EXPECT_EQ(book_.getLevelCount(side, price), 0u) << "price " << price;
        }
    }

    void expectBookUntouched() {
        EXPECT_EQ(book_.size(), 4u);
        EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 10)), (Prices {15, 10}));
        EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 10)), (Prices {95, 100}));
    }
};

TEST_F(IllegalPriceTest, LevelQueries_PriceZero_ReturnZero) {
    expectNothingAt(0);
}

TEST_F(IllegalPriceTest, LevelQueries_NegativePrices_ReturnZero) {
    for (Price price : {-1, -5, -10, -90, -95, -100})
        expectNothingAt(price);
}

TEST_F(IllegalPriceTest, LevelQueries_PricesBelowTheBand_ReturnZero) {
    for (Price price : {9, 8, 5, 1})
        expectNothingAt(price);
}

TEST_F(IllegalPriceTest, LevelQueries_PricesAboveTheBand_ReturnZero) {
    for (Price price : {101, 105, 110, 1'000, 100'000})
        expectNothingAt(price);
}

TEST_F(IllegalPriceTest, LevelQueries_PricesOffTheTickInsideTheBand_ReturnZero) {
    for (Price price : {11, 12, 13, 14, 16, 94, 96, 97, 98, 99})
        expectNothingAt(price);
}

TEST_F(IllegalPriceTest, LevelQueries_NumericLimits_ReturnZero) {
    expectNothingAt(std::numeric_limits<Price>::max());
    expectNothingAt(std::numeric_limits<Price>::max() - 1);
    expectNothingAt(std::numeric_limits<Price>::min());
    expectNothingAt(std::numeric_limits<Price>::min() + 1);
}

TEST_F(IllegalPriceTest, LevelQueries_IllegalPrices_LeaveTheBookUntouched) {
    for (Price price : {0, -5, 5, 12, 105, std::numeric_limits<Price>::max(), std::numeric_limits<Price>::min()})
        expectNothingAt(price);

    expectBookUntouched();
}

TEST_F(IllegalPriceTest, LevelQueries_LegalPricesStillWork_AfterIllegalOnes) {
    expectNothingAt(0);
    expectNothingAt(std::numeric_limits<Price>::max());

    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 15), 5u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 7u);
}

TEST(IllegalPriceDefaultBookTest, LevelQueries_NextToTheDefaultBand_ReturnZero) {
    OrderBook book;
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 5);
    book.addOrder(SELLER, OrderType::GTC, Side::sell, DEFAULT_MAX, 6);

    for (Side side : {Side::buy, Side::sell}) {
        EXPECT_EQ(book.getLevelQuantity(side, 0), 0u);
        EXPECT_EQ(book.getLevelQuantity(side, DEFAULT_MAX + 1), 0u);
        EXPECT_EQ(book.getLevelQuantity(side, -1), 0u);
        EXPECT_EQ(book.getLevelCount(side, 0), 0u);
        EXPECT_EQ(book.getLevelCount(side, DEFAULT_MAX + 1), 0u);
    }
}

TEST(IllegalPriceEmptyBookTest, LevelQueries_IllegalPricesOnAnEmptyBook_ReturnZero) {
    OrderBook book;

    for (Price price : {0, -1, DEFAULT_MAX + 1, std::numeric_limits<Price>::max(), std::numeric_limits<Price>::min()}) {
        EXPECT_EQ(book.getLevelQuantity(Side::buy, price), 0u) << "price " << price;
        EXPECT_EQ(book.getLevelQuantity(Side::sell, price), 0u) << "price " << price;
        EXPECT_EQ(book.getLevelCount(Side::buy, price), 0u) << "price " << price;
        EXPECT_EQ(book.getLevelCount(Side::sell, price), 0u) << "price " << price;
    }
}

// --------------------------
// Other side of a price
// --------------------------
TEST_F(PriceLevelTest, LevelQueries_BuysAskedAtAPriceThatOnlyHasSells_ReturnZero) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 60, 8);

    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 60), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, 60), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 60), 8u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 60), 1u);
}

TEST_F(PriceLevelTest, LevelQueries_SellsAskedAtAPriceThatOnlyHasBuys_ReturnZero) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 60, 8);

    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 60), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 60), 0u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 60), 8u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, 60), 1u);
}

TEST_F(PriceLevelTest, LevelQueries_BuysAskedAtEveryPriceOfTheSells_ReturnZero) {
    for (Price price : {1, 50, 5'000, DEFAULT_MAX})
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, price, 3);

    for (Price price : {1, 50, 5'000, DEFAULT_MAX}) {
        EXPECT_EQ(book_.getLevelQuantity(Side::buy, price), 0u) << "price " << price;
        EXPECT_EQ(book_.getLevelCount(Side::buy, price), 0u) << "price " << price;
    }
}

TEST_F(PriceLevelTest, LevelQueries_SellsAskedAtEveryPriceOfTheBuys_ReturnZero) {
    for (Price price : {1, 50, 5'000, DEFAULT_MAX})
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, 3);

    for (Price price : {1, 50, 5'000, DEFAULT_MAX}) {
        EXPECT_EQ(book_.getLevelQuantity(Side::sell, price), 0u) << "price " << price;
        EXPECT_EQ(book_.getLevelCount(Side::sell, price), 0u) << "price " << price;
    }
}

// --------------------------
// Far gaps between levels
// --------------------------
TEST_F(PriceLevelTest, Cancel_BestAskWithNextOneFarAway_NextBecomesBestAsk) {
    OrderID near = book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 5).orderID;
    OrderID far = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7).orderID;

    book_.cancelOrder(near);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), far);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {FAR_ASK}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, NEAR_ASK), 0u);
}

TEST_F(PriceLevelTest, Cancel_BestBidWithNextOneFarAway_NextBecomesBestBid) {
    OrderID near = book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5).orderID;
    OrderID far = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7).orderID;

    book_.cancelOrder(near);

    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), far);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {FAR_BID}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, NEAR_BID), 0u);
}

TEST_F(PriceLevelTest, Cancel_FarAskOnly_BestAskStaysAndDepthKeepsOneLevel) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 5);
    OrderID far = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7).orderID;

    book_.cancelOrder(far);

    EXPECT_EQ(book_.getBestAsk()->getPrice(), NEAR_ASK);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {NEAR_ASK}));
}

TEST_F(PriceLevelTest, Cancel_FarBidOnly_BestBidStaysAndDepthKeepsOneLevel) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5);
    OrderID far = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7).orderID;

    book_.cancelOrder(far);

    EXPECT_EQ(book_.getBestBid()->getPrice(), NEAR_BID);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {NEAR_BID}));
}

TEST_F(PriceLevelTest, Fill_BestAskFullyWithNextOneFarAway_NextBecomesBestAsk) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 5);
    OrderID far = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7).orderID;

    auto result = book_.addOrder(BUYER, OrderType::IOC, Side::buy, NEAR_ASK, 5);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, NEAR_ASK);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), far);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FAR_ASK), 7u);
}

TEST_F(PriceLevelTest, Fill_BestBidFullyWithNextOneFarAway_NextBecomesBestBid) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5);
    OrderID far = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7).orderID;

    auto result = book_.addOrder(SELLER, OrderType::IOC, Side::sell, NEAR_BID, 5);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, NEAR_BID);
    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), far);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FAR_BID), 7u);
}

TEST_F(PriceLevelTest, Fill_BuyCrossesTheGap_TradesAtBothLevelsAndRestsTheRest) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 3);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 4);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_ASK, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {NEAR_ASK, FAR_ASK}));
    EXPECT_EQ(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FAR_ASK), 3u);
}

TEST_F(PriceLevelTest, Fill_SellCrossesTheGap_TradesAtBothLevelsAndRestsTheRest) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 3);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 4);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_BID, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {NEAR_BID, FAR_BID}));
    EXPECT_EQ(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FAR_BID), 3u);
}

TEST_F(PriceLevelTest, Modify_BestAskMovedBeyondTheOtherFarLevel_OtherBecomesBestAsk) {
    OrderID near = book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 5).orderID;
    OrderID far = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7).orderID;

    book_.modifyOrder(near, FAR_ASK + 10, 5);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), far);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {FAR_ASK, FAR_ASK + 10}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, NEAR_ASK), 0u);
}

TEST_F(PriceLevelTest, Modify_BestBidMovedBeyondTheOtherFarLevel_OtherBecomesBestBid) {
    OrderID near = book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5).orderID;
    OrderID far = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7).orderID;

    book_.modifyOrder(near, FAR_BID - 10, 5);

    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), far);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {FAR_BID, FAR_BID - 10}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, NEAR_BID), 0u);
}

TEST_F(PriceLevelTest, Modify_FarAskMovedToTheFrontOfTheBook_BecomesBestAsk) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 5);
    OrderID far = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7).orderID;

    auto result = book_.modifyOrder(far, 5, 7);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getBestAsk()->getID(), result->orderID);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {5, NEAR_ASK}));
}

TEST_F(PriceLevelTest, Modify_FarBidMovedToTheFrontOfTheBook_BecomesBestBid) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5);
    OrderID far = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7).orderID;

    auto result = book_.modifyOrder(far, DEFAULT_MAX, 7);

    ASSERT_TRUE(result.has_value());
    ASSERT_NE(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getBestBid()->getID(), result->orderID);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {DEFAULT_MAX, NEAR_BID}));
}

TEST_F(PriceLevelTest, Cancel_BestAskThenNewOrdersAtNearerPrices_BestAskFollows) {
    OrderID near = book_.addOrder(SELLER, OrderType::GTC, Side::sell, NEAR_ASK, 5).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7);
    book_.cancelOrder(near);

    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 50'000, 1);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), 50'000);

    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 20, 1);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), 20);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {20, 50'000, FAR_ASK}));
}

TEST_F(PriceLevelTest, Cancel_BestBidThenNewOrdersAtNearerPrices_BestBidFollows) {
    OrderID near = book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7);
    book_.cancelOrder(near);

    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 50'000, 1);
    EXPECT_EQ(book_.getBestBid()->getPrice(), 50'000);

    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99'000, 1);
    EXPECT_EQ(book_.getBestBid()->getPrice(), 99'000);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {99'000, 50'000, FAR_BID}));
}

// --------------------------
// getDepth with far gaps
// --------------------------
namespace {

struct ExpectedLevel {
    Price price;
    Quantity quantity;
    std::size_t count;
};

std::vector<ExpectedLevel> farLevels(Side side) {
    std::vector<ExpectedLevel> levels {{1, 5, 1}, {50'000, 12, 2}, {DEFAULT_MAX, 20, 1}};
    if (side == Side::buy) std::reverse(levels.begin(), levels.end());
    return levels;
}

} // namespace

class FarDepthTest : public ::testing::Test {
protected:
    OrderBook book_;

    void restFarLevels(Side side) {
        OwnerID owner = (side == Side::buy) ? BUYER : SELLER;
        book_.addOrder(owner, OrderType::GTC, side, DEFAULT_MAX, 20);
        book_.addOrder(owner, OrderType::GTC, side, 50'000, 7);
        book_.addOrder(owner, OrderType::GTC, side, 1, 5);
        book_.addOrder(owner, OrderType::GTC, side, 50'000, 5);
    }

    void expectDepth(Side side, std::size_t levels, std::size_t expectedSize) {
        auto depth = book_.getDepth(side, levels);
        std::vector<ExpectedLevel> expected = farLevels(side);

        ASSERT_EQ(depth.size(), expectedSize);
        for (std::size_t i {0}; i < expectedSize; ++i) {
            EXPECT_EQ(depth[i].price, expected[i].price) << "level " << i;
            EXPECT_EQ(depth[i].quantity, expected[i].quantity) << "level " << i;
            EXPECT_EQ(depth[i].count, expected[i].count) << "level " << i;
        }
    }
};

TEST_F(FarDepthTest, GetDepth_SellsWithFarGaps_AllLevelsBestFirst) {
    restFarLevels(Side::sell);

    expectDepth(Side::sell, 3, 3);
}

TEST_F(FarDepthTest, GetDepth_BuysWithFarGaps_AllLevelsBestFirst) {
    restFarLevels(Side::buy);

    expectDepth(Side::buy, 3, 3);
}

TEST_F(FarDepthTest, GetDepth_SellsZeroLevels_ReturnsNothing) {
    restFarLevels(Side::sell);

    expectDepth(Side::sell, 0, 0);
}

TEST_F(FarDepthTest, GetDepth_BuysZeroLevels_ReturnsNothing) {
    restFarLevels(Side::buy);

    expectDepth(Side::buy, 0, 0);
}

TEST_F(FarDepthTest, GetDepth_SellsOneLevel_ReturnsOnlyTheBest) {
    restFarLevels(Side::sell);

    expectDepth(Side::sell, 1, 1);
}

TEST_F(FarDepthTest, GetDepth_BuysOneLevel_ReturnsOnlyTheBest) {
    restFarLevels(Side::buy);

    expectDepth(Side::buy, 1, 1);
}

TEST_F(FarDepthTest, GetDepth_SellsTwoOfThreeLevels_ReturnsTheTwoBest) {
    restFarLevels(Side::sell);

    expectDepth(Side::sell, 2, 2);
}

TEST_F(FarDepthTest, GetDepth_BuysTwoOfThreeLevels_ReturnsTheTwoBest) {
    restFarLevels(Side::buy);

    expectDepth(Side::buy, 2, 2);
}

TEST_F(FarDepthTest, GetDepth_SellsMoreLevelsThanThereAre_ReturnsAllOfThem) {
    restFarLevels(Side::sell);

    expectDepth(Side::sell, 4, 3);
    expectDepth(Side::sell, 1'000'000, 3);
}

TEST_F(FarDepthTest, GetDepth_BuysMoreLevelsThanThereAre_ReturnsAllOfThem) {
    restFarLevels(Side::buy);

    expectDepth(Side::buy, 4, 3);
    expectDepth(Side::buy, 1'000'000, 3);
}

TEST_F(FarDepthTest, GetDepth_MiddleSellLevelCancelled_GapBecomesWider) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 5);
    OrderID middle = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 50'000, 7).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, DEFAULT_MAX, 9);

    book_.cancelOrder(middle);

    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 10)), (Prices {1, DEFAULT_MAX}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 50'000), 0u);
}

TEST_F(FarDepthTest, GetDepth_MiddleBuyLevelCancelled_GapBecomesWider) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, DEFAULT_MAX, 9);
    OrderID middle = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 50'000, 7).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 5);

    book_.cancelOrder(middle);

    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 10)), (Prices {DEFAULT_MAX, 1}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 50'000), 0u);
}

TEST_F(FarDepthTest, GetDepth_EmptySide_ReturnsNothingForAnyCount) {
    restFarLevels(Side::sell);

    EXPECT_TRUE(book_.getDepth(Side::buy, 0).empty());
    EXPECT_TRUE(book_.getDepth(Side::buy, 1).empty());
    EXPECT_TRUE(book_.getDepth(Side::buy, 100).empty());
}

// --------------------------
// Band of a million prices
// --------------------------
constexpr Price MILLION = 1'000'000;

class WideBandTest : public ::testing::Test {
protected:
    OrderBook book_ {BookConfig {1, 1, MILLION}};
};

TEST_F(WideBandTest, Rest_SellsAtFirstMiddleAndLastPrice_DepthListsThemInOrder) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, MILLION, 3);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 1);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 500'000, 2);

    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {1, 500'000, MILLION}));
    EXPECT_EQ(book_.getBestAsk()->getPrice(), 1);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, MILLION), 3u);
}

TEST_F(WideBandTest, Rest_BuysAtFirstMiddleAndLastPrice_DepthListsThemInOrder) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 1);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, MILLION, 3);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 500'000, 2);

    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {MILLION, 500'000, 1}));
    EXPECT_EQ(book_.getBestBid()->getPrice(), MILLION);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 1), 1u);
}

TEST_F(WideBandTest, Cancel_BestSellsOneByOne_NextBestIsFoundAcrossTheBand) {
    OrderID first = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 1).orderID;
    OrderID middle = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 500'000, 2).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, MILLION, 3);

    book_.cancelOrder(first);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), 500'000);

    book_.cancelOrder(middle);
    EXPECT_EQ(book_.getBestAsk()->getPrice(), MILLION);
}

TEST_F(WideBandTest, Cancel_BestBuysOneByOne_NextBestIsFoundAcrossTheBand) {
    OrderID first = book_.addOrder(BUYER, OrderType::GTC, Side::buy, MILLION, 1).orderID;
    OrderID middle = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 500'000, 2).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 3);

    book_.cancelOrder(first);
    EXPECT_EQ(book_.getBestBid()->getPrice(), 500'000);

    book_.cancelOrder(middle);
    EXPECT_EQ(book_.getBestBid()->getPrice(), 1);
}

TEST_F(WideBandTest, Match_BuyAtLastPriceSweepsTheWholeBand_TradesInPriceOrder) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, MILLION, 3);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 1);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 500'000, 2);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, MILLION, 6);

    EXPECT_EQ(tradePrices(result.trades), (Prices {1, 500'000, MILLION}));
    expectEmptyBook(book_);
}

TEST_F(WideBandTest, Match_SellAtFirstPriceSweepsTheWholeBand_TradesInPriceOrder) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 3);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, MILLION, 1);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 500'000, 2);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 6);

    EXPECT_EQ(tradePrices(result.trades), (Prices {MILLION, 500'000, 1}));
    expectEmptyBook(book_);
}

TEST_F(WideBandTest, Rest_ThousandSellsSpreadOverTheBand_CancelledOneByOneInOrderOfPrice) {
    constexpr Price STEP = 997;
    std::vector<OrderID> ids;
    for (Price price {1}; price <= MILLION; price += STEP)
        ids.push_back(book_.addOrder(SELLER, OrderType::GTC, Side::sell, price, 1).orderID);

    ASSERT_EQ(book_.getDepth(Side::sell, ids.size() + 1).size(), ids.size());
    Price expectedPrice {1};
    for (OrderID id : ids) {
        ASSERT_NE(book_.getBestAsk(), nullptr);
        ASSERT_EQ(book_.getBestAsk()->getPrice(), expectedPrice);
        book_.cancelOrder(id);
        expectedPrice += STEP;
    }

    expectEmptyBook(book_);
}

TEST_F(WideBandTest, Rest_ThousandBuysSpreadOverTheBand_CancelledOneByOneInOrderOfPrice) {
    constexpr Price STEP = 997;
    std::vector<OrderID> ids;
    std::vector<Price> prices;
    for (Price price {MILLION}; price >= 1; price -= STEP) {
        prices.push_back(price);
        ids.push_back(book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, 1).orderID);
    }

    ASSERT_EQ(book_.getDepth(Side::buy, ids.size() + 1).size(), ids.size());
    for (std::size_t i {0}; i < ids.size(); ++i) {
        ASSERT_NE(book_.getBestBid(), nullptr);
        ASSERT_EQ(book_.getBestBid()->getPrice(), prices[i]);
        book_.cancelOrder(ids[i]);
    }

    expectEmptyBook(book_);
}

// --------------------------
// Side emptied and reused
// --------------------------
TEST_F(PriceLevelTest, SellSide_EmptiedByCancelsManyTimes_EveryRoundStartsClean) {
    for (Price round {0}; round < 200; ++round) {
        OrderID low = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100 + round, 5).orderID;
        OrderID middle = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 40'000 + round, 6).orderID;
        OrderID high = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99'000 - round, 7).orderID;
        ASSERT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {100 + round, 40'000 + round, 99'000 - round}));

        book_.cancelOrder(middle);
        book_.cancelOrder(low);
        ASSERT_EQ(book_.getBestAsk()->getPrice(), 99'000 - round) << "round " << round;
        book_.cancelOrder(high);

        ASSERT_TRUE(book_.empty()) << "round " << round;
        ASSERT_EQ(book_.getBestAsk(), nullptr) << "round " << round;
        ASSERT_TRUE(book_.getDepth(Side::sell, 5).empty()) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100 + round), 0u) << "round " << round;
    }
}

TEST_F(PriceLevelTest, BuySide_EmptiedByCancelsManyTimes_EveryRoundStartsClean) {
    for (Price round {0}; round < 200; ++round) {
        OrderID high = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99'000 - round, 5).orderID;
        OrderID middle = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 40'000 + round, 6).orderID;
        OrderID low = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100 + round, 7).orderID;
        ASSERT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {99'000 - round, 40'000 + round, 100 + round}));

        book_.cancelOrder(middle);
        book_.cancelOrder(high);
        ASSERT_EQ(book_.getBestBid()->getPrice(), 100 + round) << "round " << round;
        book_.cancelOrder(low);

        ASSERT_TRUE(book_.empty()) << "round " << round;
        ASSERT_EQ(book_.getBestBid(), nullptr) << "round " << round;
        ASSERT_TRUE(book_.getDepth(Side::buy, 5).empty()) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100 + round), 0u) << "round " << round;
    }
}

TEST_F(PriceLevelTest, SellSide_EmptiedByFillsManyTimes_EveryRoundStartsClean) {
    for (Price round {0}; round < 200; ++round) {
        Price price = 1 + round * 450;
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, price, 5);

        auto result = book_.addOrder(BUYER, OrderType::IOC, Side::buy, price, 5);

        ASSERT_EQ(tradePrices(result.trades), (Prices {price})) << "round " << round;
        ASSERT_TRUE(book_.empty()) << "round " << round;
        ASSERT_EQ(book_.getBestAsk(), nullptr) << "round " << round;
        ASSERT_TRUE(book_.getDepth(Side::sell, 5).empty()) << "round " << round;
    }
}

TEST_F(PriceLevelTest, BuySide_EmptiedByFillsManyTimes_EveryRoundStartsClean) {
    for (Price round {0}; round < 200; ++round) {
        Price price = DEFAULT_MAX - round * 450;
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, 5);

        auto result = book_.addOrder(SELLER, OrderType::IOC, Side::sell, price, 5);

        ASSERT_EQ(tradePrices(result.trades), (Prices {price})) << "round " << round;
        ASSERT_TRUE(book_.empty()) << "round " << round;
        ASSERT_EQ(book_.getBestBid(), nullptr) << "round " << round;
        ASSERT_TRUE(book_.getDepth(Side::buy, 5).empty()) << "round " << round;
    }
}

TEST_F(PriceLevelTest, BothSides_EmptiedInDifferentOrdersManyTimes_EveryRoundStartsClean) {
    for (int round {0}; round < 100; ++round) {
        OrderID buy = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 30'000, 5).orderID;
        OrderID sell = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 70'000, 6).orderID;
        ASSERT_EQ(book_.size(), 2u);

        if (round % 2 == 0) {
            book_.cancelOrder(buy);
            book_.cancelOrder(sell);
        } else {
            book_.cancelOrder(sell);
            book_.cancelOrder(buy);
        }

        ASSERT_TRUE(book_.empty()) << "round " << round;
        ASSERT_EQ(book_.getBestBid(), nullptr) << "round " << round;
        ASSERT_EQ(book_.getBestAsk(), nullptr) << "round " << round;
    }
}

// --------------------------
// A price that changes side
// --------------------------
TEST_F(PriceLevelTest, Price_RestedAndCancelledBySides_OnlyTheCurrentSideShowsIt) {
    for (int round {0}; round < 100; ++round) {
        OrderID buy = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 50'000, 7).orderID;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, 50'000), 7u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, 50'000), 0u) << "round " << round;
        ASSERT_EQ(book_.getBestAsk(), nullptr) << "round " << round;
        book_.cancelOrder(buy);

        OrderID sell = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 50'000, 9).orderID;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, 50'000), 9u) << "round " << round;
        ASSERT_EQ(book_.getLevelCount(Side::sell, 50'000), 1u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, 50'000), 0u) << "round " << round;
        ASSERT_EQ(book_.getBestBid(), nullptr) << "round " << round;
        book_.cancelOrder(sell);
    }

    EXPECT_TRUE(book_.empty());
}

TEST_F(PriceLevelTest, Price_RemainderRestsOnTheOtherSideEachTime_LevelFollowsTheRemainder) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 10);

    for (int round {0}; round < 50; ++round) {
        auto sell = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 500, 15);
        ASSERT_EQ(totalTraded(sell.trades), 10u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, 500), 5u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, 500), 0u) << "round " << round;
        ASSERT_EQ(book_.getBestBid(), nullptr) << "round " << round;

        auto buy = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 15);
        ASSERT_EQ(totalTraded(buy.trades), 5u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::buy, 500), 10u) << "round " << round;
        ASSERT_EQ(book_.getLevelQuantity(Side::sell, 500), 0u) << "round " << round;
        ASSERT_EQ(book_.getBestAsk(), nullptr) << "round " << round;
    }
}

// --------------------------
// Market orders
// --------------------------
TEST_F(PriceLevelTest, Market_BuyLargerThanTheAskSide_SweepsItAllAndNothingRests) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 10, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 20, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 30, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 100);

    EXPECT_EQ(tradePrices(result.trades), (Prices {10, 20, 30}));
    EXPECT_EQ(totalTraded(result.trades), 15u);
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_SellLargerThanTheBidSide_SweepsItAllAndNothingRests) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 30, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 20, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 10, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 100);

    EXPECT_EQ(tradePrices(result.trades), (Prices {30, 20, 10}));
    EXPECT_EQ(totalTraded(result.trades), 15u);
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_BuyEqualToTheAskSide_EmptiesItExactly) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 10, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {10, FAR_ASK}));
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_SellEqualToTheBidSide_EmptiesItExactly) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, NEAR_BID, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {NEAR_BID, FAR_BID}));
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_BuyAcrossFarGaps_TradesAtEachLevelAndLeavesTheRest) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 10, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, DEFAULT_MAX - 10, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 12);

    EXPECT_EQ(tradePrices(result.trades), (Prices {10, FAR_ASK, DEFAULT_MAX - 10}));
    EXPECT_EQ(totalTraded(result.trades), 12u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {DEFAULT_MAX - 10}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, DEFAULT_MAX - 10), 3u);
}

TEST_F(PriceLevelTest, Market_SellAcrossFarGaps_TradesAtEachLevelAndLeavesTheRest) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, DEFAULT_MAX - 10, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 10, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 12);

    EXPECT_EQ(tradePrices(result.trades), (Prices {DEFAULT_MAX - 10, FAR_BID, 10}));
    EXPECT_EQ(totalTraded(result.trades), 12u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {10}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 10), 3u);
}

TEST_F(PriceLevelTest, Market_BuyWithOnlyAskAtTheLastPrice_TradesThere) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, DEFAULT_MAX, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 3);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, DEFAULT_MAX);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, DEFAULT_MAX), 2u);
}

TEST_F(PriceLevelTest, Market_SellWithOnlyBidAtTheFirstPrice_TradesThere) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 3);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, 1);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 1), 2u);
}

TEST_F(PriceLevelTest, Market_BuyWithAsksAtBothEdges_TradesAtBothEdges) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 1, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, DEFAULT_MAX, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {1, DEFAULT_MAX}));
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_SellWithBidsAtBothEdges_TradesAtBothEdges) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 1, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, DEFAULT_MAX, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {DEFAULT_MAX, 1}));
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_BuyAgainstOnlyOwnAsksCancelNewest_NothingTradesAndAsksStay) {
    book_.addOrder(BUYER, OrderType::GTC, Side::sell, NEAR_ASK, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::sell, FAR_ASK, 5);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 8);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 2u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {NEAR_ASK, FAR_ASK}));
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(PriceLevelTest, Market_SellAgainstOnlyOwnBidsCancelNewest_NothingTradesAndBidsStay) {
    book_.addOrder(SELLER, OrderType::GTC, Side::buy, NEAR_BID, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::buy, FAR_BID, 5);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 8);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 2u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {NEAR_BID, FAR_BID}));
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(PriceLevelTest, Market_BuyAgainstOnlyOwnAsksCancelOldest_AllAsksCancelledAcrossTheGap) {
    book_.setSelfTradePolicy(BUYER, SelfTradePolicy::cancelOldest);
    OrderID near = book_.addOrder(BUYER, OrderType::GTC, Side::sell, NEAR_ASK, 5).orderID;
    OrderID far = book_.addOrder(BUYER, OrderType::GTC, Side::sell, FAR_ASK, 5).orderID;

    auto result = book_.addMarketOrder(BUYER, Side::buy, 8);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result.cancelledOrders, (std::vector<OrderID> {near, far}));
    expectEmptyBook(book_);
}

TEST_F(PriceLevelTest, Market_SellAgainstOnlyOwnBidsCancelOldest_AllBidsCancelledAcrossTheGap) {
    book_.setSelfTradePolicy(SELLER, SelfTradePolicy::cancelOldest);
    OrderID near = book_.addOrder(SELLER, OrderType::GTC, Side::buy, NEAR_BID, 5).orderID;
    OrderID far = book_.addOrder(SELLER, OrderType::GTC, Side::buy, FAR_BID, 5).orderID;

    auto result = book_.addMarketOrder(SELLER, Side::sell, 8);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result.cancelledOrders, (std::vector<OrderID> {near, far}));
    expectEmptyBook(book_);
}

// --------------------------
// cancelOldest across far levels
// --------------------------
TEST_F(PriceLevelTest, CancelOldest_OwnAskIsTheLastOfItsLevel_MarketBuyContinuesToTheFarLevel) {
    book_.setSelfTradePolicy(BUYER, SelfTradePolicy::cancelOldest);
    OrderID own = book_.addOrder(BUYER, OrderType::GTC, Side::sell, NEAR_ASK, 5).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 7);

    auto result = book_.addMarketOrder(BUYER, Side::buy, 5);

    EXPECT_EQ(result.cancelledOrders, (std::vector<OrderID> {own}));
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, FAR_ASK);
    EXPECT_EQ(result.trades[0].quantity, 5u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, NEAR_ASK), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::sell, NEAR_ASK), 0u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {FAR_ASK}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FAR_ASK), 2u);
}

TEST_F(PriceLevelTest, CancelOldest_OwnBidIsTheLastOfItsLevel_MarketSellContinuesToTheFarLevel) {
    book_.setSelfTradePolicy(SELLER, SelfTradePolicy::cancelOldest);
    OrderID own = book_.addOrder(SELLER, OrderType::GTC, Side::buy, NEAR_BID, 5).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 7);

    auto result = book_.addMarketOrder(SELLER, Side::sell, 5);

    EXPECT_EQ(result.cancelledOrders, (std::vector<OrderID> {own}));
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, FAR_BID);
    EXPECT_EQ(result.trades[0].quantity, 5u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, NEAR_BID), 0u);
    EXPECT_EQ(book_.getLevelCount(Side::buy, NEAR_BID), 0u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {FAR_BID}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FAR_BID), 2u);
}

TEST_F(PriceLevelTest, CancelOldest_GTCBuyCancelsOwnNearAskAndRestsAfterTheFarLevel) {
    book_.setSelfTradePolicy(BUYER, SelfTradePolicy::cancelOldest);
    book_.addOrder(BUYER, OrderType::GTC, Side::sell, NEAR_ASK, 5);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_ASK, 4);

    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_ASK, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, FAR_ASK);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, FAR_ASK), 6u);
}

TEST_F(PriceLevelTest, CancelOldest_GTCSellCancelsOwnNearBidAndRestsAfterTheFarLevel) {
    book_.setSelfTradePolicy(SELLER, SelfTradePolicy::cancelOldest);
    book_.addOrder(SELLER, OrderType::GTC, Side::buy, NEAR_BID, 5);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, FAR_BID, 4);

    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, FAR_BID, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, FAR_BID);
    EXPECT_EQ(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, FAR_BID), 6u);
}

// --------------------------
// Fill or kill over far levels
// --------------------------
class FarFillOrKillTest : public ::testing::Test {
protected:
    OrderBook book_;

    void restAsks() {
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, 10, 3);
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, 45'000, 3);
        book_.addOrder(SELLER, OrderType::GTC, Side::sell, 90'010, 4);
    }

    void restBids() {
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99'990, 3);
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, 55'000, 3);
        book_.addOrder(BUYER, OrderType::GTC, Side::buy, 9'990, 4);
    }
};

TEST_F(FarFillOrKillTest, Buy_ExactlyEnoughOverFarLevels_FillsAcrossAllOfThem) {
    restAsks();

    auto result = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 90'010, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {10, 45'000, 90'010}));
    EXPECT_EQ(totalTraded(result.trades), 10u);
    expectEmptyBook(book_);
}

TEST_F(FarFillOrKillTest, Sell_ExactlyEnoughOverFarLevels_FillsAcrossAllOfThem) {
    restBids();

    auto result = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 9'990, 10);

    EXPECT_EQ(tradePrices(result.trades), (Prices {99'990, 55'000, 9'990}));
    EXPECT_EQ(totalTraded(result.trades), 10u);
    expectEmptyBook(book_);
}

TEST_F(FarFillOrKillTest, Buy_OneShortOverFarLevels_RejectedAndBookUntouched) {
    restAsks();

    auto result = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 90'010, 11);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 3u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {10, 45'000, 90'010}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 90'010), 4u);
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(FarFillOrKillTest, Sell_OneShortOverFarLevels_RejectedAndBookUntouched) {
    restBids();

    auto result = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 9'990, 11);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 3u);
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {99'990, 55'000, 9'990}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 9'990), 4u);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(FarFillOrKillTest, Buy_LimitStopsBeforeTheFarthestLevel_Rejected) {
    restAsks();

    auto result = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 50'000, 10);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 3u);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 90'010), 4u);
}

TEST_F(FarFillOrKillTest, Sell_LimitStopsBeforeTheFarthestLevel_Rejected) {
    restBids();

    auto result = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 50'000, 10);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.size(), 3u);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 9'990), 4u);
}

TEST_F(FarFillOrKillTest, Buy_FillsTheFirstTwoLevelsOnly_LeavesTheFarthestUntouched) {
    restAsks();

    auto result = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 50'000, 6);

    EXPECT_EQ(tradePrices(result.trades), (Prices {10, 45'000}));
    EXPECT_EQ(pricesOf(book_.getDepth(Side::sell, 5)), (Prices {90'010}));
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 90'010), 4u);
}

TEST_F(FarFillOrKillTest, Sell_FillsTheFirstTwoLevelsOnly_LeavesTheFarthestUntouched) {
    restBids();

    auto result = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 50'000, 6);

    EXPECT_EQ(tradePrices(result.trades), (Prices {99'990, 55'000}));
    EXPECT_EQ(pricesOf(book_.getDepth(Side::buy, 5)), (Prices {9'990}));
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 9'990), 4u);
}

// --------------------------
// Random run against a reference
// --------------------------
namespace {

struct ReferenceOrder {
    OrderID id;
    OwnerID owner;
    Quantity quantity;
};

struct ReferenceResult {
    OrderID id;
    Trades trades;
};

struct ReferenceInfo {
    Side side;
    Price price;
    Quantity quantity;
};

// Price-time priority written the plain way, to compare the engine with
class ReferenceBook {
private:
    struct Location {
        Side side;
        Price price;
    };

    using Queue = std::deque<ReferenceOrder>;

    std::map<Price, Queue, std::greater<Price>> bids_;
    std::map<Price, Queue, std::less<Price>> asks_;
    std::unordered_map<OrderID, Location> locations_;
    OrderID nextID_ {1};

    static bool crossesLimit(Side incoming, Price limit, Price resting) {
        return (incoming == Side::buy) ? resting <= limit : resting >= limit;
    }

    template<typename MapType>
    static Quantity availableAgainst(const MapType& opposite, Side incoming, Price limit) {
        Quantity total {0};
        for (const auto& [price, queue] : opposite) {
            if (!crossesLimit(incoming, limit, price)) break;
            for (const ReferenceOrder& order : queue)
                total += order.quantity;
        }
        return total;
    }

    template<typename MapType>
    void takeFrom(MapType& opposite, OrderID id, OwnerID owner, Side incoming, Price limit,
                  Quantity& remaining, Trades& trades) {
        while (remaining > 0 && !opposite.empty()) {
            auto level = opposite.begin();
            if (!crossesLimit(incoming, limit, level->first)) break;

            ReferenceOrder& resting = level->second.front();
            Quantity quantity = std::min(remaining, resting.quantity);
            bool incomingIsBuy = incoming == Side::buy;
            trades.push_back({incomingIsBuy ? id : resting.id, incomingIsBuy ? resting.id : id,
                              level->first, quantity,
                              incomingIsBuy ? owner : resting.owner, incomingIsBuy ? resting.owner : owner});

            remaining -= quantity;
            resting.quantity -= quantity;
            if (resting.quantity == 0) {
                locations_.erase(resting.id);
                level->second.pop_front();
            }
            if (level->second.empty()) opposite.erase(level);
        }
    }

    ReferenceResult submit(OwnerID owner, OrderType type, Side side, Price limit, bool canRest, Quantity quantity) {
        OrderID id = nextID_++;
        Trades trades;

        bool isBuy = side == Side::buy;
        if (type == OrderType::FOK) {
            Quantity available = isBuy ? availableAgainst(asks_, side, limit) : availableAgainst(bids_, side, limit);
            if (available < quantity) return {id, trades};
        }

        Quantity remaining = quantity;
        if (isBuy) takeFrom(asks_, id, owner, side, limit, remaining, trades);
        else takeFrom(bids_, id, owner, side, limit, remaining, trades);

        if (remaining > 0 && canRest && type == OrderType::GTC) {
            if (isBuy) bids_[limit].push_back({id, owner, remaining});
            else asks_[limit].push_back({id, owner, remaining});
            locations_[id] = {side, limit};
        }
        return {id, trades};
    }

    template<typename MapType>
    static Queue* queueAt(MapType& side, Price price) {
        auto it = side.find(price);
        return (it == side.end()) ? nullptr : &it->second;
    }

    Queue* queueOf(const Location& location) {
        return (location.side == Side::buy) ? queueAt(bids_, location.price) : queueAt(asks_, location.price);
    }

    template<typename MapType>
    static std::vector<LevelView> depthOf(const MapType& side) {
        std::vector<LevelView> depth;
        for (const auto& [price, queue] : side) {
            Quantity quantity {0};
            for (const ReferenceOrder& order : queue)
                quantity += order.quantity;
            depth.push_back({price, quantity, queue.size()});
        }
        return depth;
    }

public:
    ReferenceResult add(OwnerID owner, OrderType type, Side side, Price price, Quantity quantity) {
        return submit(owner, type, side, price, true, quantity);
    }

    ReferenceResult addMarket(OwnerID owner, Side side, Quantity quantity) {
        Price limit = (side == Side::buy) ? std::numeric_limits<Price>::max() : std::numeric_limits<Price>::min();
        return submit(owner, OrderType::IOC, side, limit, false, quantity);
    }

    void cancel(OrderID id) {
        auto found = locations_.find(id);
        if (found == locations_.end()) return;

        Location location = found->second;
        Queue* queue = queueOf(location);
        queue->erase(std::find_if(queue->begin(), queue->end(),
                                  [&](const ReferenceOrder& order) { return order.id == id; }));
        if (queue->empty()) {
            if (location.side == Side::buy) bids_.erase(location.price);
            else asks_.erase(location.price);
        }
        locations_.erase(found);
    }

    std::optional<ReferenceInfo> describe(OrderID id) {
        auto found = locations_.find(id);
        if (found == locations_.end()) return std::nullopt;

        Queue* queue = queueOf(found->second);
        auto order = std::find_if(queue->begin(), queue->end(),
                                  [&](const ReferenceOrder& candidate) { return candidate.id == id; });
        return ReferenceInfo {found->second.side, found->second.price, order->quantity};
    }

    std::optional<ReferenceResult> modify(OrderID id, Price newPrice, Quantity newQuantity) {
        std::optional<ReferenceInfo> info = describe(id);
        if (!info) return std::nullopt;

        if (newPrice == info->price && newQuantity == info->quantity) return ReferenceResult {id, {}};
        if (newPrice == info->price && newQuantity < info->quantity) {
            Queue* queue = queueOf(locations_.at(id));
            std::find_if(queue->begin(), queue->end(),
                         [&](const ReferenceOrder& order) { return order.id == id; })->quantity = newQuantity;
            return ReferenceResult {id, {}};
        }

        OwnerID owner = findOwner(id);
        cancel(id);
        return add(owner, OrderType::GTC, info->side, newPrice, newQuantity);
    }

    OwnerID findOwner(OrderID id) {
        Queue* queue = queueOf(locations_.at(id));
        return std::find_if(queue->begin(), queue->end(),
                            [&](const ReferenceOrder& order) { return order.id == id; })->owner;
    }

    const ReferenceOrder* best(Side side) const {
        if (side == Side::buy) return bids_.empty() ? nullptr : &bids_.begin()->second.front();
        return asks_.empty() ? nullptr : &asks_.begin()->second.front();
    }

    std::vector<LevelView> depth(Side side) const {
        return (side == Side::buy) ? depthOf(bids_) : depthOf(asks_);
    }

    std::size_t size() const {
        return locations_.size();
    }

    std::vector<OrderID> liveIDs() const {
        std::vector<OrderID> ids;
        for (const auto& [price, queue] : bids_)
            for (const ReferenceOrder& order : queue)
                ids.push_back(order.id);
        for (const auto& [price, queue] : asks_)
            for (const ReferenceOrder& order : queue)
                ids.push_back(order.id);
        return ids;
    }

    OrderID nextID() const {
        return nextID_;
    }
};

bool sameTrades(const Trades& actual, const Trades& expected) {
    if (actual.size() != expected.size()) return false;
    for (std::size_t i {0}; i < actual.size(); ++i) {
        const Trade& a = actual[i];
        const Trade& e = expected[i];
        if (a.bidID != e.bidID || a.askID != e.askID || a.price != e.price ||
            a.quantity != e.quantity || a.bidOwner != e.bidOwner || a.askOwner != e.askOwner)
            return false;
    }
    return true;
}

bool sameDepth(const std::vector<LevelView>& actual, const std::vector<LevelView>& expected) {
    if (actual.size() != expected.size()) return false;
    for (std::size_t i {0}; i < actual.size(); ++i)
        if (actual[i].price != expected[i].price || actual[i].quantity != expected[i].quantity ||
            actual[i].count != expected[i].count)
            return false;
    return true;
}

// What the run reached, to check it hit the interesting cases
struct RunCounts {
    std::size_t trades {0};
    std::size_t emptyBooks {0};
    std::size_t rejectedFillOrKills {0};
    std::size_t movedOrders {0};
};

// A narrow cluster mid-band plus far outliers, a few crossing the whole band
class PriceGenerator {
private:
    Price tickSize_;
    Price lowTick_;
    Price highTick_;
    Price middleTick_;
    Price clusterTicks_;
    Price gapTicks_;
    std::mt19937& generator_;

    Price atTick(Price tick) const {
        return tick * tickSize_;
    }

    Price inTicks(Price from, Price to) {
        return atTick(std::uniform_int_distribution<Price>(std::max(from, lowTick_), std::min(to, highTick_))(generator_));
    }

public:
    PriceGenerator(const BookConfig& config, Price clusterTicks, Price gapTicks, std::mt19937& generator)
        : tickSize_ {config.tickSize}, lowTick_ {config.minPrice / config.tickSize},
          highTick_ {config.maxPrice / config.tickSize}, middleTick_ {(lowTick_ + highTick_) / 2},
          clusterTicks_ {clusterTicks}, gapTicks_ {gapTicks}, generator_ {generator}
    {}

    Price pick(Side side) {
        int roll = std::uniform_int_distribution<int>(0, 99)(generator_);
        if (roll < 70) return inTicks(middleTick_ - clusterTicks_, middleTick_ + clusterTicks_);
        if (roll < 95 && side == Side::buy) return inTicks(lowTick_, middleTick_ - gapTicks_);
        if (roll < 95) return inTicks(middleTick_ + gapTicks_, highTick_);
        return inTicks(lowTick_, highTick_);
    }

    Price pickAny() {
        return inTicks(lowTick_, highTick_);
    }
};

void runAgainstReference(const BookConfig& config, Price clusterTicks, Price gapTicks, unsigned seed, RunCounts& counts) {
    constexpr std::size_t STEPS = 20'000;

    OrderBook book(config);
    ReferenceBook reference;
    std::mt19937 generator(seed);
    PriceGenerator prices(config, clusterTicks, gapTicks, generator);
    std::uniform_int_distribution<int> percent(0, 99);
    std::uniform_int_distribution<Quantity> smallQuantity(1, 30);
    std::uniform_int_distribution<Quantity> marketQuantity(1, 60);
    OwnerID nextOwner {1};

    auto randomSide = [&]() { return (percent(generator) < 50) ? Side::buy : Side::sell; };

    auto pickID = [&]() {
        std::vector<OrderID> live = reference.liveIDs();
        if (!live.empty() && percent(generator) < 85)
            return live[std::uniform_int_distribution<std::size_t>(0, live.size() - 1)(generator)];
        return static_cast<OrderID>(std::uniform_int_distribution<OrderID>(1, reference.nextID() + 5)(generator));
    };

    for (std::size_t step {0}; step < STEPS; ++step) {
        int roll = percent(generator);
        Price touched {config.minPrice};

        if (roll < 55) {
            OrderType type = (roll < 35) ? OrderType::GTC : (roll < 47) ? OrderType::IOC : OrderType::FOK;
            Side side = randomSide();
            Price price = prices.pick(side);
            Quantity quantity = smallQuantity(generator);
            OwnerID owner = nextOwner++;

            ReferenceResult expected = reference.add(owner, type, side, price, quantity);
            OrderResult actual = book.addOrder(owner, type, side, price, quantity);

            ASSERT_EQ(actual.orderID, expected.id) << "step " << step;
            ASSERT_TRUE(sameTrades(actual.trades, expected.trades)) << "step " << step;
            ASSERT_EQ(actual.status, OrderStatus::ok) << "step " << step;
            if (type == OrderType::FOK && expected.trades.empty()) ++counts.rejectedFillOrKills;
            counts.trades += expected.trades.size();
            touched = price;
        } else if (roll < 65) {
            Side side = randomSide();
            Quantity quantity = marketQuantity(generator);
            OwnerID owner = nextOwner++;

            ReferenceResult expected = reference.addMarket(owner, side, quantity);
            OrderResult actual = book.addMarketOrder(owner, side, quantity);

            ASSERT_EQ(actual.orderID, expected.id) << "step " << step;
            ASSERT_TRUE(sameTrades(actual.trades, expected.trades)) << "step " << step;
            counts.trades += expected.trades.size();
        } else if (roll < 82) {
            OrderID id = pickID();
            std::optional<ReferenceInfo> info = reference.describe(id);
            if (info) touched = info->price;

            reference.cancel(id);
            book.cancelOrder(id);
        } else {
            OrderID id = pickID();
            std::optional<ReferenceInfo> info = reference.describe(id);
            Price price = (info && percent(generator) < 30) ? info->price : prices.pickAny();
            Quantity quantity = smallQuantity(generator);
            touched = price;

            std::optional<ReferenceResult> expected = reference.modify(id, price, quantity);
            std::optional<OrderResult> actual = book.modifyOrder(id, price, quantity);

            ASSERT_EQ(actual.has_value(), expected.has_value()) << "step " << step;
            if (expected) {
                ASSERT_EQ(actual->orderID, expected->id) << "step " << step;
                ASSERT_TRUE(sameTrades(actual->trades, expected->trades)) << "step " << step;
                counts.trades += expected->trades.size();
                if (expected->id != id) ++counts.movedOrders;
            }
        }

        ASSERT_EQ(book.size(), reference.size()) << "step " << step;
        ASSERT_EQ(book.empty(), reference.size() == 0) << "step " << step;
        for (Side side : {Side::buy, Side::sell}) {
            const Order* actualBest = (side == Side::buy) ? book.getBestBid() : book.getBestAsk();
            const ReferenceOrder* expectedBest = reference.best(side);
            ASSERT_EQ(actualBest == nullptr, expectedBest == nullptr) << "step " << step;
            if (actualBest != nullptr) {
                ASSERT_EQ(actualBest->getID(), expectedBest->id) << "step " << step;
                ASSERT_EQ(actualBest->getRemainingQuantity(), expectedBest->quantity) << "step " << step;
                ASSERT_EQ(actualBest->getOwnerID(), expectedBest->owner) << "step " << step;
            }

            std::vector<LevelView> expectedDepth = reference.depth(side);
            ASSERT_TRUE(sameDepth(book.getDepth(side, std::numeric_limits<std::size_t>::max()), expectedDepth))
                << "step " << step;

            std::size_t fewLevels = std::min<std::size_t>(3, expectedDepth.size());
            std::vector<LevelView> expectedFew(expectedDepth.begin(), expectedDepth.begin() + static_cast<std::ptrdiff_t>(fewLevels));
            ASSERT_TRUE(sameDepth(book.getDepth(side, 3), expectedFew)) << "step " << step;

            auto level = std::find_if(expectedDepth.begin(), expectedDepth.end(),
                                      [&](const LevelView& view) { return view.price == touched; });
            Quantity expectedQuantity = (level == expectedDepth.end()) ? 0 : level->quantity;
            std::size_t expectedCount = (level == expectedDepth.end()) ? 0 : level->count;
            ASSERT_EQ(book.getLevelQuantity(side, touched), expectedQuantity) << "step " << step;
            ASSERT_EQ(book.getLevelCount(side, touched), expectedCount) << "step " << step;
        }

        if (reference.size() == 0) ++counts.emptyBooks;
    }
}

} // namespace

TEST(PriceLevelRandomTest, DefaultBandWithClusterAndFarOutliers_MatchesTheReferenceBook) {
    RunCounts counts;

    runAgainstReference(BookConfig {}, 15, 400, 12345, counts);

    EXPECT_GT(counts.trades, 1'000u);
    EXPECT_GT(counts.rejectedFillOrKills, 50u);
    EXPECT_GT(counts.movedOrders, 100u);
}

TEST(PriceLevelRandomTest, SmallBandWithATickOfFive_MatchesTheReferenceBook) {
    RunCounts counts;

    runAgainstReference(BookConfig {5, 5, 500}, 4, 10, 777, counts);

    EXPECT_GT(counts.trades, 1'000u);
    EXPECT_GT(counts.emptyBooks, 10u);
    EXPECT_GT(counts.movedOrders, 100u);
}

TEST(PriceLevelRandomTest, BandOfAMillionPrices_MatchesTheReferenceBook) {
    RunCounts counts;

    runAgainstReference(BookConfig {1, 1, MILLION}, 10, 100'000, 2024, counts);

    EXPECT_GT(counts.trades, 500u);
    EXPECT_GT(counts.rejectedFillOrKills, 20u);
}
