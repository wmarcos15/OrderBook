#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
#include "orderbook.hpp"
#include "types.hpp"

using namespace orderbook;

// Every buy order belongs to BUYER and every sell order to SELLER, so a trade in
// these tests is never between an owner and itself.
constexpr OwnerID BUYER  = 1;
constexpr OwnerID SELLER = 2;

// A legal price of the default book that is not one of the resting prices below
constexpr Price LEGAL_PRICE = 100;

// What a moved-from book must refuse: a logic_error, and not the invalid_argument
// that means the caller passed a bad price or quantity
template<typename Action>
static void expectLogicErrorOnly(Action action) {
    try {
        action();
        ADD_FAILURE() << "nothing was thrown";
    } catch (const std::invalid_argument&) {
        ADD_FAILURE() << "std::invalid_argument was thrown";
    } catch (const std::logic_error&) {
        SUCCEED();
    } catch (...) {
        ADD_FAILURE() << "something other than std::logic_error was thrown";
    }
}

// The two ways of moving a book. Each test runs once for each of them.
struct MoveConstruct {
    static void apply(OrderBook& source, std::optional<OrderBook>& target) {
        target.emplace(std::move(source));
    }
};

struct MoveAssign {
    static void apply(OrderBook& source, std::optional<OrderBook>& target) {
        // The target has orders of its own, which the assignment must replace
        target.emplace();
        target->addOrder(BUYER, OrderType::GTC, Side::buy, 40, 5);
        *target = std::move(source);
    }
};

using MoveKinds = ::testing::Types<MoveConstruct, MoveAssign>;

// source_ holds two buys at 99 and a sell at 101 and is then moved into target_
template<typename Mover>
class MovedBookTest : public ::testing::Test {
protected:
    OrderBook source_;
    std::optional<OrderBook> target_;
    OrderID firstBidID_ {0};
    OrderID secondBidID_ {0};
    OrderID askID_ {0};

    void SetUp() override {
        firstBidID_ = source_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50).orderID;
        secondBidID_ = source_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30).orderID;
        askID_ = source_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 20).orderID;

        Mover::apply(source_, target_);
    }

    // Everything a caller might do to the moved-from book, none of which may reach the target
    void touchSource() {
        source_.cancelOrder(firstBidID_);
        source_.cancelOrder(askID_);
        source_.modifyOrder(secondBidID_, 98, 10);
        source_.addMarketOrder(BUYER, Side::buy, 10);
        source_.addMarketOrder(SELLER, Side::sell, 10);
        source_.getBestBid();
        source_.getBestAsk();
        source_.getDepth(Side::buy, 5);
        source_.getLevelQuantity(Side::buy, 99);
        try {
            source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10);
        } catch (const std::logic_error&) {
            // Refusing is what a moved-from book does, and the target must not notice either way
        }
    }
};

TYPED_TEST_SUITE(MovedBookTest, MoveKinds);

// --------------------------
// Adding orders
// --------------------------
TYPED_TEST(MovedBookTest, AddOrder_GTCBuy_ThrowsLogicErrorNotInvalidArgument) {
    expectLogicErrorOnly([&] { this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10); });
}

TYPED_TEST(MovedBookTest, AddOrder_GTCSell_ThrowsLogicErrorNotInvalidArgument) {
    expectLogicErrorOnly([&] { this->source_.addOrder(SELLER, OrderType::GTC, Side::sell, LEGAL_PRICE, 10); });
}

TYPED_TEST(MovedBookTest, AddOrder_IOCBuy_ThrowsLogicErrorNotInvalidArgument) {
    expectLogicErrorOnly([&] { this->source_.addOrder(BUYER, OrderType::IOC, Side::buy, LEGAL_PRICE, 10); });
}

TYPED_TEST(MovedBookTest, AddOrder_IOCSell_ThrowsLogicErrorNotInvalidArgument) {
    expectLogicErrorOnly([&] { this->source_.addOrder(SELLER, OrderType::IOC, Side::sell, LEGAL_PRICE, 10); });
}

TYPED_TEST(MovedBookTest, AddOrder_FOKBuy_ThrowsLogicErrorNotInvalidArgument) {
    expectLogicErrorOnly([&] { this->source_.addOrder(BUYER, OrderType::FOK, Side::buy, LEGAL_PRICE, 10); });
}

TYPED_TEST(MovedBookTest, AddOrder_FOKSell_ThrowsLogicErrorNotInvalidArgument) {
    expectLogicErrorOnly([&] { this->source_.addOrder(SELLER, OrderType::FOK, Side::sell, LEGAL_PRICE, 10); });
}

TYPED_TEST(MovedBookTest, AddOrder_Refused_RestsNothing) {
    try {
        this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10);
    } catch (const std::logic_error&) {
    }

    EXPECT_TRUE(this->source_.empty());
    EXPECT_EQ(this->source_.getBestBid(), nullptr);
}

// --------------------------
// Cancelling and modifying
// --------------------------
TYPED_TEST(MovedBookTest, CancelOrder_BidThatWasInIt_HasNoEffect) {
    EXPECT_NO_THROW(this->source_.cancelOrder(this->firstBidID_));

    EXPECT_NE(this->target_->getOrder(this->firstBidID_), nullptr);
    EXPECT_EQ(this->target_->size(), 3u);
}

TYPED_TEST(MovedBookTest, CancelOrder_AskThatWasInIt_HasNoEffect) {
    EXPECT_NO_THROW(this->source_.cancelOrder(this->askID_));

    EXPECT_NE(this->target_->getOrder(this->askID_), nullptr);
    EXPECT_EQ(this->target_->size(), 3u);
}

TYPED_TEST(MovedBookTest, CancelOrder_IdThatWasInIt_LeavesTheMovedFromBookEmpty) {
    this->source_.cancelOrder(this->firstBidID_);

    EXPECT_TRUE(this->source_.empty());
    EXPECT_EQ(this->source_.size(), 0u);
}

TYPED_TEST(MovedBookTest, ModifyOrder_BidThatWasInItNewPrice_ReturnsNullopt) {
    EXPECT_EQ(this->source_.modifyOrder(this->firstBidID_, 98, 50), std::nullopt);
}

TYPED_TEST(MovedBookTest, ModifyOrder_BidThatWasInItQuantityDown_ReturnsNullopt) {
    EXPECT_EQ(this->source_.modifyOrder(this->firstBidID_, 99, 10), std::nullopt);
}

TYPED_TEST(MovedBookTest, ModifyOrder_AskThatWasInItNewPrice_ReturnsNullopt) {
    EXPECT_EQ(this->source_.modifyOrder(this->askID_, 102, 20), std::nullopt);
}

TYPED_TEST(MovedBookTest, ModifyOrder_AskThatWasInItQuantityDown_ReturnsNullopt) {
    EXPECT_EQ(this->source_.modifyOrder(this->askID_, 101, 5), std::nullopt);
}

TYPED_TEST(MovedBookTest, ModifyOrder_IdThatWasInIt_LeavesTheTargetAlone) {
    this->source_.modifyOrder(this->firstBidID_, 98, 10);

    EXPECT_EQ(this->target_->getOrder(this->firstBidID_)->getPrice(), 99);
    EXPECT_EQ(this->target_->getOrder(this->firstBidID_)->getRemainingQuantity(), 50u);
}

// --------------------------
// Looking things up
// --------------------------
TYPED_TEST(MovedBookTest, GetOrder_BidThatWasInIt_ReturnsNullptr) {
    EXPECT_EQ(this->source_.getOrder(this->firstBidID_), nullptr);
    EXPECT_EQ(this->source_.getOrder(this->secondBidID_), nullptr);
}

TYPED_TEST(MovedBookTest, GetOrder_AskThatWasInIt_ReturnsNullptr) {
    EXPECT_EQ(this->source_.getOrder(this->askID_), nullptr);
}

TYPED_TEST(MovedBookTest, GetBestBid_AfterTheMove_ReturnsNullptr) {
    EXPECT_EQ(this->source_.getBestBid(), nullptr);
}

TYPED_TEST(MovedBookTest, GetBestAsk_AfterTheMove_ReturnsNullptr) {
    EXPECT_EQ(this->source_.getBestAsk(), nullptr);
}

TYPED_TEST(MovedBookTest, GetLevelQuantity_BuyPriceThatHadOrders_ReturnsZero) {
    EXPECT_EQ(this->source_.getLevelQuantity(Side::buy, 99), 0u);
}

TYPED_TEST(MovedBookTest, GetLevelQuantity_SellPriceThatHadOrders_ReturnsZero) {
    EXPECT_EQ(this->source_.getLevelQuantity(Side::sell, 101), 0u);
}

TYPED_TEST(MovedBookTest, GetLevelCount_BuyPriceThatHadOrders_ReturnsZero) {
    EXPECT_EQ(this->source_.getLevelCount(Side::buy, 99), 0u);
}

TYPED_TEST(MovedBookTest, GetLevelCount_SellPriceThatHadOrders_ReturnsZero) {
    EXPECT_EQ(this->source_.getLevelCount(Side::sell, 101), 0u);
}

TYPED_TEST(MovedBookTest, GetDepth_BuySide_IsEmpty) {
    EXPECT_TRUE(this->source_.getDepth(Side::buy, 5).empty());
}

TYPED_TEST(MovedBookTest, GetDepth_SellSide_IsEmpty) {
    EXPECT_TRUE(this->source_.getDepth(Side::sell, 5).empty());
}

TYPED_TEST(MovedBookTest, Size_AfterTheMove_IsZero) {
    EXPECT_EQ(this->source_.size(), 0u);
}

TYPED_TEST(MovedBookTest, Empty_AfterTheMove_IsTrue) {
    EXPECT_TRUE(this->source_.empty());
}

// --------------------------
// Market orders
// --------------------------
TYPED_TEST(MovedBookTest, AddMarketOrder_Buy_DoesNotThrowAndHasNoTrades) {
    OrderResult result;

    EXPECT_NO_THROW(result = this->source_.addMarketOrder(BUYER, Side::buy, 10));

    EXPECT_TRUE(result.trades.empty());
}

TYPED_TEST(MovedBookTest, AddMarketOrder_Sell_DoesNotThrowAndHasNoTrades) {
    OrderResult result;

    EXPECT_NO_THROW(result = this->source_.addMarketOrder(SELLER, Side::sell, 10));

    EXPECT_TRUE(result.trades.empty());
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_StillHoldsEverything) {
    this->touchSource();

    EXPECT_EQ(this->target_->size(), 3u);
    EXPECT_EQ(this->target_->getLevelQuantity(Side::buy, 99), 80u);
    EXPECT_EQ(this->target_->getLevelCount(Side::buy, 99), 2u);
    EXPECT_EQ(this->target_->getLevelQuantity(Side::sell, 101), 20u);
    ASSERT_NE(this->target_->getBestBid(), nullptr);
    EXPECT_EQ(this->target_->getBestBid()->getID(), this->firstBidID_);
    ASSERT_NE(this->target_->getBestAsk(), nullptr);
    EXPECT_EQ(this->target_->getBestAsk()->getID(), this->askID_);
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_CanMatch) {
    this->touchSource();

    auto result = this->target_->addOrder(SELLER, OrderType::GTC, Side::sell, 99, 60);

    ASSERT_EQ(result.trades.size(), 2u);
    EXPECT_EQ(result.trades[0].bidID, this->firstBidID_);
    EXPECT_EQ(result.trades[0].quantity, 50u);
    EXPECT_EQ(result.trades[1].bidID, this->secondBidID_);
    EXPECT_EQ(result.trades[1].quantity, 10u);
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_CanCancelAndModify) {
    this->touchSource();

    this->target_->cancelOrder(this->firstBidID_);
    auto result = this->target_->modifyOrder(this->askID_, 101, 5);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(this->target_->getOrder(this->firstBidID_), nullptr);
    EXPECT_EQ(this->target_->getLevelQuantity(Side::buy, 99), 30u);
    EXPECT_EQ(this->target_->getLevelQuantity(Side::sell, 101), 5u);
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_CanTakeNewOrders) {
    this->touchSource();

    auto result = this->target_->addOrder(BUYER, OrderType::GTC, Side::buy, 100, 7);

    EXPECT_TRUE(result.trades.empty());
    ASSERT_NE(this->target_->getBestBid(), nullptr);
    EXPECT_EQ(this->target_->getBestBid()->getID(), result.orderID);
    EXPECT_EQ(this->target_->size(), 4u);
}

// --------------------------
// Assigning a fresh book to the moved-from one
// --------------------------
TYPED_TEST(MovedBookTest, AssignedFreshBook_StartsEmpty) {
    this->source_ = OrderBook();

    EXPECT_TRUE(this->source_.empty());
    EXPECT_EQ(this->source_.size(), 0u);
    EXPECT_EQ(this->source_.getBestBid(), nullptr);
    EXPECT_EQ(this->source_.getBestAsk(), nullptr);
    EXPECT_EQ(this->source_.getOrder(this->firstBidID_), nullptr);
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_AddOrder_Rests) {
    this->source_ = OrderBook();

    auto result = this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10);

    ASSERT_NE(this->source_.getBestBid(), nullptr);
    EXPECT_EQ(this->source_.getBestBid()->getID(), result.orderID);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::buy, LEGAL_PRICE), 10u);
    EXPECT_EQ(this->source_.size(), 1u);
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_Match_TradesAtTheRestingPrice) {
    this->source_ = OrderBook();
    this->source_.addOrder(SELLER, OrderType::GTC, Side::sell, LEGAL_PRICE, 10);

    auto result = this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].price, LEGAL_PRICE);
    EXPECT_TRUE(this->source_.empty());
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_CancelAndModify_Work) {
    this->source_ = OrderBook();
    OrderID bid = this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10).orderID;
    OrderID ask = this->source_.addOrder(SELLER, OrderType::GTC, Side::sell, 200, 10).orderID;

    this->source_.cancelOrder(bid);
    auto result = this->source_.modifyOrder(ask, 200, 4);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(this->source_.getBestBid(), nullptr);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::sell, 200), 4u);
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_GetDepth_ListsItsLevels) {
    this->source_ = OrderBook();
    this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 90, 1);
    this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 95, 2);

    auto depth = this->source_.getDepth(Side::buy, 5);

    ASSERT_EQ(depth.size(), 2u);
    EXPECT_EQ(depth[0].price, 95);
    EXPECT_EQ(depth[1].price, 90);
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_MarketOrder_Sweeps) {
    this->source_ = OrderBook();
    this->source_.addOrder(SELLER, OrderType::GTC, Side::sell, 150, 10);

    auto result = this->source_.addMarketOrder(BUYER, Side::buy, 4);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].quantity, 4u);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::sell, 150), 6u);
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_WithAnotherConfig_UsesTheNewBand) {
    this->source_ = OrderBook(BookConfig {5, 10, 200});

    EXPECT_THROW(this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 12, 10), std::invalid_argument);
    EXPECT_THROW(this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 205, 10), std::invalid_argument);
    this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 15, 10);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::buy, 15), 10u);
    EXPECT_EQ(this->source_.getBookConfig().tickSize, 5);
}

TYPED_TEST(MovedBookTest, AssignedFreshBook_LeavesTheTargetAlone) {
    this->source_ = OrderBook();
    this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 10);

    EXPECT_EQ(this->target_->size(), 3u);
    EXPECT_EQ(this->target_->getLevelQuantity(Side::buy, 99), 80u);
}
