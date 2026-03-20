#include <gtest/gtest.h>
#include <stdexcept>
#include "orderbook.hpp"
#include "types.hpp"

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book_;
};

// --------------------------
// Basic functionality
// --------------------------

// --------------------------
// Add Order
// --------------------------
TEST_F(OrderBookTest, AddBuyOrder_EmptyBook_RestsAtCorrectPlace) {
    auto [id, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 250);

    ASSERT_FALSE(book_.empty());
    ASSERT_EQ(book_.size(), 1);
    ASSERT_EQ(book_.getBestBid()->getID(), id);
    ASSERT_TRUE(trades.empty());
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(OrderBookTest, AddOrder_QtyOf0_ThrowsInvalidArgumentException) {
    ASSERT_THROW(book_.addOrder(OrderType::GTC, Side::buy, 98, 0), std::invalid_argument);
}

TEST_F(OrderBookTest, AddOrder_PriceOf0_ThrowsInvalidArgumentException) {
    ASSERT_THROW(book_.addOrder(OrderType::GTC, Side::buy, 0, 250), std::invalid_argument);
}

TEST_F(OrderBookTest, AddSellOrder_EmptyBook_RestsAtCorrectPlace) {
    auto [id, trades] = book_.addOrder(OrderType::GTC, Side::sell, 100, 250);

    ASSERT_FALSE(book_.empty());
    ASSERT_EQ(book_.size(), 1);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), id);
    ASSERT_TRUE(trades.empty());
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(OrderBookTest, AddBuyOrder_NonEmptyBuySideEmptySellSide_NewBidIsBestBid) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 50);
    auto [id, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);

    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), id);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, AddSellOrder_NonEmptySellSideEmptyBuySide_NewAskIsBestAsk) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [id, trades] = book_.addOrder(OrderType::GTC, Side::sell, 99, 50);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), id);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, AddBuyOrder_NonEmptyBuySideEmptySellSide_NewBidIsNotBestBid) {
    auto [oldID, _1] = book_.addOrder(OrderType::GTC, Side::buy, 101, 50);
    auto [newID, _2] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);

    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), oldID);
}

TEST_F(OrderBookTest, AddSellOrder_NonEmptySellSideEmptyBuySide_NewAskIsNotBestAsk) {
    auto [oldID, _1] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [newID, _2] = book_.addOrder(OrderType::GTC, Side::sell, 101, 50);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), oldID);
}

TEST_F(OrderBookTest, AddBuyOrder_NonEmptyBook_NoMatch) {
    auto [bidID1, _1] = book_.addOrder(OrderType::GTC, Side::buy,  98, 100);
    auto [askID,  _2] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto [bidID2, trades] = book_.addOrder(OrderType::GTC, Side::buy, 99, 50);

    ASSERT_EQ(book_.size(), 3);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), bidID2);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, AddSellOrder_NonEmptyBook_NoMatch) {
    auto [askID1, _1] = book_.addOrder(OrderType::GTC, Side::sell,  105, 100);
    auto [bidID,  _2] = book_.addOrder(OrderType::GTC, Side::buy, 90, 100);
    auto [askID2, trades] = book_.addOrder(OrderType::GTC, Side::sell, 99, 50);

    ASSERT_EQ(book_.size(), 3);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), bidID);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, AddBuyOrder_SamePriceTwoOrders_FirstAddedIsBestBid) {
    auto [oldID, _] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), oldID);
}

TEST_F(OrderBookTest, AddSellOrder_SamePriceTwoOrders_FirstAddedIsBestAsk) {
    auto [oldID, _] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), oldID);
}

TEST_F(OrderBookTest, AddBuyOrder_EmptyBook_LevelDataCorrect) {
    auto [id, trades] = book_.addOrder(OrderType::GTC, Side::buy, 100, 250);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 250);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, AddSellOrder_EmptyBook_LevelDataCorrect) {
    auto [id, trades] = book_.addOrder(OrderType::GTC, Side::sell, 100, 250);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 250);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(OrderBookTest, AddBuyOrder_SamePrice_LevelDataAccumulates) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 100);
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 150);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 2);
}

TEST_F(OrderBookTest, AddSellOrder_DifferentPrices_LevelDataIndependent) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 100);
    book_.addOrder(OrderType::GTC, Side::sell, 99, 50);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 100);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 99), 50);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 99), 1);
}

// --------------------------
// Cancel Order
// --------------------------
TEST_F(OrderBookTest, CancelOrder_NonExistent_NoEffect) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 200);
    book_.cancelOrder(2);
    ASSERT_FALSE(book_.empty());
    ASSERT_EQ(book_.size(), 1);
}

TEST_F(OrderBookTest, CancelOrder_OnlyOrder_BookIsEmpty) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.cancelOrder(id);
    ASSERT_TRUE(book_.empty());
}

TEST_F(OrderBookTest, CancelOrder_ManyOrders_GetOrderReturnsNullptr) {
    book_.addOrder(OrderType::GTC, Side::sell, 102, 75);
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.cancelOrder(id);
    ASSERT_TRUE(book_.getOrder(id) == nullptr);
}

TEST_F(OrderBookTest, CancelBestBid_ManyOrders_NewBestBid) {
    auto [id, _1] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [cancelID, _2] = book_.addOrder(OrderType::GTC, Side::buy, 105, 200);
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), id);
}

TEST_F(OrderBookTest, CancelBestAsk_ManyOrders_NewBestAsk) {
    auto [id, _1] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [cancelID, _2] = book_.addOrder(OrderType::GTC, Side::sell, 105, 200);
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), id);
}

TEST_F(OrderBookTest, CancelBid_LastPrice_BestBidHasOtherPrice){
    Price price {90};
    auto [cancelID, _] = book_.addOrder(OrderType::GTC, Side::buy, 100, 200);
    book_.addOrder(OrderType::GTC, Side::buy, price, 150);
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getPrice(), price);
}

TEST_F(OrderBookTest, CancelAsk_LastPrice_BestAskHasOtherPrice){
    Price price {100};
    auto [cancelID, _] = book_.addOrder(OrderType::GTC, Side::sell, 90, 200);
    book_.addOrder(OrderType::GTC, Side::sell, price, 150);
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), price);
}

TEST_F(OrderBookTest, CancelBuyOrder_ManyOrdersAtLevel_LevelDataUpdated) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.cancelOrder(id1);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, CancelSellOrder_ManyOrdersAtLevel_LevelDataUpdated) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::sell, 100, 100);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.cancelOrder(id1);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(OrderBookTest, CancelBuyOrder_LastOrderAtLevel_LevelDataGone) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    book_.cancelOrder(id);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 0);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 0);
}

TEST_F(OrderBookTest, CancelSellOrder_LastOrderAtLevel_LevelDataGone) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 100, 100);
    book_.cancelOrder(id);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 0);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 0);
}

TEST_F(OrderBookTest, CancelBuyOrder_NonExistent_LevelDataUnchanged) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    book_.cancelOrder(999);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 100);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, CancelSellOrder_NonExistent_LevelDataUnchanged) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 100, 100);
    book_.cancelOrder(999);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 100);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(OrderBookTest, CancelBuyOrder_PartiallyFilledAmongOthers_LevelDataReflectsRemainder) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 30);
    auto [bidID, _1] = book_.addOrder(OrderType::GTC, Side::buy, 100, 100);
    auto [bid2ID, _2] = book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    book_.cancelOrder(bidID);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, CancelSellOrder_PartiallyFilledAmongOthers_LevelDataReflectsRemainder) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 30);
    auto [askID, _1]  = book_.addOrder(OrderType::GTC, Side::sell, 100, 100);
    auto [ask2ID, _2] = book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    book_.cancelOrder(askID);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

// --------------------------
// Modify Order
// --------------------------
TEST_F(OrderBookTest, ModifyOrder_DoesNotExist_ReturnsNullOpt) {
    auto result = book_.modifyOrder(1, 20, 20);
    ASSERT_EQ(result, std::nullopt);
} 

TEST_F(OrderBookTest, ModifyOrder_SamePriceSameQty_ReturnsSameIDNoTrades) {
    Price price {98};
    Quantity qty {250};
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, price, qty);
    auto [returnID, trades] = *book_.modifyOrder(id, price, qty);
    ASSERT_EQ(id, returnID);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, ModifyOrder_ModifyOrderTo0Qty_ThrowsInvalidArgument) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    ASSERT_THROW(book_.modifyOrder(id, 98, 0), std::invalid_argument);
}

TEST_F(OrderBookTest, ModifyOrder_ModifyOrderTo0Price_ThrowsInvalidArgument) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    ASSERT_THROW(book_.modifyOrder(id, 0, 100), std::invalid_argument);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_ReturnsSameID) {
    Price price {98};
    Quantity qty1 {250};
    Quantity qty2 {100};
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, price, qty1);
    auto [returnID, trades] = *book_.modifyOrder(id, price, qty2);
    ASSERT_EQ(id, returnID);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_ReturnsNoTrades) {
    Price price {98};
    Quantity qty1 {250};
    Quantity qty2 {100};
    auto [id, _2] = book_.addOrder(OrderType::GTC, Side::buy, price, qty1);
    auto [_, trades] = *book_.modifyOrder(id, price, qty2);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_OrderStillAtBook) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 250);
    book_.modifyOrder(id, 98, 100);
    ASSERT_EQ(book_.size(), 1);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_OrderHasNewQty) {
    Quantity newQty {100};
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 250);
    book_.modifyOrder(id, 98, newQty);
    ASSERT_EQ(book_.getOrder(id)->getRemainingQuantity(), newQty);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_OrderPreservesPriority) {
    Quantity newQty {100};
    auto [id, _1] = book_.addOrder(OrderType::GTC, Side::buy, 98, 250);
    auto [noPriorityId, _2] = book_.addOrder(OrderType::GTC, Side::buy, 98, 75);
    book_.modifyOrder(id, 98, newQty);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), id);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_NewIDReturned) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    auto result = book_.modifyOrder(id, 99, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_OldOrderGone) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    auto result = book_.modifyOrder(id, 99, 100);
    ASSERT_EQ(book_.getOrder(id), nullptr);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_NewOrderResting) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    auto result = book_.modifyOrder(id, 99, 100);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), result->orderID);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_LoosesTimePriority) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    auto result = book_.modifyOrder(id1, 99, 150);
    ASSERT_EQ(book_.getBestBid()->getID(), id2);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_NewIDReturned) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.modifyOrder(id, 102, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_OldOrderGone) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.modifyOrder(id, 102, 100);
    ASSERT_EQ(book_.getOrder(id), nullptr);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_NewOrderResting) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.modifyOrder(id, 102, 100);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), result->orderID);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_LoosesTimePriority) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::sell, 99, 100);
    auto result = book_.modifyOrder(id1, 99, 150);
    ASSERT_EQ(book_.getBestAsk()->getID(), id2);
}

TEST_F(OrderBookTest, ModifyBuyOrder_QuantityIncrease_NewIDReturned) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    auto result = book_.modifyOrder(id, 99, 150);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifyBuyOrder_QuantityIncrease_LoosesTimePriority) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::buy, 99, 100);
    auto result = book_.modifyOrder(id1, 99, 150);
    ASSERT_EQ(book_.getBestBid()->getID(), id2);
}

TEST_F(OrderBookTest, ModifySellOrder_QuantityIncrease_NewIDReturned) {
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.modifyOrder(id, 101, 150);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifySellOrder_QuantityIncrease_LoosesTimePriority) {
    auto [id1, _1] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto [id2, _2] = book_.addOrder(OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.modifyOrder(id1, 101, 150);
    ASSERT_EQ(book_.getBestAsk()->getID(), id2);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChangeCrossesSpread_TradesGenerated) {
    book_.addOrder(OrderType::GTC, Side::sell, 100, 50);
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 98, 100);
    auto result = book_.modifyOrder(id, 100, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChangeCrossesSpread_TradesGenerated) {
    book_.addOrder(OrderType::GTC, Side::buy, 100, 50);
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 102, 100);
    auto result = book_.modifyOrder(id, 100, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

TEST_F(OrderBookTest, ModifyBuyOrder_QuantityIncreaseCrossesSpread_TradesGenerated) {
    book_.addOrder(OrderType::GTC, Side::sell, 101, 200);
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::buy, 99, 50);
    auto result = book_.modifyOrder(id, 101, 200);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

TEST_F(OrderBookTest, ModifySellOrder_QuantityIncreaseCrossesSpread_TradesGenerated) {
    book_.addOrder(OrderType::GTC, Side::buy, 99, 200);
    auto [id, _] = book_.addOrder(OrderType::GTC, Side::sell, 101, 50);
    auto result = book_.modifyOrder(id, 99, 200);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

