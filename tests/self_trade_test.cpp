#include <gtest/gtest.h>
#include <optional>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// OWNER sits on both sides of the book; OTHER is a second trader
constexpr OwnerID OWNER = 10;
constexpr OwnerID OTHER = 20;

// The incoming orders are OWNER's, on side_, and the resting ones are on the other side
class SelfTradeTest : public SidedBookTest {
protected:
    void useCancelOldest() {
        book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);
    }

    OrderID restFor(OwnerID owner, Price price, Quantity quantity) {
        return book_.addOrder(owner, OrderType::GTC, other_, at(price), quantity).orderID;
    }

    OrderResult send(OrderType type, Price price, Quantity quantity) {
        return book_.addOrder(OWNER, type, side_, at(price), quantity);
    }

    OrderResult sendMarket(Quantity quantity) {
        return book_.addMarketOrder(OWNER, side_, quantity);
    }

    OwnerID makerOf(const Trade& trade) const {
        return (side_ == Side::buy) ? trade.askOwner : trade.bidOwner;
    }

    static void expectCancelledNewest(const OrderResult& result) {
        EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
        EXPECT_TRUE(result.cancelledOrders.empty());
    }

    static void expectCancelledOldest(const OrderResult& result, const IDs& cancelled) {
        EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledOldest);
        EXPECT_EQ(result.cancelledOrders, cancelled);
    }

    static void expectNoSelfTrade(const OrderResult& result) {
        EXPECT_EQ(result.status, OrderStatus::ok);
        EXPECT_TRUE(result.cancelledOrders.empty());
    }
};

INSTANTIATE_FOR_BOTH_SIDES(SelfTradeTest);

// --------------------------
// Policies
// --------------------------
TEST(SelfTradePolicyTest, UnknownOwner_GetsCancelNewest) {
    OrderBook book;

    EXPECT_EQ(book.getBookConfig().defaultSelfTradePolicy, SelfTradePolicy::cancelNewest);
    EXPECT_EQ(book.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelNewest);
}

TEST(SelfTradePolicyTest, SetForOneOwner_OthersKeepTheDefault) {
    OrderBook book;

    book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);

    EXPECT_EQ(book.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelOldest);
    EXPECT_EQ(book.getSelfTradePolicy(OTHER), SelfTradePolicy::cancelNewest);
    EXPECT_EQ(book.getBookConfig().defaultSelfTradePolicy, SelfTradePolicy::cancelNewest);
}

TEST(SelfTradePolicyTest, SetTwice_LastOneWins) {
    OrderBook book;
    book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelOldest);

    book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);

    EXPECT_EQ(book.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelNewest);
}

TEST(SelfTradePolicyTest, CustomDefault_UsedForUnknownOwnersOnly) {
    OrderBook book(BookConfig {.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});
    book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);

    EXPECT_EQ(book.getBookConfig().defaultSelfTradePolicy, SelfTradePolicy::cancelOldest);
    EXPECT_EQ(book.getSelfTradePolicy(OTHER), SelfTradePolicy::cancelOldest);
    EXPECT_EQ(book.getSelfTradePolicy(OWNER), SelfTradePolicy::cancelNewest);
}

TEST(SelfTradePolicyTest, CustomDefault_AppliedWhenMatching) {
    OrderBook book(BookConfig {.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});
    OrderID ask = book.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50).orderID;

    OrderResult result = book.addOrder(OWNER, OrderType::GTC, Side::buy, 100, 75);

    EXPECT_EQ(result.cancelledOrders, (IDs {ask}));
    EXPECT_EQ(book.getOrder(ask), nullptr);
}

// The check uses the policy of the owner, not the default of the book
TEST(SelfTradePolicyTest, CustomDefaultButOwnerCancelsNewest_FOKStopsAtItsOwnOrder) {
    OrderBook book(BookConfig {.defaultSelfTradePolicy = SelfTradePolicy::cancelOldest});
    book.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);
    book.addOrder(OWNER, OrderType::GTC, Side::sell, 100, 50);
    book.addOrder(OTHER, OrderType::GTC, Side::sell, 100, 50);

    OrderResult result = book.addOrder(OWNER, OrderType::FOK, Side::buy, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    EXPECT_EQ(result.status, OrderStatus::selfTradeCancelledNewest);
}

TEST_P(SelfTradeTest, PolicyChangedAfterResting_NewOneApplies) {
    OrderID own = restFor(OWNER, 100, 50);
    useCancelOldest();

    OrderResult result = send(OrderType::GTC, 100, 50);

    EXPECT_EQ(result.cancelledOrders, (IDs {own}));
    EXPECT_EQ(book_.getOrder(own), nullptr);
    EXPECT_NE(book_.getOrder(result.orderID), nullptr);
}

TEST_P(SelfTradeTest, PolicyChangedBackAfterResting_NewOneApplies) {
    useCancelOldest();
    OrderID own = restFor(OWNER, 100, 50);
    book_.setSelfTradePolicy(OWNER, SelfTradePolicy::cancelNewest);

    OrderResult result = send(OrderType::GTC, 100, 50);

    expectCancelledNewest(result);
    EXPECT_NE(book_.getOrder(own), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
}

// --------------------------
// cancelNewest
// --------------------------
TEST_P(SelfTradeTest, CancelNewest_AnyTypeHitsItsOwnOrder_IncomingIsDropped) {
    OrderID own = restFor(OWNER, 100, 50);
    Snapshot before = snapshotOf(book_);

    for (OrderType type : {OrderType::GTC, OrderType::IOC, OrderType::FOK}) {
        OrderResult result = send(type, 100, 50);

        EXPECT_TRUE(result.trades.empty());
        expectCancelledNewest(result);
        EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
    }

    ASSERT_NE(book_.getOrder(own), nullptr);
    EXPECT_EQ(book_.getOrder(own)->getRemainingQuantity(), 50u);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, CancelNewest_LargerThanItsOwnOrder_RemainderDoesNotRest) {
    restFor(OWNER, 100, 50);
    Snapshot before = snapshotOf(book_);

    for (OrderType type : {OrderType::GTC, OrderType::IOC}) {
        OrderResult result = send(type, 100, 75);

        EXPECT_TRUE(result.trades.empty());
        expectCancelledNewest(result);
    }

    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, CancelNewest_MarketOrderAgainstOnlyOwnOrders_BookUntouched) {
    restFor(OWNER, 10, 5);
    restFor(OWNER, 90'010, 5);
    Snapshot before = snapshotOf(book_);

    OrderResult result = sendMarket(8);

    EXPECT_TRUE(result.trades.empty());
    expectCancelledNewest(result);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, CancelNewest_OtherAheadOfOwn_FillKeptThenStops) {
    for (OrderType type : {OrderType::GTC, OrderType::IOC}) {
        restFor(OTHER, 100, 30);
        OrderID own = restFor(OWNER, 100, 50);

        OrderResult result = send(type, 100, 80);

        ASSERT_EQ(result.trades.size(), 1u);
        EXPECT_EQ(result.trades[0].quantity, 30u);
        EXPECT_EQ(makerOf(result.trades[0]), OTHER);
        expectCancelledNewest(result);
        EXPECT_EQ(book_.getOrder(result.orderID), nullptr);
        EXPECT_NE(book_.getOrder(own), nullptr);
        expectLevel(other_, 100, 50, 1);
        book_.cancelOrder(own);
    }
}

TEST_P(SelfTradeTest, CancelNewest_MarketOrderOtherAheadOfOwn_FillKeptThenStops) {
    restFor(OTHER, 99, 30);
    restFor(OWNER, 100, 50);

    OrderResult result = sendMarket(80);

    EXPECT_EQ(tradePrices(result.trades), at(Prices {99}));
    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {30}));
    expectCancelledNewest(result);
    expectLevel(other_, 100, 50, 1);
}

TEST_P(SelfTradeTest, CancelNewest_OwnAtTheBest_DoesNotTradeThroughIt) {
    restFor(OWNER, 100, 50);
    restFor(OTHER, 101, 30);
    Snapshot before = snapshotOf(book_);

    OrderResult result = send(OrderType::GTC, 101, 80);

    EXPECT_TRUE(result.trades.empty());
    expectCancelledNewest(result);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, CancelNewest_FilledBeforeReachingItsOwnOrder_NoSelfTrade) {
    restFor(OTHER, 100, 30);
    restFor(OWNER, 100, 50);

    OrderResult result = send(OrderType::GTC, 100, 30);

    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {30}));
    expectNoSelfTrade(result);
    expectLevel(other_, 100, 50, 1);
}

// --------------------------
// cancelOldest
// --------------------------
TEST_P(SelfTradeTest, CancelOldest_HitsItsOwnOrder_RestingIsCancelledAndReported) {
    useCancelOldest();
    OrderID own = restFor(OWNER, 100, 50);

    OrderResult result = send(OrderType::GTC, 100, 75);

    EXPECT_TRUE(result.trades.empty());
    expectCancelledOldest(result, {own});
    EXPECT_EQ(book_.getOrder(own), nullptr);
    EXPECT_EQ(best(other_), nullptr);
    expectLevel(other_, 100, 0, 0);
}

TEST_P(SelfTradeTest, CancelOldest_GTC_RestsWithAllItsQuantity) {
    useCancelOldest();
    restFor(OWNER, 100, 50);

    OrderResult result = send(OrderType::GTC, 100, 75);

    ASSERT_NE(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID)->getRemainingQuantity(), 75u);
    expectLevel(side_, 100, 75, 1);
    EXPECT_EQ(book_.size(), 1u);
}

TEST_P(SelfTradeTest, CancelOldest_IOC_RemainderDiscarded) {
    useCancelOldest();
    OrderID own = restFor(OWNER, 100, 50);

    OrderResult result = send(OrderType::IOC, 100, 75);

    expectCancelledOldest(result, {own});
    expectEmptyBook(book_);
}

TEST_P(SelfTradeTest, CancelOldest_MarketOrderAgainstOnlyOwnOrders_AllCancelledAcrossTheGap) {
    useCancelOldest();
    OrderID near = restFor(OWNER, 10, 5);
    OrderID far = restFor(OWNER, 90'010, 5);

    OrderResult result = sendMarket(8);

    EXPECT_TRUE(result.trades.empty());
    expectCancelledOldest(result, {near, far});
    expectEmptyBook(book_);
}

TEST_P(SelfTradeTest, CancelOldest_MarketOrderOwnAloneAtItsLevel_ContinuesToTheFarLevel) {
    useCancelOldest();
    OrderID own = restFor(OWNER, 10, 5);
    restFor(OTHER, 90'010, 7);

    OrderResult result = sendMarket(5);

    expectCancelledOldest(result, {own});
    EXPECT_EQ(tradePrices(result.trades), at(Prices {90'010}));
    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {5}));
    expectLevel(other_, 10, 0, 0);
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{90'010, 2, 1}}));
}

TEST_P(SelfTradeTest, CancelOldest_GTCOwnAloneAtItsLevel_ContinuesAndTheRemainderRests) {
    useCancelOldest();
    OrderID own = restFor(OWNER, 10, 50);
    restFor(OTHER, 90'010, 30);

    OrderResult result = send(OrderType::GTC, 90'010, 80);

    expectCancelledOldest(result, {own});
    EXPECT_EQ(tradePrices(result.trades), at(Prices {90'010}));
    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {30}));
    EXPECT_EQ(best(other_), nullptr);
    ASSERT_NE(book_.getOrder(result.orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result.orderID)->getRemainingQuantity(), 50u);
    expectLevel(side_, 90'010, 50, 1);
}

TEST_P(SelfTradeTest, CancelOldest_OwnSharesItsLevel_TotalsFollow) {
    useCancelOldest();
    restFor(OWNER, 100, 50);
    restFor(OTHER, 100, 30);

    OrderResult result = send(OrderType::GTC, 100, 20);

    ASSERT_EQ(result.trades.size(), 1u);
    EXPECT_EQ(result.trades[0].quantity, 20u);
    EXPECT_EQ(makerOf(result.trades[0]), OTHER);
    expectLevel(other_, 100, 10, 1);
}

TEST_P(SelfTradeTest, CancelOldest_FillBeforeOwn_KeptAndMatchingContinues) {
    useCancelOldest();
    restFor(OTHER, 100, 30);
    OrderID own = restFor(OWNER, 100, 50);
    restFor(OTHER, 100, 40);

    OrderResult result = send(OrderType::GTC, 100, 60);

    expectCancelledOldest(result, {own});
    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {30, 30}));
    expectLevel(other_, 100, 10, 1);
    EXPECT_EQ(best(side_), nullptr);
}

TEST_P(SelfTradeTest, CancelOldest_SeveralOwnAtOneLevel_CancelledInOrderOfArrival) {
    useCancelOldest();
    IDs own;
    for (Quantity quantity : {10, 20, 30})
        own.push_back(restFor(OWNER, 100, quantity));

    OrderResult result = send(OrderType::GTC, 100, 40);

    EXPECT_TRUE(result.trades.empty());
    expectCancelledOldest(result, own);
    EXPECT_EQ(best(other_), nullptr);
    EXPECT_EQ(book_.size(), 1u);
}

TEST_P(SelfTradeTest, CancelOldest_OwnAcrossLevels_CancelledBestPriceFirst) {
    useCancelOldest();
    OrderID far = restFor(OWNER, 101, 10);
    OrderID near = restFor(OWNER, 100, 10);

    OrderResult result = send(OrderType::GTC, 101, 40);

    expectCancelledOldest(result, {near, far});
    EXPECT_EQ(best(other_), nullptr);
    expectLevel(side_, 101, 40, 1);
}

// --------------------------
// Fill or kill
// --------------------------
TEST_P(SelfTradeTest, FOKCancelOldest_OnlyOwnOrders_KilledAndNothingCancelled) {
    useCancelOldest();
    restFor(OWNER, 100, 50);
    Snapshot before = snapshotOf(book_);

    OrderResult result = send(OrderType::FOK, 100, 50);

    EXPECT_TRUE(result.trades.empty());
    expectNoSelfTrade(result);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, FOKCancelNewest_OwnOrderBlocksTheOnesBehind_Killed) {
    restFor(OTHER, 100, 30);
    restFor(OWNER, 100, 50);
    restFor(OTHER, 101, 100);
    Snapshot before = snapshotOf(book_);

    OrderResult result = send(OrderType::FOK, 101, 100);

    EXPECT_TRUE(result.trades.empty());
    expectCancelledNewest(result);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, FOKCancelOldest_OwnOrderSkipped_FilledFromTheOthers) {
    useCancelOldest();
    restFor(OTHER, 100, 30);
    OrderID own = restFor(OWNER, 100, 50);
    restFor(OTHER, 101, 100);

    OrderResult result = send(OrderType::FOK, 101, 100);

    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {30, 70}));
    expectCancelledOldest(result, {own});
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{101, 30, 1}}));
}

TEST_P(SelfTradeTest, FOKCancelNewest_EnoughBeforeOwn_Filled) {
    restFor(OTHER, 100, 100);
    restFor(OWNER, 100, 50);

    OrderResult result = send(OrderType::FOK, 100, 100);

    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {100}));
    expectNoSelfTrade(result);
    expectLevel(other_, 100, 50, 1);
}

TEST_P(SelfTradeTest, FOKCancelOldest_NotEnoughWithoutOwn_KilledAndNothingCancelled) {
    useCancelOldest();
    restFor(OTHER, 100, 30);
    restFor(OWNER, 100, 50);
    Snapshot before = snapshotOf(book_);

    OrderResult result = send(OrderType::FOK, 100, 60);

    EXPECT_TRUE(result.trades.empty());
    expectNoSelfTrade(result);
    EXPECT_EQ(snapshotOf(book_), before);
}

TEST_P(SelfTradeTest, FOK_NotEnoughAndNoOwnOrders_KilledWithStatusOk) {
    restFor(OTHER, 100, 30);

    OrderResult result = send(OrderType::FOK, 100, 60);

    EXPECT_TRUE(result.trades.empty());
    expectNoSelfTrade(result);
}

TEST_P(SelfTradeTest, FOKCancelNewest_OwnOrderBeyondTheLimit_KilledWithStatusOk) {
    restFor(OTHER, 100, 30);
    restFor(OWNER, 101, 50);

    OrderResult result = send(OrderType::FOK, 100, 60);

    EXPECT_TRUE(result.trades.empty());
    expectNoSelfTrade(result);
}

// --------------------------
// Other traders
// --------------------------
TEST_P(SelfTradeTest, DifferentOwnersCross_FilledWithStatusOk) {
    restFor(OTHER, 100, 50);

    OrderResult result = send(OrderType::GTC, 100, 50);

    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {50}));
    expectNoSelfTrade(result);
    expectEmptyBook(book_);
}

TEST_P(SelfTradeTest, PolicyOfTheIncomingOwner_DoesNotAffectItsTradesWithOthers) {
    useCancelOldest();
    restFor(OTHER, 100, 50);

    OrderResult result = send(OrderType::GTC, 100, 50);

    EXPECT_EQ(tradeQuantities(result.trades), (Quantities {50}));
    expectNoSelfTrade(result);
}

TEST_P(SelfTradeTest, OwnOrdersThatDoNotCross_BothRest) {
    restFor(OWNER, 101, 50);

    OrderResult result = send(OrderType::GTC, 100, 50);

    expectNoSelfTrade(result);
    EXPECT_EQ(book_.size(), 2u);
}

// --------------------------
// Modify
// --------------------------
TEST_P(SelfTradeTest, CancelNewest_ModifyCrossesItsOwnOrder_ModifiedOrderIsGone) {
    OrderID own = restFor(OWNER, 100, 50);
    OrderID id = send(OrderType::GTC, 99, 50).orderID;

    auto result = book_.modifyOrder(id, at(100), 50);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->trades.empty());
    expectCancelledNewest(*result);
    EXPECT_EQ(book_.getOrder(id), nullptr);
    EXPECT_EQ(book_.getOrder(result->orderID), nullptr);
    EXPECT_EQ(best(side_), nullptr);
    ASSERT_NE(book_.getOrder(own), nullptr);
    EXPECT_EQ(book_.getOrder(own)->getRemainingQuantity(), 50u);
}

TEST_P(SelfTradeTest, CancelOldest_ModifyCrossesItsOwnOrder_OwnCancelledAndNewOneRests) {
    useCancelOldest();
    OrderID own = restFor(OWNER, 100, 50);
    OrderID id = send(OrderType::GTC, 99, 50).orderID;

    auto result = book_.modifyOrder(id, at(100), 50);

    ASSERT_TRUE(result.has_value());
    expectCancelledOldest(*result, {own});
    EXPECT_EQ(book_.getOrder(own), nullptr);
    ASSERT_NE(book_.getOrder(result->orderID), nullptr);
    EXPECT_EQ(book_.getOrder(result->orderID)->getPrice(), at(100));
    EXPECT_EQ(book_.getOrder(result->orderID)->getRemainingQuantity(), 50u);
    EXPECT_EQ(book_.size(), 1u);
}

TEST_P(SelfTradeTest, ModifyReducesTheQuantity_NeverMatches) {
    restFor(OWNER, 101, 50);
    OrderID id = send(OrderType::GTC, 100, 50).orderID;

    auto result = book_.modifyOrder(id, at(100), 20);

    ASSERT_TRUE(result.has_value());
    expectNoSelfTrade(*result);
    EXPECT_EQ(book_.size(), 2u);
}
