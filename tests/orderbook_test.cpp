#include <gtest/gtest.h>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "orderbook.hpp"
#include "types.hpp"

using namespace orderbook;

// Every buy order belongs to BUYER and every sell order to SELLER, so a trade in
// these tests is never between an owner and itself.
constexpr OwnerID BUYER  = 1;
constexpr OwnerID SELLER = 2;

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
    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 250);

    ASSERT_FALSE(book_.empty());
    ASSERT_EQ(book_.size(), 1);
    ASSERT_EQ(book_.getBestBid()->getID(), result.orderID);
    ASSERT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.getBestAsk(), nullptr);
}

TEST_F(OrderBookTest, AddOrder_QtyOf0_ThrowsInvalidArgumentException) {
    ASSERT_THROW(book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 0), std::invalid_argument);
}

TEST_F(OrderBookTest, AddOrder_PriceOf0_ThrowsInvalidArgumentException) {
    ASSERT_THROW(book_.addOrder(BUYER, OrderType::GTC, Side::buy, 0, 250), std::invalid_argument);
}

TEST_F(OrderBookTest, AddSellOrder_EmptyBook_RestsAtCorrectPlace) {
    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 250);

    ASSERT_FALSE(book_.empty());
    ASSERT_EQ(book_.size(), 1);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), result.orderID);
    ASSERT_TRUE(result.trades.empty());
    EXPECT_EQ(book_.getBestBid(), nullptr);
}

TEST_F(OrderBookTest, AddBuyOrder_NonEmptyBuySideEmptySellSide_NewBidIsBestBid) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50);
    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);

    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), result.orderID);
    ASSERT_TRUE(result.trades.empty());
}

TEST_F(OrderBookTest, AddSellOrder_NonEmptySellSideEmptyBuySide_NewAskIsBestAsk) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto result = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 50);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), result.orderID);
    ASSERT_TRUE(result.trades.empty());
}

TEST_F(OrderBookTest, AddBuyOrder_NonEmptyBuySideEmptySellSide_NewBidIsNotBestBid) {
    auto oldID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 50).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);

    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), oldID);
}

TEST_F(OrderBookTest, AddSellOrder_NonEmptySellSideEmptyBuySide_NewAskIsNotBestAsk) {
    auto oldID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);

    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), oldID);
}

TEST_F(OrderBookTest, AddBuyOrder_NonEmptyBook_NoMatch) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy,  98, 100);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100);
    auto result = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50);

    ASSERT_EQ(book_.size(), 3);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), result.orderID);
    ASSERT_TRUE(result.trades.empty());
}

TEST_F(OrderBookTest, AddSellOrder_NonEmptyBook_NoMatch) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell,  105, 100);
    auto bidID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 90, 100).orderID;
    auto trades = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 50).trades;

    ASSERT_EQ(book_.size(), 3);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), bidID);
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, AddBuyOrder_SamePriceTwoOrders_FirstAddedIsBestBid) {
    auto oldID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), oldID);
}

TEST_F(OrderBookTest, AddSellOrder_SamePriceTwoOrders_FirstAddedIsBestAsk) {
    auto oldID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), oldID);
}

TEST_F(OrderBookTest, AddBuyOrder_EmptyBook_LevelDataCorrect) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 250);
    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 250);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, AddSellOrder_EmptyBook_LevelDataCorrect) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 250);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 250);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(OrderBookTest, AddBuyOrder_SamePrice_LevelDataAccumulates) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 100);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 150);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 2);
}

TEST_F(OrderBookTest, AddSellOrder_DifferentPrices_LevelDataIndependent) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 100);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 50);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 100);
    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 99), 50);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 99), 1);
}

// --------------------------
// Cancel Order
// --------------------------
TEST_F(OrderBookTest, CancelOrder_NonExistent_NoEffect) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 200);
    book_.cancelOrder(2);
    ASSERT_FALSE(book_.empty());
    ASSERT_EQ(book_.size(), 1);
}

TEST_F(OrderBookTest, CancelOrder_OnlyOrder_BookIsEmpty) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.cancelOrder(id);
    ASSERT_TRUE(book_.empty());
}

TEST_F(OrderBookTest, CancelOrder_ManyOrders_GetOrderReturnsNullptr) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 75);
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.cancelOrder(id);
    ASSERT_TRUE(book_.getOrder(id) == nullptr);
}

TEST_F(OrderBookTest, CancelBestBid_ManyOrders_NewBestBid) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto cancelID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 105, 200).orderID;
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), id);
}

TEST_F(OrderBookTest, CancelBestAsk_ManyOrders_NewBestAsk) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50).orderID;
    auto cancelID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 105, 200).orderID;
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), id);
}

TEST_F(OrderBookTest, CancelBid_LastPrice_BestBidHasOtherPrice){
    Price price {90};
    auto cancelID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 200).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, 150);
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getPrice(), price);
}

TEST_F(OrderBookTest, CancelAsk_LastPrice_BestAskHasOtherPrice){
    Price price {100};
    auto cancelID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 90, 200).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, price, 150);
    book_.cancelOrder(cancelID);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getPrice(), price);
}

TEST_F(OrderBookTest, CancelBuyOrder_ManyOrdersAtLevel_LevelDataUpdated) {
    auto id1 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.cancelOrder(id1);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, CancelSellOrder_ManyOrdersAtLevel_LevelDataUpdated) {
    auto id1 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 100).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.cancelOrder(id1);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(OrderBookTest, CancelBuyOrder_LastOrderAtLevel_LevelDataGone) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100).orderID;
    book_.cancelOrder(id);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 0);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 0);
}

TEST_F(OrderBookTest, CancelSellOrder_LastOrderAtLevel_LevelDataGone) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 100).orderID;
    book_.cancelOrder(id);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 0);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 0);
}

TEST_F(OrderBookTest, CancelBuyOrder_NonExistent_LevelDataUnchanged) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100);
    book_.cancelOrder(999);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 100);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, CancelSellOrder_NonExistent_LevelDataUnchanged) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 100);
    book_.cancelOrder(999);

    ASSERT_EQ(book_.getLevelQuantity(Side::sell, 100), 100);
    ASSERT_EQ(book_.getLevelCount(Side::sell, 100), 1);
}

TEST_F(OrderBookTest, CancelBuyOrder_PartiallyFilledAmongOthers_LevelDataReflectsRemainder) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto bidID = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 100).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.cancelOrder(bidID);

    ASSERT_EQ(book_.getLevelQuantity(Side::buy, 100), 50);
    ASSERT_EQ(book_.getLevelCount(Side::buy, 100), 1);
}

TEST_F(OrderBookTest, CancelSellOrder_PartiallyFilledAmongOthers_LevelDataReflectsRemainder) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    auto askID = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 100).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
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
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, qty).orderID;
    auto result = *book_.modifyOrder(id, price, qty);
    ASSERT_EQ(id, result.orderID);
    ASSERT_TRUE(result.trades.empty());
}

TEST_F(OrderBookTest, ModifyOrder_ModifyOrderTo0Qty_ThrowsInvalidArgument) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100).orderID;
    ASSERT_THROW(book_.modifyOrder(id, 98, 0), std::invalid_argument);
}

TEST_F(OrderBookTest, ModifyOrder_ModifyOrderTo0Price_ThrowsInvalidArgument) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100).orderID;
    ASSERT_THROW(book_.modifyOrder(id, 0, 100), std::invalid_argument);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_ReturnsSameID) {
    Price price {98};
    Quantity qty1 {250};
    Quantity qty2 {100};
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, qty1).orderID;
    auto returnID = book_.modifyOrder(id, price, qty2)->orderID;
    ASSERT_EQ(id, returnID);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_ReturnsNoTrades) {
    Price price {98};
    Quantity qty1 {250};
    Quantity qty2 {100};
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, price, qty1).orderID;
    auto trades = book_.modifyOrder(id, price, qty2)->trades;
    ASSERT_TRUE(trades.empty());
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_OrderStillAtBook) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 250).orderID;
    book_.modifyOrder(id, 98, 100);
    ASSERT_EQ(book_.size(), 1);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_OrderHasNewQty) {
    Quantity newQty {100};
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 250).orderID;
    book_.modifyOrder(id, 98, newQty);
    ASSERT_EQ(book_.getOrder(id)->getRemainingQuantity(), newQty);
}

TEST_F(OrderBookTest, ModifyOrder_FriendlyModify_OrderPreservesPriority) {
    Quantity newQty {100};
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 250).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 75);
    book_.modifyOrder(id, 98, newQty);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), id);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_NewIDReturned) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100).orderID;
    auto result = book_.modifyOrder(id, 99, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_OldOrderGone) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100).orderID;
    auto result = book_.modifyOrder(id, 99, 100);
    ASSERT_EQ(book_.getOrder(id), nullptr);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_NewOrderResting) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100).orderID;
    auto result = book_.modifyOrder(id, 99, 100);
    ASSERT_NE(book_.getBestBid(), nullptr);
    ASSERT_EQ(book_.getBestBid()->getID(), result->orderID);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChange_LoosesTimePriority) {
    auto id1 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100).orderID;
    auto id2 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100).orderID;
    auto result = book_.modifyOrder(id1, 99, 150);
    ASSERT_EQ(book_.getBestBid()->getID(), id2);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_NewIDReturned) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100).orderID;
    auto result = book_.modifyOrder(id, 102, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_OldOrderGone) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100).orderID;
    auto result = book_.modifyOrder(id, 102, 100);
    ASSERT_EQ(book_.getOrder(id), nullptr);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_NewOrderResting) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100).orderID;
    auto result = book_.modifyOrder(id, 102, 100);
    ASSERT_NE(book_.getBestAsk(), nullptr);
    ASSERT_EQ(book_.getBestAsk()->getID(), result->orderID);
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChange_LoosesTimePriority) {
    auto id1 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100).orderID;
    auto id2 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 99, 100).orderID;
    auto result = book_.modifyOrder(id1, 99, 150);
    ASSERT_EQ(book_.getBestAsk()->getID(), id2);
}

TEST_F(OrderBookTest, ModifyBuyOrder_QuantityIncrease_NewIDReturned) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100).orderID;
    auto result = book_.modifyOrder(id, 99, 150);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifyBuyOrder_QuantityIncrease_LoosesTimePriority) {
    auto id1 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100).orderID;
    auto id2 = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 100).orderID;
    auto result = book_.modifyOrder(id1, 99, 150);
    ASSERT_EQ(book_.getBestBid()->getID(), id2);
}

TEST_F(OrderBookTest, ModifySellOrder_QuantityIncrease_NewIDReturned) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100).orderID;
    auto result = book_.modifyOrder(id, 101, 150);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
}

TEST_F(OrderBookTest, ModifySellOrder_QuantityIncrease_LoosesTimePriority) {
    auto id1 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100).orderID;
    auto id2 = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 100).orderID;
    auto result = book_.modifyOrder(id1, 101, 150);
    ASSERT_EQ(book_.getBestAsk()->getID(), id2);
}

TEST_F(OrderBookTest, ModifyBuyOrder_PriceChangeCrossesSpread_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 100).orderID;
    auto result = book_.modifyOrder(id, 100, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

TEST_F(OrderBookTest, ModifySellOrder_PriceChangeCrossesSpread_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 100).orderID;
    auto result = book_.modifyOrder(id, 100, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

TEST_F(OrderBookTest, ModifyBuyOrder_QuantityIncreaseCrossesSpread_TradesGenerated) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 200);
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50).orderID;
    auto result = book_.modifyOrder(id, 101, 200);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}

TEST_F(OrderBookTest, ModifySellOrder_QuantityIncreaseCrossesSpread_TradesGenerated) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 200);
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50).orderID;
    auto result = book_.modifyOrder(id, 99, 200);
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->trades.empty());
}


// --------------------------
// Book Configuration
// --------------------------
// Tick 5, band [5, 1'000]: used by every test below that needs a custom book.
// minPrice must be on tick too, so it's 5 rather than the default 1.
static BookConfig tick5Config() {
    return BookConfig{.tickSize = 5, .minPrice = 5, .maxPrice = 1'000};
}

// Config validation
TEST(BookConfigTest, Constructor_TickSize0_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.tickSize = 0}), std::invalid_argument);
}

TEST(BookConfigTest, Constructor_NegativeTickSize_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.tickSize = -5}), std::invalid_argument);
}

TEST(BookConfigTest, Constructor_MinPrice0_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.minPrice = 0}), std::invalid_argument);
}

TEST(BookConfigTest, Constructor_NegativeMinPrice_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.minPrice = -10}), std::invalid_argument);
}

TEST(BookConfigTest, Constructor_MinPriceAboveMaxPrice_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.minPrice = 200, .maxPrice = 100}), std::invalid_argument);
}

TEST(BookConfigTest, Constructor_MinPriceNotOnTick_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.tickSize = 5, .minPrice = 7, .maxPrice = 1'000}),
                 std::invalid_argument);
}

// Deliberate: changing only the tick leaves the default band edges (1, 100'000),
// and 1 is not a multiple of 5. Callers who change the tick must set the band too.
TEST(BookConfigTest, Constructor_OnlyTickSizeChanged_DefaultBandNotOnTick_Throws) {
    ASSERT_THROW(OrderBook(BookConfig{.tickSize = 5}), std::invalid_argument);
}

TEST(BookConfigTest, Constructor_MaxPriceNotOnTick_ThrowsInvalidArgument) {
    ASSERT_THROW(OrderBook(BookConfig{.tickSize = 5, .minPrice = 5, .maxPrice = 1'003}),
                 std::invalid_argument);
}

// Config acceptance
TEST(BookConfigTest, Constructor_ValidConfig_StoresExactValues) {
    OrderBook book(tick5Config());
    const BookConfig& config = book.getBookConfig();
    ASSERT_EQ(config.tickSize, 5);
    ASSERT_EQ(config.minPrice, 5);
    ASSERT_EQ(config.maxPrice, 1'000);
    ASSERT_TRUE(book.empty());
}

TEST(BookConfigTest, Constructor_Default_UsesDefaultValues) {
    OrderBook book;
    const BookConfig& config = book.getBookConfig();
    ASSERT_EQ(config.tickSize, 1);
    ASSERT_EQ(config.minPrice, 1);
    ASSERT_EQ(config.maxPrice, 100'000);
}

TEST(BookConfigTest, Constructor_MinPriceEqualsMaxPrice_Accepted) {
    OrderBook book(BookConfig{.tickSize = 5, .minPrice = 100, .maxPrice = 100});
    auto result = book.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 10);
    ASSERT_NE(book.getOrder(result.orderID), nullptr);
    ASSERT_TRUE(result.trades.empty());
}

// addOrder: tick
TEST(BookConfigTest, AddOrder_PriceOnTick_Rests) {
    OrderBook book(tick5Config());
    auto result = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 10);
    ASSERT_TRUE(result.trades.empty());
    ASSERT_EQ(book.getBestBid()->getID(), result.orderID);
    ASSERT_EQ(book.getLevelQuantity(Side::buy, 500), 10);
}

TEST(BookConfigTest, AddOrder_PriceOffTick_ThrowsInvalidArgument) {
    OrderBook book(tick5Config());
    ASSERT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 503, 10), std::invalid_argument);
    ASSERT_THROW(book.addOrder(SELLER, OrderType::GTC, Side::sell, 503, 10), std::invalid_argument);
}

TEST(BookConfigTest, AddOrder_PriceOffTickIOCAndFOK_ThrowsInvalidArgument) {
    OrderBook book(tick5Config());
    ASSERT_THROW(book.addOrder(BUYER, OrderType::IOC, Side::buy, 503, 10), std::invalid_argument);
    ASSERT_THROW(book.addOrder(BUYER, OrderType::FOK, Side::buy, 503, 10), std::invalid_argument);
}

// addOrder: band (edges are inclusive)
TEST(BookConfigTest, AddOrder_PriceExactlyMinPrice_Rests) {
    OrderBook book(tick5Config());
    auto id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 5, 10).orderID;
    ASSERT_EQ(book.getBestBid()->getID(), id);
}

TEST(BookConfigTest, AddOrder_PriceExactlyMaxPrice_Rests) {
    OrderBook book(tick5Config());
    auto id = book.addOrder(SELLER, OrderType::GTC, Side::sell, 1'000, 10).orderID;
    ASSERT_EQ(book.getBestAsk()->getID(), id);
}

TEST(BookConfigTest, AddOrder_OneTickBelowMinPrice_ThrowsInvalidArgument) {
    OrderBook book(BookConfig{.tickSize = 5, .minPrice = 100, .maxPrice = 1'000});
    ASSERT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 95, 10), std::invalid_argument);
}

TEST(BookConfigTest, AddOrder_OneTickAboveMaxPrice_ThrowsInvalidArgument) {
    OrderBook book(tick5Config());
    ASSERT_THROW(book.addOrder(SELLER, OrderType::GTC, Side::sell, 1'005, 10), std::invalid_argument);
}

TEST(BookConfigTest, AddOrder_DefaultBook_AboveDefaultMaxPrice_ThrowsInvalidArgument) {
    OrderBook book;
    ASSERT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 100'001, 10), std::invalid_argument);
}

TEST(BookConfigTest, AddOrder_DefaultBook_AnyPositivePriceInBand_Rests) {
    OrderBook book;
    auto id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 7, 10).orderID;
    ASSERT_EQ(book.getBestBid()->getID(), id);
}

// Rejected orders leave no trace
TEST(BookConfigTest, AddOrder_Rejected_BookUnchangedAndNoIDConsumed) {
    OrderBook book(tick5Config());
    auto firstID = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 10).orderID;

    ASSERT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 503, 10), std::invalid_argument);
    ASSERT_THROW(book.addOrder(BUYER, OrderType::GTC, Side::buy, 2'000, 10), std::invalid_argument);

    ASSERT_EQ(book.size(), 1);
    ASSERT_EQ(book.getLevelQuantity(Side::buy, 500), 10);

    OrderResult next = book.addOrder(BUYER, OrderType::GTC, Side::buy, 505, 10);
    ASSERT_EQ(next.orderID, firstID + 1);
}

// modifyOrder: invalid price throws and leaves the order untouched
TEST(BookConfigTest, ModifyOrder_PriceOffTick_ThrowsAndOrderUnchanged) {
    OrderBook book(tick5Config());
    auto id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 100).orderID;

    ASSERT_THROW(book.modifyOrder(id, 503, 100), std::invalid_argument);

    const Order* order = book.getOrder(id);
    ASSERT_NE(order, nullptr);
    ASSERT_EQ(order->getID(), id);
    ASSERT_EQ(order->getPrice(), 500);
    ASSERT_EQ(order->getRemainingQuantity(), 100);
    ASSERT_EQ(book.getLevelQuantity(Side::buy, 500), 100);
    ASSERT_EQ(book.getLevelCount(Side::buy, 500), 1);
    ASSERT_EQ(book.size(), 1);
}

TEST(BookConfigTest, ModifyOrder_PriceAboveMaxPrice_ThrowsAndOrderUnchanged) {
    OrderBook book(tick5Config());
    auto id = book.addOrder(SELLER, OrderType::GTC, Side::sell, 500, 100).orderID;

    ASSERT_THROW(book.modifyOrder(id, 1'005, 100), std::invalid_argument);

    const Order* order = book.getOrder(id);
    ASSERT_NE(order, nullptr);
    ASSERT_EQ(order->getPrice(), 500);
    ASSERT_EQ(order->getRemainingQuantity(), 100);
    ASSERT_EQ(book.getLevelQuantity(Side::sell, 500), 100);
    ASSERT_EQ(book.getLevelCount(Side::sell, 500), 1);
}

TEST(BookConfigTest, ModifyOrder_PriceBelowMinPrice_ThrowsAndOrderUnchanged) {
    OrderBook book(BookConfig{.tickSize = 5, .minPrice = 100, .maxPrice = 1'000});
    auto id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 100).orderID;

    ASSERT_THROW(book.modifyOrder(id, 95, 100), std::invalid_argument);

    const Order* order = book.getOrder(id);
    ASSERT_NE(order, nullptr);
    ASSERT_EQ(order->getPrice(), 500);
    ASSERT_EQ(book.getLevelQuantity(Side::buy, 500), 100);
}

TEST(BookConfigTest, ModifyOrder_PriceOnTick_MovesOrderWithNewID) {
    OrderBook book(tick5Config());
    auto id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 100).orderID;

    auto result = book.modifyOrder(id, 505, 100);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
    ASSERT_EQ(book.getOrder(id), nullptr);
    ASSERT_EQ(book.getOrder(result->orderID)->getPrice(), 505);
    ASSERT_EQ(book.getLevelQuantity(Side::buy, 500), 0);
    ASSERT_EQ(book.getLevelQuantity(Side::buy, 505), 100);
}

TEST(BookConfigTest, ModifyOrder_DoesNotExistWithInvalidPrice_ReturnsNullOpt) {
    OrderBook book(tick5Config());
    ASSERT_EQ(book.modifyOrder(42, 503, 100), std::nullopt);
}

// --------------------------
// Owner IDs
// --------------------------
TEST_F(OrderBookTest, AddOrder_RestingOrder_StoresOwner) {
    constexpr OwnerID OWNER = 42;
    auto id = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 10).orderID;
    ASSERT_EQ(book_.getOrder(id)->getOwnerID(), OWNER);
}

TEST_F(OrderBookTest, AddOrder_PartiallyFilledRemainder_KeepsOwner) {
    constexpr OwnerID OWNER = 42;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    auto id = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    ASSERT_EQ(book_.getOrder(id)->getRemainingQuantity(), 20);
    ASSERT_EQ(book_.getOrder(id)->getOwnerID(), OWNER);
}

TEST_F(OrderBookTest, ModifyOrder_QuantityReduced_KeepsOwner) {
    constexpr OwnerID OWNER = 42;
    auto id = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto result = book_.modifyOrder(id, 100, 20);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(book_.getOrder(result->orderID)->getOwnerID(), OWNER);
}

TEST_F(OrderBookTest, ModifyOrder_PriceChanged_NewOrderKeepsOwner) {
    constexpr OwnerID OWNER = 42;
    auto id = book_.addOrder(OWNER, OrderType::GTC, Side::sell, 105, 50).orderID;
    auto result = book_.modifyOrder(id, 103, 50);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
    ASSERT_EQ(book_.getOrder(result->orderID)->getOwnerID(), OWNER);
}

TEST_F(OrderBookTest, ModifyOrder_QuantityIncreased_NewOrderKeepsOwner) {
    constexpr OwnerID OWNER = 42;
    auto id = book_.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto result = book_.modifyOrder(id, 100, 80);
    ASSERT_TRUE(result.has_value());
    ASSERT_NE(result->orderID, id);
    ASSERT_EQ(book_.getOrder(result->orderID)->getOwnerID(), OWNER);
}

// --------------------------
// Depth snapshot
// --------------------------
TEST_F(OrderBookTest, GetDepth_EmptyBook_ReturnsNoLevels) {
    EXPECT_TRUE(book_.getDepth(Side::buy, 5).empty());
    EXPECT_TRUE(book_.getDepth(Side::sell, 5).empty());
}

TEST_F(OrderBookTest, GetDepth_ZeroLevelsRequested_ReturnsNoLevels) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50);

    EXPECT_TRUE(book_.getDepth(Side::buy, 0).empty());
    EXPECT_TRUE(book_.getDepth(Side::sell, 0).empty());
}

TEST_F(OrderBookTest, GetDepth_FewerLevelsThanRequested_ReturnsAllOfThem) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30);

    auto depth = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 2);
    EXPECT_EQ(depth[0].price, 100);
    EXPECT_EQ(depth[1].price, 99);
}

TEST_F(OrderBookTest, GetDepth_MoreLevelsThanRequested_ReturnsOnlyTheBestOnes) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 10);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 20);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 103, 30);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 104, 40);

    auto depth = book_.getDepth(Side::sell, 2);

    ASSERT_EQ(depth.size(), 2);
    EXPECT_EQ(depth[0].price, 101);
    EXPECT_EQ(depth[1].price, 102);
}

TEST_F(OrderBookTest, GetDepth_ExactlyAsManyLevelsAsRequested_ReturnsAllOfThem) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 10);

    auto depth = book_.getDepth(Side::buy, 3);

    ASSERT_EQ(depth.size(), 3);
    EXPECT_EQ(depth[2].price, 98);
}

TEST_F(OrderBookTest, GetDepth_BidsAddedInMixedOrder_HighestPriceFirst) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 10);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 20);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 97, 30);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 40);

    auto depth = book_.getDepth(Side::buy, 4);

    ASSERT_EQ(depth.size(), 4);
    EXPECT_EQ(depth[0].price, 100);
    EXPECT_EQ(depth[1].price, 99);
    EXPECT_EQ(depth[2].price, 98);
    EXPECT_EQ(depth[3].price, 97);
}

TEST_F(OrderBookTest, GetDepth_AsksAddedInMixedOrder_LowestPriceFirst) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 103, 10);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 20);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 104, 30);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 40);

    auto depth = book_.getDepth(Side::sell, 4);

    ASSERT_EQ(depth.size(), 4);
    EXPECT_EQ(depth[0].price, 101);
    EXPECT_EQ(depth[1].price, 102);
    EXPECT_EQ(depth[2].price, 103);
    EXPECT_EQ(depth[3].price, 104);
}

TEST_F(OrderBookTest, GetDepth_SeveralOrdersAtOnePrice_OneLevelWithTotals) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 20);

    auto depth = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].price, 100);
    EXPECT_EQ(depth[0].quantity, 100);
    EXPECT_EQ(depth[0].count, 3);
}

TEST_F(OrderBookTest, GetDepth_BothSidesPopulated_EachSideHoldsOnlyItsOwnLevels) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 98, 40);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 30);

    auto bidDepth = book_.getDepth(Side::buy, 5);
    auto askDepth = book_.getDepth(Side::sell, 5);

    ASSERT_EQ(bidDepth.size(), 2);
    ASSERT_EQ(askDepth.size(), 1);
    EXPECT_EQ(bidDepth[0].price, 99);
    EXPECT_EQ(bidDepth[1].price, 98);
    EXPECT_EQ(askDepth[0].price, 101);
    EXPECT_EQ(askDepth[0].quantity, 30);
}

TEST_F(OrderBookTest, GetDepth_PopulatedBook_AgreesWithBestAndLevelQueries) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 25);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 97, 40);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 30);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 104, 60);

    auto bidDepth = book_.getDepth(Side::buy, 5);
    auto askDepth = book_.getDepth(Side::sell, 5);

    ASSERT_EQ(bidDepth.size(), 2);
    ASSERT_EQ(askDepth.size(), 2);
    EXPECT_EQ(bidDepth[0].price, book_.getBestBid()->getPrice());
    EXPECT_EQ(askDepth[0].price, book_.getBestAsk()->getPrice());

    for (const auto& level : bidDepth) {
        EXPECT_EQ(level.quantity, book_.getLevelQuantity(Side::buy, level.price));
        EXPECT_EQ(level.count, book_.getLevelCount(Side::buy, level.price));
    }
    for (const auto& level : askDepth) {
        EXPECT_EQ(level.quantity, book_.getLevelQuantity(Side::sell, level.price));
        EXPECT_EQ(level.count, book_.getLevelCount(Side::sell, level.price));
    }
}

TEST_F(OrderBookTest, GetDepth_RestingOrderPartiallyFilled_QuantityDropsCountStays) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 30);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 20);

    auto depth = book_.getDepth(Side::sell, 5);

    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].quantity, 60);
    EXPECT_EQ(depth[0].count, 2);
    EXPECT_TRUE(book_.getDepth(Side::buy, 5).empty());
}

TEST_F(OrderBookTest, GetDepth_BestLevelFullyFilled_NextLevelBecomesFirst) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 50);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 30);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);

    auto depth = book_.getDepth(Side::sell, 5);

    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].price, 101);
    EXPECT_EQ(depth[0].quantity, 30);
    EXPECT_EQ(depth[0].count, 1);
}

TEST_F(OrderBookTest, GetDepth_OneOrderAmongSeveralCancelled_QuantityAndCountDrop) {
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 20);
    book_.cancelOrder(id);

    auto depth = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].quantity, 70);
    EXPECT_EQ(depth[0].count, 2);
}

TEST_F(OrderBookTest, GetDepth_LastOrderAtLevelCancelled_LevelDisappears) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30);
    book_.cancelOrder(id);

    auto depth = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].price, 99);
    EXPECT_EQ(depth[0].quantity, 30);
}

TEST_F(OrderBookTest, GetDepth_QuantityReducedInPlace_QuantityDropsCountStays) {
    auto id = book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 50).orderID;
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 30);
    book_.modifyOrder(id, 101, 10);

    auto depth = book_.getDepth(Side::sell, 5);

    ASSERT_EQ(depth.size(), 1);
    EXPECT_EQ(depth[0].quantity, 40);
    EXPECT_EQ(depth[0].count, 2);
}

TEST_F(OrderBookTest, GetDepth_OrderMovedToNewPrice_QuantityMovesToNewLevel) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 30);
    book_.modifyOrder(id, 98, 50);

    auto depth = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 2);
    EXPECT_EQ(depth[0].price, 100);
    EXPECT_EQ(depth[0].quantity, 30);
    EXPECT_EQ(depth[0].count, 1);
    EXPECT_EQ(depth[1].price, 98);
    EXPECT_EQ(depth[1].quantity, 50);
    EXPECT_EQ(depth[1].count, 1);
}

// The result is a copy: later changes to the book must not show up in it
TEST_F(OrderBookTest, GetDepth_BookChangedAfterwards_EarlierSnapshotUnchanged) {
    auto id = book_.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50).orderID;
    auto before = book_.getDepth(Side::buy, 5);

    book_.cancelOrder(id);
    book_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30);
    auto after = book_.getDepth(Side::buy, 5);

    ASSERT_EQ(before.size(), 1);
    EXPECT_EQ(before[0].price, 100);
    EXPECT_EQ(before[0].quantity, 50);
    EXPECT_EQ(before[0].count, 1);
    ASSERT_EQ(after.size(), 1);
    EXPECT_EQ(after[0].price, 99);
    EXPECT_EQ(after[0].quantity, 30);
}

// Asking for more levels than could ever exist must return the whole side, not
// try to allocate room for all of them.
TEST_F(OrderBookTest, GetDepth_LargestPossibleLevelsRequested_ReturnsWholeSide) {
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 10);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 20);
    book_.addOrder(SELLER, OrderType::GTC, Side::sell, 103, 30);

    auto depth = book_.getDepth(Side::sell, std::numeric_limits<std::size_t>::max());

    ASSERT_EQ(depth.size(), 3);
    EXPECT_EQ(depth[0].price, 101);
    EXPECT_EQ(depth[2].price, 103);
}

// Only levels that hold orders are reported: 105 and 110 are legal prices
// between the two orders, but nothing rests there.
TEST(BookConfigTest, GetDepth_EmptyPricesBetweenLevels_NotReported) {
    OrderBook book(tick5Config());
    book.addOrder(SELLER, OrderType::GTC, Side::sell, 100, 10);
    book.addOrder(SELLER, OrderType::GTC, Side::sell, 115, 20);

    auto depth = book.getDepth(Side::sell, 5);

    ASSERT_EQ(depth.size(), 2);
    EXPECT_EQ(depth[0].price, 100);
    EXPECT_EQ(depth[1].price, 115);
}

// --------------------------
// Copy and move
// --------------------------
// The book's entries point into its own levels, so a copy would share state
// with the original. Copying must not compile; moving must leave a working book.
TEST(OrderBookCopyMoveTest, OrderBook_Copy_IsNotAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<OrderBook>);
    EXPECT_FALSE(std::is_copy_assignable_v<OrderBook>);
}

TEST(OrderBookCopyMoveTest, OrderBook_Move_IsAllowed) {
    EXPECT_TRUE(std::is_move_constructible_v<OrderBook>);
    EXPECT_TRUE(std::is_move_assignable_v<OrderBook>);
}

TEST(OrderBookCopyMoveTest, MoveConstructed_BookKeepsOrdersAndLevelData) {
    OrderBook source;
    auto bidID = source.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50).orderID;
    source.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30);
    source.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 20);

    OrderBook book(std::move(source));

    ASSERT_EQ(book.size(), 3);
    ASSERT_NE(book.getBestBid(), nullptr);
    EXPECT_EQ(book.getBestBid()->getID(), bidID);
    EXPECT_EQ(book.getLevelQuantity(Side::buy, 99), 80);
    EXPECT_EQ(book.getLevelCount(Side::buy, 99), 2);
    EXPECT_EQ(book.getLevelQuantity(Side::sell, 101), 20);
}

// Cancel, modify and match all go through the pointers stored in the entries
TEST(OrderBookCopyMoveTest, MoveConstructed_BookCanCancelModifyAndMatch) {
    OrderBook source;
    auto firstID = source.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50).orderID;
    auto secondID = source.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30).orderID;
    source.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 20);

    OrderBook book(std::move(source));
    book.cancelOrder(firstID);
    book.modifyOrder(secondID, 99, 10);
    auto result = book.addOrder(BUYER, OrderType::GTC, Side::buy, 101, 20);

    EXPECT_EQ(book.getOrder(firstID), nullptr);
    EXPECT_EQ(book.getLevelQuantity(Side::buy, 99), 10);
    EXPECT_EQ(book.getLevelCount(Side::buy, 99), 1);
    ASSERT_EQ(result.trades.size(), 1);
    EXPECT_EQ(result.trades[0].quantity, 20);
    EXPECT_EQ(book.getBestAsk(), nullptr);
}

TEST(OrderBookCopyMoveTest, MoveConstructed_BookKeepsConfigPoliciesAndNextID) {
    OrderBook source(tick5Config());
    source.setSelfTradePolicy(BUYER, SelfTradePolicy::cancelOldest);
    source.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 50);

    OrderBook book(std::move(source));
    auto id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 95, 10).orderID;

    EXPECT_EQ(book.getBookConfig().tickSize, 5);
    EXPECT_EQ(book.getSelfTradePolicy(BUYER), SelfTradePolicy::cancelOldest);
    EXPECT_EQ(id, 2);
}

TEST(OrderBookCopyMoveTest, MoveAssigned_BookReplacesItsOrdersAndStaysUsable) {
    OrderBook source;
    auto id = source.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 40).orderID;
    source.addOrder(SELLER, OrderType::GTC, Side::sell, 102, 60);

    OrderBook book;
    book.addOrder(BUYER, OrderType::GTC, Side::buy, 50, 5);
    book = std::move(source);
    book.cancelOrder(id);

    ASSERT_EQ(book.size(), 1);
    EXPECT_EQ(book.getBestBid(), nullptr);
    ASSERT_NE(book.getBestAsk(), nullptr);
    EXPECT_EQ(book.getBestAsk()->getPrice(), 102);
    EXPECT_EQ(book.getLevelQuantity(Side::sell, 101), 0);
    EXPECT_EQ(book.getLevelQuantity(Side::sell, 102), 60);
}
