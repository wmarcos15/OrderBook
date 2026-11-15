#include <gtest/gtest.h>
#include <stdexcept>
#include "orderbook.hpp"
#include "types.hpp"

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
