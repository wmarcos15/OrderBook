#include <gtest/gtest.h>
#include "orderbook.hpp"
#include "types.hpp"

#include <vector>

// Every buy order belongs to BUYER and every sell order to SELLER, so a trade in
// these tests is never between an owner and itself.
constexpr OwnerID BUYER  = 1;
constexpr OwnerID SELLER = 2;

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
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, GTC_FullFIllAskIsAgressor_BothConsumed) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, GTC_FullFIllBidIsAgressor_OneTrade) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
}

TEST_F(MatchingTest, GTC_FullFIllAskIsAgressor_OneTrade) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_AskConsumed) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_CorrectRestingQty) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150);
    ASSERT_EQ(book_.getBestBid()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_OneTradeCorrectQty) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 125);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillLargerBidAggressor_OneTradeCorrectPrice) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 125);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 150).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 99);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_BidConsumed) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_CorrectRestingQty) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150);
    ASSERT_EQ(book_.getBestAsk()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_OneTradeCorrectQty) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillLargerAskAggressor_OneTradeCorrectPrice) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 100);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_BidConsumed) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_CorrectRestingQty) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100);
    ASSERT_EQ(book_.getBestAsk()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_OneTradeCorrectQty) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerBidAggressor_OneTradeCorrectPrice) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 150);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 99);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_AskConsumed) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_CorrectRestingQty) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100);
    ASSERT_EQ(book_.getBestBid()->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_OneTradeCorrectQty) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 100);
}

TEST_F(MatchingTest, GTC_PartialFillSmallerAskAggressor_OneTradeCorrectPrice) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 150);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 95, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price, 99);
}

TEST_F(MatchingTest, GTC_BuySweeepsThreeAskLevels_ThreeTradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 150).trades;
    ASSERT_EQ(trades.size(), 3);
}

TEST_F(MatchingTest, GTC_BuySweepsThreeAskLevels_AllAsksConsumed) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 150);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, GTC_BuySweepsThreeAskLevels_CorrectTradeQuantities) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 75);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 100);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 225).trades;
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(trades[1].quantity, 75);
    ASSERT_EQ(trades[2].quantity, 100);
}

TEST_F(MatchingTest, GTC_SellSweepsThreeBidLevels_ThreeTradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 150).trades;
    ASSERT_EQ(trades.size(), 3);
}

TEST_F(MatchingTest, GTC_SellSweepsThreeBidLevels_AllBidsConsumed) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 150);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, GTC_SellSweepsThreeBidLevels_CorrectTradeQuantities) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 75);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 225).trades;
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(trades[1].quantity, 75);
    ASSERT_EQ(trades[2].quantity, 100);
}

TEST_F(MatchingTest, GTC_BuyPartialSweep_FirstTwoLevelsConsumed) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 100).trades;
    ASSERT_EQ(trades.size(), 2);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), 102);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderAskFilledFirst) {
    auto id1 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].askID, id1);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderAskFilledFirst_NewerAskUntouched) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto id2 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(book_.getBestAsk()->getID(), id2);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderBidFilledFirst) {
    auto id1 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].bidID, id1);
}

TEST_F(MatchingTest, GTC_TimePriority_OlderBidFilledFirst_NewerBidUntouched) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto id2 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    ASSERT_EQ(book_.getBestBid()->getID(), id2);
}

TEST_F(MatchingTest, GTC_TradeStruct_BidIDAndAskIDCorrect) {
    auto askID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    ASSERT_EQ(result.trades[0].bidID, result.orderID);
    ASSERT_EQ(result.trades[0].askID, askID);
}

TEST_F(MatchingTest, GTC_TradeStruct_PriceIsRestingOrderPrice) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 105, 50).trades;
    ASSERT_EQ(trades[0].price, 100);
}

TEST_F(MatchingTest, GTC_TradeStruct_QuantityCorrectOnPartialFill) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades[0].quantity, 30);
}


// --------------------------
// FOK
// --------------------------
TEST_F(MatchingTest, FOK_BidCanFullyFill_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 150);
    auto trades = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 150).trades;
    ASSERT_FALSE(trades.empty());
}

TEST_F(MatchingTest, FOK_AskCanFullyFill_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 150);
    auto trades = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 150).trades;
    ASSERT_FALSE(trades.empty());
}

TEST_F(MatchingTest, FOK_BidCannotFullyFill_NoChangesInBook) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 50);

    std::size_t sizeBefore = book_.size();
    Price bestBidBefore = book_.getBestBid()->getPrice();
    Price bestAskBefore = book_.getBestAsk()->getPrice();
    Quantity bidQtyBefore = book_.getLevelQuantity(Side::buy, 98);
    Quantity askQtyBefore = book_.getLevelQuantity(Side::sell, 100);

    book_.addOrder(BUYER, OrderType::FOK, Side::buy, 100, 200);

    ASSERT_EQ(book_.size(), sizeBefore);
    ASSERT_EQ(book_.getBestBid()->getPrice(), bestBidBefore);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), bestAskBefore);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 98), bidQtyBefore);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), askQtyBefore);
}

TEST_F(MatchingTest, FOK_BidCannotFullyFill_NoTrades) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 50);

    auto trades = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 100, 200).trades;

    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, FOK_AskCannotFullyFill_NoChangesInBook) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    std::size_t sizeBefore = book_.size();
    Price bestBidBefore = book_.getBestBid()->getPrice();
    Price bestAskBefore = book_.getBestAsk()->getPrice();
    Quantity bidQtyBefore = book_.getLevelQuantity(Side::buy, 100);
    Quantity askQtyBefore = book_.getLevelQuantity(Side::sell, 102);
    book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 200);
    ASSERT_EQ(book_.size(), sizeBefore);
    ASSERT_EQ(book_.getBestBid()->getPrice(), bestBidBefore);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), bestAskBefore);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), bidQtyBefore);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 102), askQtyBefore);
}

TEST_F(MatchingTest, FOK_AskCannotFullyFill_NoTrades) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 200).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, FOK_BidExactLiquidity_FullyFilled) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, FOK_AskExactLiquidity_FullyFilled) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, FOK_BidMultiLevel_SufficientLiquidity_FullyFilled) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 102, 150).trades;
    ASSERT_EQ(trades.size(), 3);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, FOK_AskMultiLevel_SufficientLiquidity_FullyFilled) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 150).trades;
    ASSERT_EQ(trades.size(), 3);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, FOK_BidMultiLevel_InsufficientLiquidity_NoTrades) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addOrder(BUYER, OrderType::FOK, Side::buy, 102, 200).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, FOK_BidMultiLevel_InsufficientLiquidity_BookUnchanged) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    std::size_t sizeBefore = book_.size();
    Quantity l1Before = book_.getLevelQuantity(Side::sell, 100);
    Quantity l2Before = book_.getLevelQuantity(Side::sell, 101);
    Quantity l3Before = book_.getLevelQuantity(Side::sell, 102);
    book_.addOrder(BUYER, OrderType::FOK, Side::buy, 102, 200);
    ASSERT_EQ(book_.size(), sizeBefore);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), l1Before);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 101), l2Before);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 102), l3Before);
}

TEST_F(MatchingTest, FOK_AskMultiLevel_InsufficientLiquidity_NoTrades) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 200).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, FOK_AskMultiLevel_InsufficientLiquidity_BookUnchanged) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    std::size_t sizeBefore = book_.size();
    Quantity l1Before = book_.getLevelQuantity(Side::buy, 102);
    Quantity l2Before = book_.getLevelQuantity(Side::buy, 101);
    Quantity l3Before = book_.getLevelQuantity(Side::buy, 100);
    book_.addOrder(SELLER, OrderType::FOK, Side::sell, 100, 200);
    ASSERT_EQ(book_.size(), sizeBefore);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 102), l1Before);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 101), l2Before);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), l3Before);
}

// --------------------------
// IOC
// --------------------------
TEST_F(MatchingTest, IOC_BidFullyFilled_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
}

TEST_F(MatchingTest, IOC_BidFullyFilled_NothingRests) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto id = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 100, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, IOC_AskFullyFilled_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
}

TEST_F(MatchingTest, IOC_AskFullyFilled_NothingRests) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto id = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 100, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, IOC_BidPartiallyFilled_TradeCorrectQty) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto trades = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 100, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 30);
}

TEST_F(MatchingTest, IOC_BidPartiallyFilled_ResidualDiscarded) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto id = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 100, 100).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, IOC_AskPartiallyFilled_TradeCorrectQty) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    auto trades = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 100, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 30);
}

TEST_F(MatchingTest, IOC_AskPartiallyFilled_ResidualDiscarded) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    auto id = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 100, 100).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, IOC_BidNoFill_NoTrades) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    auto trades = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 100, 50).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, IOC_BidNoFill_OrderDiscarded) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    auto id = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 100, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, IOC_AskNoFill_NoTrades) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50);
    auto trades = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 100, 50).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, IOC_AskNoFill_OrderDiscarded) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50);
    auto id = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 100, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, IOC_BidMultiLevel_PartialFill_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 101, 200).trades;
    ASSERT_EQ(trades.size(), 2);
}

TEST_F(MatchingTest, IOC_BidMultiLevel_PartialFill_ResidualDiscarded) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto id = book_.addOrder(BUYER, OrderType::IOC, Side::buy, 101, 200).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), 102);
}

TEST_F(MatchingTest, IOC_AskMultiLevel_PartialFill_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 101, 200).trades;
    ASSERT_EQ(trades.size(), 2);
}

TEST_F(MatchingTest, IOC_AskMultiLevel_PartialFill_ResidualDiscarded) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto id = book_.addOrder(SELLER, OrderType::IOC, Side::sell, 101, 200).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestBid()->getPrice(), 100);
}

// --------------------------
// Market Order
// --------------------------
TEST_F(MatchingTest, Market_BidFullyFilled_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addMarketOrder(BUYER, Side::buy, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
}

TEST_F(MatchingTest, Market_BidFullyFilled_NothingRests) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto id = book_.addMarketOrder(BUYER, Side::buy, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, Market_AskFullyFilled_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addMarketOrder(SELLER, Side::sell, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
}

TEST_F(MatchingTest, Market_AskFullyFilled_NothingRests) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto id = book_.addMarketOrder(SELLER, Side::sell, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, Market_BidEmptyAskSide_NoTrades) {
    auto trades = book_.addMarketOrder(BUYER, Side::buy, 50).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, Market_BidEmptyAskSide_NothingRests) {
    auto id = book_.addMarketOrder(BUYER, Side::buy, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, Market_AskEmptyBidSide_NoTrades) {
    auto trades = book_.addMarketOrder(SELLER, Side::sell, 50).trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(MatchingTest, Market_AskEmptyBidSide_NothingRests) {
    auto id = book_.addMarketOrder(SELLER, Side::sell, 50).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_TRUE(book_.empty());
}

TEST_F(MatchingTest, Market_BidPartialFill_TradeCorrectQty) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto trades = book_.addMarketOrder(BUYER, Side::buy, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 30);
}

TEST_F(MatchingTest, Market_BidPartialFill_ResidualDiscarded) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto id = book_.addMarketOrder(BUYER, Side::buy, 100).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, Market_AskPartialFill_TradeCorrectQty) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    auto trades = book_.addMarketOrder(SELLER, Side::sell, 100).trades;
    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 30);
}

TEST_F(MatchingTest, Market_AskPartialFill_ResidualDiscarded) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    auto id = book_.addMarketOrder(SELLER, Side::sell, 100).orderID;
    ASSERT_EQ(book_.getOrder(id), nullptr);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, Market_BidSweepsMultipleLevels_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    auto trades = book_.addMarketOrder(BUYER, Side::buy, 150).trades;
    ASSERT_EQ(trades.size(), 3);
}

TEST_F(MatchingTest, Market_BidSweepsMultipleLevels_AllAsksConsumed) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 50);
    book_.addMarketOrder(BUYER, Side::buy, 150);
    ASSERT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(MatchingTest, Market_AskSweepsMultipleLevels_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addMarketOrder(SELLER, Side::sell, 150).trades;
    ASSERT_EQ(trades.size(), 3);
}

TEST_F(MatchingTest, Market_AskSweepsMultipleLevels_AllBidsConsumed) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 102, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addMarketOrder(SELLER, Side::sell, 150);
    ASSERT_EQ(book_.getBestBid(), nullptr);
}


// --------------------------
// Owner IDs on trades
// --------------------------
TEST_F(MatchingTest, Trade_BuyIsAggressor_CarriesBothOwners) {
    constexpr OwnerID MAKER = 10, TAKER = 20;
    book_.addOrder(MAKER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addOrder(TAKER, OrderType::GTC, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].bidOwner, TAKER);
    EXPECT_EQ(trades[0].askOwner, MAKER);
}

TEST_F(MatchingTest, Trade_SellIsAggressor_CarriesBothOwners) {
    constexpr OwnerID MAKER = 10, TAKER = 20;
    book_.addOrder(MAKER, OrderType::GTC, Side::buy, 100, 50);
    auto trades = book_.addOrder(TAKER, OrderType::GTC, Side::sell, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].bidOwner, MAKER);
    EXPECT_EQ(trades[0].askOwner, TAKER);
}

TEST_F(MatchingTest, Trade_SweepAcrossLevels_EachTradeCarriesItsMakersOwner) {
    constexpr OwnerID MAKER_A = 10, MAKER_B = 11, MAKER_C = 12, TAKER = 20;
    book_.addOrder(MAKER_A, OrderType::GTC, Side::sell, 100, 10);
    book_.addOrder(MAKER_B, OrderType::GTC, Side::sell, 101, 10);
    book_.addOrder(MAKER_C, OrderType::GTC, Side::sell, 102, 10);
    auto trades = book_.addOrder(TAKER, OrderType::GTC, Side::buy, 102, 30).trades;
    ASSERT_EQ(trades.size(), 3);
    EXPECT_EQ(trades[0].askOwner, MAKER_A);
    EXPECT_EQ(trades[1].askOwner, MAKER_B);
    EXPECT_EQ(trades[2].askOwner, MAKER_C);
    for (const Trade& t : trades) EXPECT_EQ(t.bidOwner, TAKER);
}

TEST_F(MatchingTest, Trade_SameLevelTimePriority_OwnersFollowQueueOrder) {
    constexpr OwnerID FIRST = 10, SECOND = 11, TAKER = 20;
    book_.addOrder(FIRST, OrderType::GTC, Side::buy, 100, 10);
    book_.addOrder(SECOND, OrderType::GTC, Side::buy, 100, 10);
    auto trades = book_.addOrder(TAKER, OrderType::GTC, Side::sell, 100, 20).trades;
    ASSERT_EQ(trades.size(), 2);
    EXPECT_EQ(trades[0].bidOwner, FIRST);
    EXPECT_EQ(trades[1].bidOwner, SECOND);
    for (const Trade& t : trades) EXPECT_EQ(t.askOwner, TAKER);
}

TEST_F(MatchingTest, Trade_MarketOrder_CarriesBothOwners) {
    constexpr OwnerID MAKER = 10, TAKER = 20;
    book_.addOrder(MAKER, OrderType::GTC, Side::sell, 100, 50);
    auto trades = book_.addMarketOrder(TAKER, Side::buy, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].bidOwner, TAKER);
    EXPECT_EQ(trades[0].askOwner, MAKER);
}

TEST_F(MatchingTest, Trade_IOCPartialFill_CarriesBothOwners) {
    constexpr OwnerID MAKER = 10, TAKER = 20;
    book_.addOrder(MAKER, OrderType::GTC, Side::buy, 100, 30);
    auto trades = book_.addOrder(TAKER, OrderType::IOC, Side::sell, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].bidOwner, MAKER);
    EXPECT_EQ(trades[0].askOwner, TAKER);
}

TEST_F(MatchingTest, Trade_ModifiedOrderFilled_CarriesOriginalOwner) {
    constexpr OwnerID MAKER = 10, TAKER = 20;
    auto id = book_.addOrder(MAKER, OrderType::GTC, Side::sell, 105, 50).orderID;
    book_.modifyOrder(id, 100, 50);
    Trades trades = book_.addOrder(TAKER, OrderType::GTC, Side::buy, 100, 50).trades;
    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].askOwner, MAKER);
}

// --------------------------
// Self-trade prevention
// --------------------------

// OWNER sits on both sides of the book in these tests; OTHER is a second trader.
constexpr OwnerID OWNER = 10;
constexpr OwnerID OTHER = 20;

// --------------------------
// Policy API
// --------------------------
TEST_F(MatchingTest, STP_UnknownOwner_GetsDefaultPolicy) {
    EXPECT_EQ(book_.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelNewest);
}

TEST_F(MatchingTest, STP_DefaultConfig_ReportsCancelNewest) {
    EXPECT_EQ(book_.getBookConfig().defaultSelfTradePolicy, SelfTradePolicy::cancelNewest);
}

TEST_F(MatchingTest, STP_OverriddenOwner_GetsOverride) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);

    EXPECT_EQ(book_.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelOldest);
}

TEST_F(MatchingTest, STP_OverrideOneOwner_OthersKeepDefault) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);

    EXPECT_EQ(book_.getSelfTradePolicy(OTHER), SelfTradePolicy::cancelNewest);
}

TEST_F(MatchingTest, STP_OverrideTwice_LastOneWins) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);

    EXPECT_EQ(book_.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelNewest);
}

TEST(SelfTradeConfigTest, CustomDefault_ReportedByConfig) {
    OrderBook book(BookConfig{.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});

    EXPECT_EQ(book.getBookConfig().defaultSelfTradePolicy, SelfTradePolicy::cancelOldest);
}

TEST(SelfTradeConfigTest, CustomDefault_UsedForUnknownOwners) {
    OrderBook book(BookConfig{.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});

    EXPECT_EQ(book.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelOldest);
}

TEST(SelfTradeConfigTest, CustomDefault_OverrideStillWins) {
    OrderBook book(BookConfig{.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});
    book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);

    EXPECT_EQ(book.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelNewest);
    EXPECT_EQ(book.getSelfTradePolicy(OTHER), SelfTradePolicy::cancelOldest);
}

TEST(SelfTradeConfigTest, CustomDefault_AppliedWhenMatching) {
    OrderBook book(BookConfig{.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});
    auto askID = book.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{askID});
    EXPECT_EQ(book.getOrder(askID), nullptr);
}

// --------------------------
// cancelNewest
// --------------------------
TEST_F(MatchingTest, STP_CancelNewest_GTCBuyHitsOwnAsk_NoTradeAndStatusSet) {
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_TRUE(result.cancelledOrders.empty());
}

TEST_F(MatchingTest, STP_CancelNewest_GTCBuyHitsOwnAsk_RemainderDoesNotRest) {
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_CancelNewest_GTCBuyHitsOwnAsk_RestingOrderUntouched) {
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    ASSERT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getOrder(askID)->getRemainingQuantity(), 50);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(MatchingTest, STP_CancelNewest_GTCSellHitsOwnBid_NoTradeAndStatusSet) {
    book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_TRUE(result.cancelledOrders.empty());
}

TEST_F(MatchingTest, STP_CancelNewest_GTCSellHitsOwnBid_RemainderDoesNotRest) {
    book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 75);

    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_CancelNewest_GTCSellHitsOwnBid_RestingOrderUntouched) {
    auto bidID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 75);

    ASSERT_NE(book_.getOrder(bidID), nullptr);
    EXPECT_EQ(book_.getOrder(bidID)->getRemainingQuantity(), 50);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 100), 50);
    EXPECT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(MatchingTest, STP_CancelNewest_IOCHitsOwnAsk_NoTradeAndRestingUntouched) {
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::IOC, Side::buy, 100, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 1);
    ASSERT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getOrder(askID)->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, STP_CancelNewest_MarketHitsOwnAsk_NoTradeAndRestingUntouched) {
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addMarketOrder(OWNER, Side::buy, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 1);
    ASSERT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getOrder(askID)->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, STP_CancelNewest_MarketHitsOwnBid_NoTradeAndRestingUntouched) {
    auto bidID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto result = book_.addMarketOrder(OWNER, Side::sell, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 1);
    ASSERT_NE(book_.getOrder(bidID), nullptr);
    EXPECT_EQ(book_.getOrder(bidID)->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, STP_CancelNewest_OtherAheadOfOwn_FillKeptThenStops) {
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 80);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[0].askOwner, OTHER);
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    ASSERT_NE(book_.getOrder(ownID), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(MatchingTest, STP_CancelNewest_IOCOtherAheadOfOwn_FillKeptThenStops) {
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::IOC, Side::buy, 100, 80);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
}

TEST_F(MatchingTest, STP_CancelNewest_MarketOtherAheadOfOwn_FillKeptThenStops) {
    book_.addOrder(OTHER, OrderType::GTC, Side::buy, 101, 30);
    book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50);
    auto result = book_.addMarketOrder(OWNER, Side::sell, 80);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[0].price, 101);
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 100), 50);
}

TEST_F(MatchingTest, STP_CancelNewest_OwnAtBestOtherBehind_DoesNotTradeThrough) {
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto otherID = book_.addOrder(OTHER, OrderType::GTC, Side::sell, 101, 30).orderID;
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 101, 80);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.getBestBid(), nullptr);
    ASSERT_NE(book_.getOrder(ownID), nullptr);
    ASSERT_NE(book_.getOrder(otherID), nullptr);
    EXPECT_EQ(book_.getOrder(ownID)->getRemainingQuantity(), 50);
    EXPECT_EQ(book_.getOrder(otherID)->getRemainingQuantity(), 30);
}

// --------------------------
// cancelOldest
// --------------------------
TEST_F(MatchingTest, STP_CancelOldest_BuyHitsOwnAsk_RestingCancelledAndReported) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{askID});
    EXPECT_EQ(book_.getOrder(askID), nullptr);
}

TEST_F(MatchingTest, STP_CancelOldest_SellHitsOwnBid_RestingCancelledAndReported) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto bidID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 75);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{bidID});
    EXPECT_EQ(book_.getOrder(bidID), nullptr);
}

TEST_F(MatchingTest, STP_CancelOldest_OwnOrderAloneAtLevel_LevelDisappears) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    EXPECT_EQ(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 0);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 100), 0);
}

TEST_F(MatchingTest, STP_CancelOldest_GTCRemainderRests) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    ASSERT_NE(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID)->getRemainingQuantity(), 75);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 100), 75);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_CancelOldest_IOCRemainderDiscarded) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::IOC, Side::buy, 100, 75);

    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{askID});
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_TRUE(book_.empty());
}

TEST_F(MatchingTest, STP_CancelOldest_MarketOrder_OwnCancelledThenFillsOther) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 101, 30);
    auto result = book_.addMarketOrder(OWNER, Side::buy, 30);

    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{ownID});
    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[0].price, 101);
    EXPECT_TRUE(book_.empty());
}

TEST_F(MatchingTest, STP_CancelOldest_OwnSharesLevelWithOther_LevelTotalsCorrect) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 20);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 20);
    EXPECT_EQ(result.trades[0].askOwner, OTHER);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 10);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(MatchingTest, STP_CancelOldest_OwnAtBestOtherBehind_ContinuesAndRemainderRests) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 101, 30);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 101, 80);

    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{ownID});
    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[0].price, 101);
    EXPECT_EQ(book_.getBestAsk(), nullptr);
    ASSERT_NE(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID)->getRemainingQuantity(), 50);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 101), 50);
}

TEST_F(MatchingTest, STP_CancelOldest_FillBeforeOwn_KeptAndMatchingContinues) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 40);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 60);

    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{ownID});
    ASSERT_EQ(result.trades.size(), 2);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[1].quantity, 30);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 10);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 100), 1);
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(MatchingTest, STP_CancelOldest_SeveralOwnOrdersAtLevel_AllCancelledInQueueOrder) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto firstID  = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 10).orderID;
    auto secondID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 20).orderID;
    auto thirdID  = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 30).orderID;
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 40);

    EXPECT_EQ(result.cancelledOrders, (std::vector<OrderID>{firstID, secondID, thirdID}));
    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.getBestAsk(), nullptr);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_CancelOldest_OwnOrdersAcrossLevels_AllCancelledBestPriceFirst) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto farID  = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 99, 10).orderID;
    auto nearID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 10).orderID;
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 99, 40);

    EXPECT_EQ(result.cancelledOrders, (std::vector<OrderID>{nearID, farID}));
    EXPECT_EQ(book_.getBestBid(), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 99), 40);
}

// --------------------------
// FOK
// --------------------------
TEST_F(MatchingTest, STP_FOK_CancelNewest_OnlyOwnLiquidity_RejectedBookUntouched) {
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_TRUE(result.cancelledOrders.empty());
    ASSERT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_FOK_CancelOldest_OnlyOwnLiquidity_RejectedNothingCancelled) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_TRUE(result.cancelledOrders.empty());
    ASSERT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_FOK_CancelNewest_OwnOrderBlocksLiquidityBehind_Rejected) {
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 101, 100);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 3);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 80);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 101), 100);
}

TEST_F(MatchingTest, STP_FOK_CancelOldest_OwnOrderSkipped_FilledFromOthers) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 101, 100);

    ASSERT_EQ(result.trades.size(), 2);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[1].quantity, 70);
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{ownID});
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 0);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 101), 30);
}

TEST_F(MatchingTest, STP_FOK_CancelNewest_SellOwnBidBlocksLiquidityBehind_Rejected) {
    book_.addOrder(OTHER, OrderType::GTC, Side::buy, 101, 30);
    book_.addOrder(OWNER, OrderType::GTC, Side::buy, 101, 50);
    book_.addOrder(OTHER, OrderType::GTC, Side::buy, 100, 100);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::sell, 100, 100);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.size(), 3);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 101), 80);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 100), 100);
}

TEST_F(MatchingTest, STP_FOK_CancelOldest_SellOwnBidSkipped_FilledFromOthers) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OTHER, OrderType::GTC, Side::buy, 101, 30);
    auto ownID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 101, 50).orderID;
    book_.addOrder(OTHER, OrderType::GTC, Side::buy, 100, 100);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::sell, 100, 100);

    ASSERT_EQ(result.trades.size(), 2);
    EXPECT_EQ(result.trades[0].quantity, 30);
    EXPECT_EQ(result.trades[1].quantity, 70);
    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{ownID});
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 101), 0);
    EXPECT_EQ(book_.getLevelQuantity(Side::buy, 100), 30);
}

TEST_F(MatchingTest, STP_FOK_CancelNewest_EnoughLiquidityBeforeOwn_FullyFilled) {
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 100);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 100);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 100);
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
}

TEST_F(MatchingTest, STP_FOK_CancelOldest_NotEnoughWithoutOwn_RejectedNothingCancelled) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 60);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_TRUE(result.cancelledOrders.empty());
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_EQ(book_.getLevelQuantity(Side::sell, 100), 80);
    EXPECT_EQ(book_.getLevelCount(Side::sell, 100), 2);
}

TEST_F(MatchingTest, STP_FOK_NoOwnOrders_RejectedForLiquidityStatusOk) {
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 60);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::ok);
}

TEST_F(MatchingTest, STP_FOK_CancelNewest_OwnOrderBeyondLimitPrice_RejectedStatusOk) {
    book_.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 30);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 101, 50);
    auto result = book_.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 60);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::ok);
}

// --------------------------
// Policy is read at match time
// --------------------------
TEST_F(MatchingTest, STP_PolicyChangedAfterResting_NewPolicyApplies) {
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50);

    EXPECT_EQ(result.cancelledOrders, std::vector<OrderID>{askID});
    EXPECT_EQ(book_.getOrder(askID), nullptr);
    EXPECT_NE(book_.getOrder(result.orderID), nullptr);
}

TEST_F(MatchingTest, STP_PolicyChangedBackAfterResting_NewPolicyApplies) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50);

    EXPECT_TRUE(result.cancelledOrders.empty());
    EXPECT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
}

// --------------------------
// Other traders are unaffected
// --------------------------
TEST_F(MatchingTest, STP_DifferentOwnersCross_FillsNormallyStatusOk) {
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OTHER, OrderType::GTC, Side::buy, 100, 50);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 50);
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_TRUE(result.cancelledOrders.empty());
    EXPECT_TRUE(book_.empty());
}

TEST_F(MatchingTest, STP_OtherOwnersPolicy_DoesNotAffectTheirTradesWithOwner) {
    book_.setSelfTradePolicy(OTHER, SelfTradePolicy::cancelOldest);
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(OTHER, OrderType::GTC, Side::buy, 100, 50);

    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_TRUE(result.cancelledOrders.empty());
}

TEST_F(MatchingTest, STP_OwnOrdersThatDoNotCross_BothRestStatusOk) {
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 101, 50);
    auto result = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50);

    EXPECT_EQ(result.status, OrderStatus::ok);
    EXPECT_TRUE(result.cancelledOrders.empty());
    EXPECT_EQ(book_.size(), 2);
}

// --------------------------
// Modify crossing the owner's own order
// --------------------------
TEST_F(MatchingTest, STP_CancelNewest_ModifyCrossesOwnAsk_OriginalAndNewOrderGone) {
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto bidID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 99, 50).orderID;
    auto result = book_.modifyOrder(bidID, 100, 50);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->trades.empty());
    EXPECT_EQ(result->status, OrderStatus::selfTradeCancelledNewest);
    EXPECT_EQ(book_.getOrder(bidID), nullptr);
    EXPECT_EQ(book_.getOrder(result->orderID), nullptr);
    EXPECT_EQ(book_.getBestBid(), nullptr);
    ASSERT_NE(book_.getOrder(askID), nullptr);
    EXPECT_EQ(book_.getOrder(askID)->getRemainingQuantity(), 50);
}

TEST_F(MatchingTest, STP_CancelOldest_ModifyCrossesOwnAsk_AskCancelledNewOrderRests) {
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    auto askID = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto bidID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 99, 50).orderID;
    auto result = book_.modifyOrder(bidID, 100, 50);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, OrderStatus::selfTradeCancelledOldest);
    EXPECT_EQ(result->cancelledOrders, std::vector<OrderID>{askID});
    EXPECT_EQ(book_.getOrder(askID), nullptr);
    ASSERT_NE(book_.getOrder(result->orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result->orderID)->getPrice(), 100);
    EXPECT_EQ(book_.getOrder(result->orderID)->getRemainingQuantity(), 50);
    EXPECT_EQ(book_.size(), 1);
}

TEST_F(MatchingTest, STP_ModifyReducesQtyInPlace_NeverMatchesStatusOk) {
    book_.addOrder(OWNER, OrderType::GTC, Side::sell, 101, 50);
    auto bidID = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto result = book_.modifyOrder(bidID, 100, 20);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, OrderStatus::ok);
    EXPECT_EQ(book_.size(), 2);
}
