#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

constexpr Price LEGAL_PRICE = 100;

// A moved-from book throws logic_error, not invalid_argument
template<typename Action>
static void expectLogicErrorOnly(Action action) {
    try {
        action();
        ADD_FAILURE() << "nothing was thrown";
    } catch (const std::invalid_argument&) {
        ADD_FAILURE() << "std::invalid_argument was thrown";
    } catch (const std::logic_error&) {
        SUCCEED();
    }
}

struct MoveConstruct {
    static void apply(OrderBook& source, std::optional<OrderBook>& target) {
        target.emplace(std::move(source));
    }
};

struct MoveAssign {
    static void apply(OrderBook& source, std::optional<OrderBook>& target) {
        // Target has orders to be replaced
        target.emplace();
        target->addOrder(BUYER, OrderType::GTC, Side::buy, 40, 5);
        *target = std::move(source);
    }
};

// The source held two bids at 99 and an ask at 101
template<typename Mover>
class MovedBookTest : public ::testing::Test {
protected:
    OrderBook source_;
    std::optional<OrderBook> target_;
    OrderID firstBid_ {0};
    OrderID secondBid_ {0};
    OrderID ask_ {0};

    void SetUp() override {
        firstBid_ = source_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 50).orderID;
        secondBid_ = source_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 30).orderID;
        ask_ = source_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 20).orderID;
        source_.setSelfTradePolicy(BUYER, SelfTradePolicy::cancelOldest);

        Mover::apply(source_, target_);
    }

    void expectTargetHoldsEverything() {
        EXPECT_EQ(target_->size(), 3u);
        EXPECT_EQ(levelsOf(*target_, Side::buy), (Levels {{99, 80, 2}}));
        EXPECT_EQ(levelsOf(*target_, Side::sell), (Levels {{101, 20, 1}}));
        ASSERT_NE(target_->getBestBid(), nullptr);
        EXPECT_EQ(target_->getBestBid()->getID(), firstBid_);
        ASSERT_NE(target_->getBestAsk(), nullptr);
        EXPECT_EQ(target_->getBestAsk()->getID(), ask_);
        EXPECT_NE(target_->getOrder(secondBid_), nullptr);
    }

    // Every call a moved-from book takes
    void touchSource() {
        for (OrderID id : {firstBid_, secondBid_, ask_}) {
            source_.cancelOrder(id);
            source_.modifyOrder(id, 98, 10);
        }
        source_.setSelfTradePolicy(BUYER, SelfTradePolicy::cancelNewest);
        EXPECT_THROW(source_.addOrder(BUYER, OrderType::GTC, Side::buy, LEGAL_PRICE, 10), std::logic_error);
        EXPECT_THROW(source_.addMarketOrder(BUYER, Side::buy, 10), std::logic_error);
        EXPECT_THROW(source_.addMarketOrder(SELLER, Side::sell, 10), std::logic_error);
    }
};

using MoveKinds = ::testing::Types<MoveConstruct, MoveAssign>;
TYPED_TEST_SUITE(MovedBookTest, MoveKinds);

TEST(MovedBookTraitsTest, CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<OrderBook>);
    EXPECT_FALSE(std::is_copy_assignable_v<OrderBook>);
    EXPECT_TRUE(std::is_move_constructible_v<OrderBook>);
    EXPECT_TRUE(std::is_move_assignable_v<OrderBook>);
}

// --------------------------
// The book moved from
// --------------------------
TYPED_TEST(MovedBookTest, Source_LooksEmpty) {
    expectEmptyBook(this->source_);
    for (OrderID id : {this->firstBid_, this->secondBid_, this->ask_})
        EXPECT_EQ(this->source_.getOrder(id), nullptr);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::buy, 99), 0u);
    EXPECT_EQ(this->source_.getLevelCount(Side::buy, 99), 0u);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::sell, 101), 0u);
    EXPECT_EQ(this->source_.getLevelCount(Side::sell, 101), 0u);
}

TYPED_TEST(MovedBookTest, Source_AddOrder_ThrowsLogicErrorAndRestsNothing) {
    for (Side side : {Side::buy, Side::sell})
        for (OrderType type : {OrderType::GTC, OrderType::IOC, OrderType::FOK})
            for (Price price : {1, 99, LEGAL_PRICE, 100'000})
                expectLogicErrorOnly([&] { this->source_.addOrder(ownerOf(side), type, side, price, 10); });

    expectEmptyBook(this->source_);
}

TYPED_TEST(MovedBookTest, Source_IllegalPriceOrQuantity_StillThrowsInvalidArgument) {
    EXPECT_THROW(this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 0, 10), std::invalid_argument);
    EXPECT_THROW(this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 100'001, 10), std::invalid_argument);
    EXPECT_THROW(this->source_.addMarketOrder(BUYER, Side::buy, 0), std::invalid_argument);
}

TYPED_TEST(MovedBookTest, Source_MarketOrder_ThrowsLogicError) {
    for (Side side : {Side::buy, Side::sell})
        expectLogicErrorOnly([&] { this->source_.addMarketOrder(ownerOf(side), side, 10); });

    expectEmptyBook(this->source_);
}

TYPED_TEST(MovedBookTest, Source_CancelOrder_HasNoEffect) {
    for (OrderID id : {this->firstBid_, this->secondBid_, this->ask_})
        this->source_.cancelOrder(id);

    expectEmptyBook(this->source_);
    this->expectTargetHoldsEverything();
}

TYPED_TEST(MovedBookTest, Source_ModifyOrder_ReturnsNullopt) {
    EXPECT_EQ(this->source_.modifyOrder(this->firstBid_, 98, 50), std::nullopt);
    EXPECT_EQ(this->source_.modifyOrder(this->firstBid_, 99, 10), std::nullopt);
    EXPECT_EQ(this->source_.modifyOrder(this->ask_, 102, 20), std::nullopt);
    EXPECT_EQ(this->source_.modifyOrder(this->ask_, 101, 5), std::nullopt);
    this->expectTargetHoldsEverything();
}

// --------------------------
// The book moved to
// --------------------------
TYPED_TEST(MovedBookTest, Target_HoldsEverything) {
    this->expectTargetHoldsEverything();
    EXPECT_EQ(this->target_->getLevelQuantity(Side::buy, 40), 0u);
}

TYPED_TEST(MovedBookTest, Target_KeepsPoliciesAndNextID) {
    EXPECT_EQ(this->target_->getSelfTradePolicy(BUYER), SelfTradePolicy::cancelOldest);
    EXPECT_EQ(this->target_->addOrder(BUYER, OrderType::GTC, Side::buy, 95, 10).orderID, this->ask_ + 1);
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_StillHoldsEverything) {
    this->touchSource();

    this->expectTargetHoldsEverything();
    EXPECT_EQ(this->target_->getSelfTradePolicy(BUYER), SelfTradePolicy::cancelOldest);
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_CanMatch) {
    this->touchSource();

    Trades trades = this->target_->addOrder(SELLER, OrderType::GTC, Side::sell, 99, 60).trades;

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].bidID, this->firstBid_);
    EXPECT_EQ(trades[0].quantity, 50u);
    EXPECT_EQ(trades[1].bidID, this->secondBid_);
    EXPECT_EQ(trades[1].quantity, 10u);
    EXPECT_EQ(levelsOf(*this->target_, Side::buy), (Levels {{99, 20, 1}}));
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_CanCancelAndModify) {
    this->touchSource();

    this->target_->cancelOrder(this->firstBid_);
    auto reduced = this->target_->modifyOrder(this->secondBid_, 99, 10);
    auto moved = this->target_->modifyOrder(this->ask_, 102, 20);

    ASSERT_TRUE(reduced.has_value());
    EXPECT_EQ(reduced->orderID, this->secondBid_);
    ASSERT_TRUE(moved.has_value());
    EXPECT_EQ(this->target_->getOrder(this->firstBid_), nullptr);
    EXPECT_EQ(levelsOf(*this->target_, Side::buy), (Levels {{99, 10, 1}}));
    EXPECT_EQ(levelsOf(*this->target_, Side::sell), (Levels {{102, 20, 1}}));
}

TYPED_TEST(MovedBookTest, Target_AfterTouchingTheSource_CanTakeNewOrders) {
    this->touchSource();

    OrderResult result = this->target_->addOrder(BUYER, OrderType::GTC, Side::buy, 100, 7);

    EXPECT_TRUE(result.trades.empty());
    ASSERT_NE(this->target_->getBestBid(), nullptr);
    EXPECT_EQ(this->target_->getBestBid()->getID(), result.orderID);
    EXPECT_EQ(this->target_->size(), 4u);
}

TYPED_TEST(MovedBookTest, Target_MovedAgain_StillWorks) {
    OrderBook last(std::move(*this->target_));

    last.cancelOrder(this->firstBid_);
    Trades trades = last.addMarketOrder(SELLER, Side::sell, 30).trades;

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].bidID, this->secondBid_);
    expectEmptyBook(*this->target_);
}

TYPED_TEST(MovedBookTest, Target_MovedOntoItself_KeepsEverythingAndStaysUsable) {
    OrderBook& same = *this->target_;

    *this->target_ = std::move(same);

    this->expectTargetHoldsEverything();
    EXPECT_EQ(this->target_->getSelfTradePolicy(BUYER), SelfTradePolicy::cancelOldest);

    this->target_->cancelOrder(this->firstBid_);
    EXPECT_EQ(this->target_->addMarketOrder(SELLER, Side::sell, 30).trades.size(), 1u);
    EXPECT_EQ(this->target_->addOrder(BUYER, OrderType::GTC, Side::buy, 95, 10).orderID, this->ask_ + 2);
    EXPECT_EQ(levelsOf(*this->target_, Side::buy), (Levels {{95, 10, 1}}));
    EXPECT_EQ(levelsOf(*this->target_, Side::sell), (Levels {{101, 20, 1}}));
}

TEST(MovedBookConfigTest, OntoABookWithAnotherBand_TakesTheBandOfTheSource) {
    OrderBook source(BookConfig {5, 10, 200});
    source.addOrder(BUYER, OrderType::GTC, Side::buy, 15, 10);
    OrderBook target;
    target.addOrder(SELLER, OrderType::GTC, Side::sell, 50'000, 10);

    target = std::move(source);

    EXPECT_EQ(target.getBookConfig().tickSize, 5);
    EXPECT_EQ(target.getBookConfig().minPrice, 10);
    EXPECT_EQ(target.getBookConfig().maxPrice, 200);
    EXPECT_EQ(levelsOf(target, Side::buy), (Levels {{15, 10, 1}}));
    EXPECT_EQ(target.getBestAsk(), nullptr);
    EXPECT_THROW(target.addOrder(BUYER, OrderType::GTC, Side::buy, 12, 10), std::invalid_argument);
    EXPECT_THROW(target.addOrder(BUYER, OrderType::GTC, Side::buy, 205, 10), std::invalid_argument);
}

// --------------------------
// A fresh book assigned to the source
// --------------------------
TYPED_TEST(MovedBookTest, FreshBookAssigned_StartsEmpty) {
    this->source_ = OrderBook();

    expectEmptyBook(this->source_);
    EXPECT_EQ(this->source_.getOrder(this->firstBid_), nullptr);
    EXPECT_EQ(this->source_.getSelfTradePolicy(BUYER), SelfTradePolicy::cancelNewest);
}

TYPED_TEST(MovedBookTest, FreshBookAssigned_WorksAndLeavesTheTargetAlone) {
    this->source_ = OrderBook();

    OrderID bid = this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 99, 10).orderID;
    OrderID ask = this->source_.addOrder(SELLER, OrderType::GTC, Side::sell, 101, 10).orderID;
    EXPECT_EQ(bid, 1u);
    EXPECT_EQ(levelsOf(this->source_, Side::buy), (Levels {{99, 10, 1}}));

    EXPECT_EQ(this->source_.addMarketOrder(BUYER, Side::buy, 4).trades.size(), 1u);
    this->source_.modifyOrder(ask, 101, 2);
    this->source_.cancelOrder(bid);
    EXPECT_EQ(levelsOf(this->source_, Side::sell), (Levels {{101, 2, 1}}));
    EXPECT_EQ(this->source_.getBestBid(), nullptr);

    this->expectTargetHoldsEverything();
}

TYPED_TEST(MovedBookTest, FreshBookAssigned_WithAnotherConfig_UsesTheNewBand) {
    this->source_ = OrderBook(BookConfig {5, 10, 200});

    EXPECT_THROW(this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 12, 10), std::invalid_argument);
    EXPECT_THROW(this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 205, 10), std::invalid_argument);
    this->source_.addOrder(BUYER, OrderType::GTC, Side::buy, 15, 10);
    EXPECT_EQ(this->source_.getLevelQuantity(Side::buy, 15), 10u);
    EXPECT_EQ(this->source_.getBookConfig().tickSize, 5);
}
