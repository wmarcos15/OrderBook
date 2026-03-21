#include <gtest/gtest.h>
#include "orderbook.hpp"
#include "types.hpp"

class MatchingTest : public ::testing::Test {
protected:
    OrderBook book_;
};

// --------------------------
// Matching Logic
// --------------------------

// --------------------------
// GTC
// --------------------------
TEST_F(MatchingTest, GTC_FullFIllBidIsAgressor_BothConsumed) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, GTC_FullFIllAskIsAgressor_BothConsumed) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, GTC_FullFIllBidIsAgressor_OneTrade) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(trades.size(), 1);
}

TEST_F(MatchingTest, GTC_FullFIllAskIsAgressor_OneTrade) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    ASSERT_EQ(trades.size(), 1);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_AskConsumed) {
    book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_CorrectRestingQty) {
    book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    ASSERT_EQ(book_.getBestBid()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_OneTradeCorrectQty) {
    book_.addOrder(OrderType::GTC, Side::sell, 101, 125);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_OneTradeCorrectPrice) {
    book_.addOrder(OrderType::GTC, Side::sell, 101, 125);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 150);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 99);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_BidConsumed) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_CorrectRestingQty) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    ASSERT_EQ(book_.getBestAsk()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_OneTradeCorrectQty) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_OneTradeCorrectPrice) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 100);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_BidConsumed) {
    book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_CorrectRestingQty) {
    book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    ASSERT_EQ(book_.getBestAsk()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_OneTradeCorrectQty) {
    book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_OneTradeCorrectPrice) {
    book_.addOrder(OrderType::GTC, Side::sell, 99, 150);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 99);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_AskConsumed) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_CorrectRestingQty) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    ASSERT_EQ(book_.getBestBid()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_OneTradeCorrectQty) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_OneTradeCorrectPrice) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 150);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 95, 100);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 99);
}

TEST_F(MatchingTest, GTC_BuySweeepsThreeAskLevels_ThreeTradesGenerated) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 102, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 102, 150);
    ASSERT_EQ(trades.size(), 3);
}

TEST_F(MatchingTest, GTC_BuySweepsThreeAskLevels_AllAsksConsumed) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 102, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 102, 150);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, GTC_BuySweepsThreeAskLevels_CorrectTradeQuantities) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 101, 75);
    book_.addOrder(OrderType::GTC, Side::sell, 102, 100);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 102, 225);
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(trades[1].quantity, 75);
    ASSERT_EQ(trades[2].quantity, 100);
}

TEST_F(MatchingTest, GTC_SellSweepsThreeBidLevels_ThreeTradesGenerated) {
    book_.addOrder(OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 100, 150);
    ASSERT_EQ(trades.size(), 3);
}

TEST_F(MatchingTest, GTC_SellSweepsThreeBidLevels_AllBidsConsumed) {
    book_.addOrder(OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 100, 150);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, GTC_SellSweepsThreeBidLevels_CorrectTradeQuantities) {
    book_.addOrder(OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 101, 75);
    book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 100, 225);
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(trades[1].quantity, 75);
    ASSERT_EQ(trades[2].quantity, 100);
}

TEST_F(MatchingTest, GTC_BuyPartialSweep_FirstTwoLevelsConsumed) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 102, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 101, 100);
    ASSERT_EQ(trades.size(), 2);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), 102);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderAskFilledFirst) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].askID, id1);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderAskFilledFirst_NewerAskUntouched) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(book_.getBestAsk()->getID(), id2);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderBidFilledFirst) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].bidID, id1);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderBidFilledFirst_NewerBidUntouched) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    ASSERT_EQ(book_.getBestBid()->getID(), id2);
}

TEST_F(MatchingTest, GTC_TradeStruct_BidIDAndAskIDCorrect) {
    auto [askID, _1] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [bidID, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(trades[0].bidID, bidID);
    ASSERT_EQ(trades[0].askID, askID);
}

TEST_F(MatchingTest, GTC_TradeStruct_PriceIsRestingOrderPrice) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 105, 50);
    ASSERT_EQ(trades[0].price, 100);
}

TEST_F(MatchingTest, GTC_TradeStruct_QuantityCorrectOnPartialFill) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 30);
    auto [_, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(trades[0].quantity, 30);
}

