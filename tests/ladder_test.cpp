#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
#include <map>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>
#include "ladder.hpp"
#include "order.hpp"
#include "order_queue.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::detail;

using Ids = std::vector<OrderID>;
using Positions = std::vector<std::size_t>;

constexpr std::size_t NONE = Ladder::NONE;

constexpr OwnerID OWNER = 1;

// Stops a walk that never ends, which a broken after would give
constexpr std::size_t WALK_LIMIT = 1'000;

template<Side side>
constexpr Side otherSide = (side == Side::buy) ? Side::sell : Side::buy;

static Ids forwardIds(const OrderQueue& queue) {
    Ids ids;
    for (const OrderNode* node = queue.head; node != nullptr; node = node->next)
        ids.push_back(node->order.getID());
    return ids;
}

static Ids backwardIds(const OrderQueue& queue) {
    Ids ids;
    for (const OrderNode* node = queue.tail; node != nullptr; node = node->prev)
        ids.push_back(node->order.getID());
    return ids;
}

template<Side side>
static Positions walk(const Ladder& ladder) {
    Positions found;
    for (std::size_t position = ladder.best<side>(); position != NONE && found.size() < WALK_LIMIT; position = ladder.after<side>(position))
        found.push_back(position);
    return found;
}

static void place(Ladder& ladder, OrderNode* node) {
    ladder.add(ladder.positionOf(node->order.getPrice()), node);
}

// Default band is 1 to 100 with a tick of 1, so price p is at position p - 1
class LadderTest : public ::testing::Test {
protected:
    Ladder ladder_ {1, 100, 1};

    // Nodes never move, so the ladder's pointers to them stay valid
    std::deque<OrderNode> nodes_;
    OrderID nextID_ {1};

    OrderNode* makeNode(Side side, Price price, Quantity quantity) {
        nodes_.emplace_back(Order(nextID_++, OWNER, OrderType::GTC, side, price, quantity));
        return &nodes_.back();
    }

    OrderNode* addNode(Side side, Price price, Quantity quantity) {
        OrderNode* node = makeNode(side, price, quantity);
        place(ladder_, node);
        return node;
    }

    const Level& levelAt(Price price) const {
        return ladder_.level(static_cast<std::size_t>(price - 1));
    }

    template<Side side>
    void checkFirstOrderQueueHoldsOnlyIt() {
        OrderNode* node = addNode(side, 40, 100);

        const Level& level = levelAt(40);
        EXPECT_FALSE(level.queue.empty());
        EXPECT_EQ(level.queue.head, node);
        EXPECT_EQ(level.queue.tail, node);
        EXPECT_EQ(node->prev, nullptr);
        EXPECT_EQ(node->next, nullptr);
    }

    template<Side side>
    void checkFirstOrderTotals() {
        addNode(side, 40, 100);

        EXPECT_EQ(levelAt(40).data.qty_, 100u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
    }

    template<Side side>
    void checkFirstOrderPointersReferToItsLevel() {
        OrderNode* node = addNode(side, 40, 100);

        EXPECT_EQ(node->level, &levelAt(40).queue);
        EXPECT_EQ(node->levelData, &levelAt(40).data);
    }

    template<Side side>
    void checkFirstOrderMakesPriceContainedForItsSideOnly() {
        addNode(side, 40, 100);

        EXPECT_TRUE(ladder_.contains<side>(39));
        EXPECT_FALSE(ladder_.contains<otherSide<side>>(39));
        EXPECT_EQ(ladder_.best<side>(), 39u);
        EXPECT_EQ(ladder_.best<otherSide<side>>(), NONE);
    }

    template<Side side>
    void checkSecondOrderGoesToTheBack() {
        OrderNode* first = addNode(side, 40, 100);
        OrderNode* second = addNode(side, 40, 30);

        const Level& level = levelAt(40);
        EXPECT_EQ(level.queue.head, first);
        EXPECT_EQ(level.queue.tail, second);
        EXPECT_EQ(first->next, second);
        EXPECT_EQ(second->prev, first);
        EXPECT_EQ(second->next, nullptr);
    }

    template<Side side>
    void checkSecondOrderTotalsAccumulate() {
        addNode(side, 40, 100);
        addNode(side, 40, 30);

        EXPECT_EQ(levelAt(40).data.qty_, 130u);
        EXPECT_EQ(levelAt(40).data.count_, 2u);
    }

    template<Side side>
    void checkSecondOrderPointersReferToTheSameLevel() {
        OrderNode* first = addNode(side, 40, 100);
        OrderNode* second = addNode(side, 40, 30);

        EXPECT_EQ(second->level, first->level);
        EXPECT_EQ(second->levelData, first->levelData);
        EXPECT_EQ(second->level, &levelAt(40).queue);
        EXPECT_EQ(second->levelData, &levelAt(40).data);
    }

    template<Side side>
    void checkQueueOrderIsArrivalOrder() {
        Ids arrival;
        for (Quantity quantity {1}; quantity <= 5; ++quantity)
            arrival.push_back(addNode(side, 40, quantity * 10)->order.getID());

        Ids reversed(arrival.rbegin(), arrival.rend());
        EXPECT_EQ(forwardIds(levelAt(40).queue), arrival);
        EXPECT_EQ(backwardIds(levelAt(40).queue), reversed);
    }

    template<Side side>
    void checkPartiallyFilledOrderTotalsUseRemaining() {
        OrderNode* node = makeNode(side, 40, 100);
        node->order.fill(30);

        place(ladder_, node);

        EXPECT_EQ(levelAt(40).data.qty_, 70u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
    }

    template<Side side>
    void checkOrdersAtFirstAndLastPrice() {
        OrderNode* first = addNode(side, 1, 10);
        OrderNode* last = addNode(side, 100, 20);

        EXPECT_EQ(ladder_.level(0).queue.head, first);
        EXPECT_EQ(ladder_.level(99).queue.head, last);
        EXPECT_EQ(first->level, &ladder_.level(0).queue);
        EXPECT_EQ(last->levelData, &ladder_.level(99).data);
        EXPECT_TRUE(ladder_.contains<side>(0));
        EXPECT_TRUE(ladder_.contains<side>(99));
    }

    template<Side side>
    void checkFillPartially() {
        OrderNode* node = addNode(side, 40, 100);

        ladder_.fill(node, 30);

        EXPECT_EQ(levelAt(40).data.qty_, 70u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
        EXPECT_EQ(node->order.getRemainingQuantity(), 70u);
        EXPECT_EQ(levelAt(40).queue.head, node);
    }

    template<Side side>
    void checkFillFullyKeepsTheNode() {
        OrderNode* node = addNode(side, 40, 100);

        ladder_.fill(node, 100);

        EXPECT_TRUE(node->order.isFilled());
        EXPECT_EQ(levelAt(40).data.qty_, 0u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
        EXPECT_EQ(levelAt(40).queue.head, node);
        EXPECT_EQ(levelAt(40).queue.tail, node);
        EXPECT_TRUE(ladder_.contains<side>(39));
    }

    template<Side side>
    void checkFillSeveralTimes() {
        OrderNode* node = addNode(side, 40, 100);

        ladder_.fill(node, 30);
        ladder_.fill(node, 30);
        ladder_.fill(node, 25);

        EXPECT_EQ(node->order.getRemainingQuantity(), 15u);
        EXPECT_EQ(levelAt(40).data.qty_, 15u);
    }

    template<Side side>
    void checkFillOneOfTwoLeavesTheOtherUntouched() {
        OrderNode* first = addNode(side, 40, 100);
        OrderNode* second = addNode(side, 40, 60);

        ladder_.fill(second, 25);

        EXPECT_EQ(first->order.getRemainingQuantity(), 100u);
        EXPECT_EQ(second->order.getRemainingQuantity(), 35u);
        EXPECT_EQ(levelAt(40).data.qty_, 135u);
        EXPECT_EQ(levelAt(40).data.count_, 2u);
    }

    template<Side side>
    void checkFillDoesNotTouchOtherPrices() {
        OrderNode* node = addNode(side, 40, 100);
        addNode(side, 41, 50);

        ladder_.fill(node, 100);

        EXPECT_EQ(levelAt(41).data.qty_, 50u);
        EXPECT_EQ(levelAt(41).data.count_, 1u);
    }

    template<Side side>
    void checkRemoveFront() {
        OrderNode* first = addNode(side, 40, 10);
        OrderNode* second = addNode(side, 40, 20);
        OrderNode* third = addNode(side, 40, 30);

        ladder_.remove(first);

        EXPECT_EQ(levelAt(40).queue.head, second);
        EXPECT_EQ(levelAt(40).queue.tail, third);
        EXPECT_EQ(second->prev, nullptr);
        EXPECT_EQ(levelAt(40).data.qty_, 50u);
        EXPECT_EQ(levelAt(40).data.count_, 2u);
    }

    template<Side side>
    void checkRemoveMiddle() {
        OrderNode* first = addNode(side, 40, 10);
        OrderNode* second = addNode(side, 40, 20);
        OrderNode* third = addNode(side, 40, 30);

        ladder_.remove(second);

        EXPECT_EQ(levelAt(40).queue.head, first);
        EXPECT_EQ(levelAt(40).queue.tail, third);
        EXPECT_EQ(first->next, third);
        EXPECT_EQ(third->prev, first);
        EXPECT_EQ(levelAt(40).data.qty_, 40u);
        EXPECT_EQ(levelAt(40).data.count_, 2u);
    }

    template<Side side>
    void checkRemoveBack() {
        OrderNode* first = addNode(side, 40, 10);
        OrderNode* second = addNode(side, 40, 20);
        OrderNode* third = addNode(side, 40, 30);

        ladder_.remove(third);

        EXPECT_EQ(levelAt(40).queue.head, first);
        EXPECT_EQ(levelAt(40).queue.tail, second);
        EXPECT_EQ(second->next, nullptr);
        EXPECT_EQ(levelAt(40).data.qty_, 30u);
        EXPECT_EQ(levelAt(40).data.count_, 2u);
    }

    template<Side side>
    void checkRemoveLeavesTheNodeUnlinked() {
        addNode(side, 40, 10);
        OrderNode* second = addNode(side, 40, 20);
        addNode(side, 40, 30);

        ladder_.remove(second);

        EXPECT_EQ(second->prev, nullptr);
        EXPECT_EQ(second->next, nullptr);
    }

    template<Side side>
    void checkRemoveOnlyOrderEmptiesThePrice() {
        OrderNode* node = addNode(side, 40, 100);

        ladder_.remove(node);

        EXPECT_TRUE(levelAt(40).queue.empty());
        EXPECT_EQ(levelAt(40).data.qty_, 0u);
        EXPECT_EQ(levelAt(40).data.count_, 0u);
        EXPECT_FALSE(ladder_.contains<side>(39));
        EXPECT_EQ(ladder_.best<side>(), NONE);
    }

    template<Side side>
    void checkRemoveOneOfTwoKeepsThePriceContained() {
        OrderNode* first = addNode(side, 40, 10);
        addNode(side, 40, 20);

        ladder_.remove(first);

        EXPECT_TRUE(ladder_.contains<side>(39));
        EXPECT_EQ(ladder_.best<side>(), 39u);
    }

    template<Side side>
    void checkRemovePartiallyFilledTakesOffTheRemaining() {
        OrderNode* first = addNode(side, 40, 100);
        addNode(side, 40, 50);
        ladder_.fill(first, 30);

        ladder_.remove(first);

        EXPECT_EQ(levelAt(40).data.qty_, 50u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
    }

    template<Side side>
    void checkRemoveFullyFilledTakesOffNothing() {
        OrderNode* first = addNode(side, 40, 100);
        addNode(side, 40, 50);
        ladder_.fill(first, 100);

        ladder_.remove(first);

        EXPECT_EQ(levelAt(40).data.qty_, 50u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
    }

    template<Side side>
    void checkRemoveFullyFilledOnlyOrderEmptiesThePrice() {
        OrderNode* node = addNode(side, 40, 100);
        ladder_.fill(node, 100);

        ladder_.remove(node);

        EXPECT_TRUE(levelAt(40).queue.empty());
        EXPECT_EQ(levelAt(40).data.count_, 0u);
        EXPECT_FALSE(ladder_.contains<side>(39));
    }

    template<Side side>
    void checkPriceEmptiedThenUsedByTheOtherSide() {
        OrderNode* old = addNode(side, 40, 100);
        ladder_.remove(old);

        OrderNode* fresh = addNode(otherSide<side>, 40, 70);

        EXPECT_TRUE(ladder_.contains<otherSide<side>>(39));
        EXPECT_FALSE(ladder_.contains<side>(39));
        EXPECT_EQ(ladder_.best<otherSide<side>>(), 39u);
        EXPECT_EQ(ladder_.best<side>(), NONE);
        EXPECT_EQ(levelAt(40).queue.head, fresh);
        EXPECT_EQ(levelAt(40).queue.tail, fresh);
        EXPECT_EQ(levelAt(40).data.qty_, 70u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
    }

    template<Side side>
    void checkPriceEmptiedThenUsedAgainBySameSide() {
        OrderNode* old = addNode(side, 40, 100);
        ladder_.remove(old);

        OrderNode* fresh = addNode(side, 40, 70);

        EXPECT_TRUE(ladder_.contains<side>(39));
        EXPECT_EQ(levelAt(40).queue.head, fresh);
        EXPECT_EQ(levelAt(40).data.qty_, 70u);
        EXPECT_EQ(levelAt(40).data.count_, 1u);
    }
};

// --------------------------
// Price and position
// --------------------------
TEST(LadderTranslationTest, TickOfOne_FirstPrice_IsPositionZero) {
    Ladder ladder(1, 100, 1);

    EXPECT_EQ(ladder.positionOf(1), 0u);
    EXPECT_EQ(ladder.priceOf(0), 1);
}

TEST(LadderTranslationTest, TickOfOne_LastPrice_IsLastPosition) {
    Ladder ladder(1, 100, 1);

    EXPECT_EQ(ladder.positionOf(100), 99u);
    EXPECT_EQ(ladder.priceOf(99), 100);
}

TEST(LadderTranslationTest, TickOfOne_MinPriceAboveOne_PositionsStartAtMin) {
    Ladder ladder(50, 60, 1);

    EXPECT_EQ(ladder.positionOf(50), 0u);
    EXPECT_EQ(ladder.positionOf(55), 5u);
    EXPECT_EQ(ladder.positionOf(60), 10u);
}

TEST(LadderTranslationTest, TickOfOne_EveryPrice_RoundTrips) {
    Ladder ladder(1, 1'000, 1);

    for (Price price {1}; price <= 1'000; ++price) {
        std::size_t position = ladder.positionOf(price);
        ASSERT_EQ(position, static_cast<std::size_t>(price - 1)) << "price " << price;
        ASSERT_EQ(ladder.priceOf(position), price) << "price " << price;
    }
}

TEST(LadderTranslationTest, TickOfFive_FirstAndLastPrice_AreTheEndPositions) {
    Ladder ladder(10, 500, 5);

    EXPECT_EQ(ladder.positionOf(10), 0u);
    EXPECT_EQ(ladder.positionOf(500), 98u);
    EXPECT_EQ(ladder.priceOf(0), 10);
    EXPECT_EQ(ladder.priceOf(98), 500);
}

TEST(LadderTranslationTest, TickOfFive_NextPrice_IsNextPosition) {
    Ladder ladder(10, 500, 5);

    EXPECT_EQ(ladder.positionOf(15), 1u);
    EXPECT_EQ(ladder.positionOf(20), 2u);
    EXPECT_EQ(ladder.priceOf(1), 15);
}

TEST(LadderTranslationTest, TickOfFive_EveryPrice_RoundTrips) {
    Ladder ladder(10, 500, 5);

    std::size_t expected {0};
    for (Price price {10}; price <= 500; price += 5) {
        std::size_t position = ladder.positionOf(price);
        ASSERT_EQ(position, expected) << "price " << price;
        ASSERT_EQ(ladder.priceOf(position), price) << "price " << price;
        ++expected;
    }
}

TEST(LadderTranslationTest, MinPriceEqualsTick_FirstPriceIsPositionZero) {
    Ladder ladder(5, 50, 5);

    EXPECT_EQ(ladder.positionOf(5), 0u);
    EXPECT_EQ(ladder.positionOf(50), 9u);
}

TEST(LadderTranslationTest, OnePriceBand_TheOnlyPrice_IsPositionZero) {
    Ladder ladder(7, 7, 1);

    EXPECT_EQ(ladder.positionOf(7), 0u);
    EXPECT_EQ(ladder.priceOf(0), 7);
}

TEST(LadderTranslationTest, OnePriceBand_NeighbourPrices_AreNone) {
    Ladder ladder(7, 7, 1);

    EXPECT_EQ(ladder.positionOf(6), NONE);
    EXPECT_EQ(ladder.positionOf(8), NONE);
}

TEST(LadderTranslationTest, OnePriceBandWithTick_OtherPrices_AreNone) {
    Ladder ladder(10, 10, 5);

    EXPECT_EQ(ladder.positionOf(10), 0u);
    EXPECT_EQ(ladder.positionOf(5), NONE);
    EXPECT_EQ(ladder.positionOf(15), NONE);
    EXPECT_EQ(ladder.positionOf(11), NONE);
}

TEST(LadderTranslationTest, PositionOf_PriceBelowMin_ReturnsNone) {
    Ladder ladder(10, 50, 5);

    EXPECT_EQ(ladder.positionOf(9), NONE);
    EXPECT_EQ(ladder.positionOf(5), NONE);
    EXPECT_EQ(ladder.positionOf(1), NONE);
}

TEST(LadderTranslationTest, PositionOf_PriceBelowMinOnTheTick_ReturnsNone) {
    Ladder ladder(10, 50, 5);

    EXPECT_EQ(ladder.positionOf(5), NONE);
    EXPECT_EQ(ladder.positionOf(0), NONE);
}

TEST(LadderTranslationTest, PositionOf_PriceAboveMax_ReturnsNone) {
    Ladder ladder(10, 50, 5);

    EXPECT_EQ(ladder.positionOf(51), NONE);
    EXPECT_EQ(ladder.positionOf(55), NONE);
    EXPECT_EQ(ladder.positionOf(1'000), NONE);
}

TEST(LadderTranslationTest, PositionOf_PriceOffTickInsideTheBand_ReturnsNone) {
    Ladder ladder(10, 50, 5);

    for (Price price {11}; price < 50; ++price) {
        if (price % 5 == 0) continue;
        EXPECT_EQ(ladder.positionOf(price), NONE) << "price " << price;
    }
}

TEST(LadderTranslationTest, PositionOf_Zero_ReturnsNone) {
    Ladder ladder(1, 100, 1);

    EXPECT_EQ(ladder.positionOf(0), NONE);
}

TEST(LadderTranslationTest, PositionOf_NegativePrices_ReturnNone) {
    Ladder ladder(5, 50, 5);

    EXPECT_EQ(ladder.positionOf(-1), NONE);
    EXPECT_EQ(ladder.positionOf(-5), NONE);
    EXPECT_EQ(ladder.positionOf(-10), NONE);
}

TEST(LadderTranslationTest, PositionOf_NumericLimits_ReturnNone) {
    Ladder ladder(5, 50, 5);

    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::max()), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::max() - 1), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::min()), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::min() + 1), NONE);
}

TEST(LadderTranslationTest, WideTick_OnlyTheTicksAreLegal) {
    Ladder ladder(1'000'000'000, 2'000'000'000, 1'000'000'000);

    EXPECT_EQ(ladder.positionOf(1'000'000'000), 0u);
    EXPECT_EQ(ladder.positionOf(2'000'000'000), 1u);
    EXPECT_EQ(ladder.priceOf(1), 2'000'000'000);
}

TEST(LadderTranslationTest, WideTick_IllegalPricesIncludingTheLargestPrice_ReturnNone) {
    Ladder ladder(1'000'000'000, 2'000'000'000, 1'000'000'000);

    EXPECT_EQ(ladder.positionOf(0), NONE);
    EXPECT_EQ(ladder.positionOf(1'500'000'000), NONE);
    EXPECT_EQ(ladder.positionOf(999'999'999), NONE);
    EXPECT_EQ(ladder.positionOf(2'000'000'001), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::max()), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::min()), NONE);
}

TEST(LadderTranslationTest, BandReachingTheLargestPrice_LastPriceIsLegal) {
    constexpr Price MAX = std::numeric_limits<Price>::max();
    Ladder ladder(MAX - 47, MAX, 1);

    EXPECT_EQ(ladder.positionOf(MAX), 47u);
    EXPECT_EQ(ladder.positionOf(MAX - 47), 0u);
    EXPECT_EQ(ladder.priceOf(47), MAX);
    EXPECT_EQ(ladder.priceOf(0), MAX - 47);
}

TEST(LadderTranslationTest, BandReachingTheLargestPrice_EveryPriceRoundTrips) {
    constexpr Price MAX = std::numeric_limits<Price>::max();
    Ladder ladder(MAX - 47, MAX, 1);

    for (std::size_t position {0}; position < 48; ++position) {
        Price price = ladder.priceOf(position);
        ASSERT_EQ(price, MAX - 47 + static_cast<Price>(position)) << "position " << position;
        ASSERT_EQ(ladder.positionOf(price), position) << "position " << position;
    }
}

TEST(LadderTranslationTest, BandReachingTheLargestPrice_PriceBelowMin_ReturnsNone) {
    constexpr Price MAX = std::numeric_limits<Price>::max();
    Ladder ladder(MAX - 47, MAX, 1);

    EXPECT_EQ(ladder.positionOf(MAX - 48), NONE);
    EXPECT_EQ(ladder.positionOf(0), NONE);
    EXPECT_EQ(ladder.positionOf(-1), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::min()), NONE);
}

TEST(LadderTranslationTest, BandNearTheLargestPrice_PriceAboveMax_ReturnsNone) {
    Ladder ladder(2'147'480'000, 2'147'483'000, 1'000);

    EXPECT_EQ(ladder.positionOf(2'147'483'000), 3u);
    EXPECT_EQ(ladder.positionOf(2'147'483'001), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::max()), NONE);
}

TEST(LadderTranslationTest, BandNearTheLargestPrice_PriceBelowMin_ReturnsNone) {
    Ladder ladder(2'147'480'000, 2'147'483'000, 1'000);

    EXPECT_EQ(ladder.positionOf(2'147'479'000), NONE);
    EXPECT_EQ(ladder.positionOf(1'000), NONE);
    EXPECT_EQ(ladder.positionOf(std::numeric_limits<Price>::min()), NONE);
}

// --------------------------
// Add
// --------------------------
TEST_F(LadderTest, AddBuy_FirstOrder_QueueHoldsOnlyIt) {
    checkFirstOrderQueueHoldsOnlyIt<Side::buy>();
}

TEST_F(LadderTest, AddSell_FirstOrder_QueueHoldsOnlyIt) {
    checkFirstOrderQueueHoldsOnlyIt<Side::sell>();
}

TEST_F(LadderTest, AddBuy_FirstOrder_TotalsAreItsQuantityAndOne) {
    checkFirstOrderTotals<Side::buy>();
}

TEST_F(LadderTest, AddSell_FirstOrder_TotalsAreItsQuantityAndOne) {
    checkFirstOrderTotals<Side::sell>();
}

TEST_F(LadderTest, AddBuy_FirstOrder_NodePointersReferToTheLevel) {
    checkFirstOrderPointersReferToItsLevel<Side::buy>();
}

TEST_F(LadderTest, AddSell_FirstOrder_NodePointersReferToTheLevel) {
    checkFirstOrderPointersReferToItsLevel<Side::sell>();
}

TEST_F(LadderTest, AddBuy_FirstOrder_PriceIsContainedForBuysOnly) {
    checkFirstOrderMakesPriceContainedForItsSideOnly<Side::buy>();
}

TEST_F(LadderTest, AddSell_FirstOrder_PriceIsContainedForSellsOnly) {
    checkFirstOrderMakesPriceContainedForItsSideOnly<Side::sell>();
}

TEST_F(LadderTest, AddBuy_SecondOrderSamePrice_GoesToTheBack) {
    checkSecondOrderGoesToTheBack<Side::buy>();
}

TEST_F(LadderTest, AddSell_SecondOrderSamePrice_GoesToTheBack) {
    checkSecondOrderGoesToTheBack<Side::sell>();
}

TEST_F(LadderTest, AddBuy_SecondOrderSamePrice_TotalsAccumulate) {
    checkSecondOrderTotalsAccumulate<Side::buy>();
}

TEST_F(LadderTest, AddSell_SecondOrderSamePrice_TotalsAccumulate) {
    checkSecondOrderTotalsAccumulate<Side::sell>();
}

TEST_F(LadderTest, AddBuy_SecondOrderSamePrice_PointersReferToTheSameLevel) {
    checkSecondOrderPointersReferToTheSameLevel<Side::buy>();
}

TEST_F(LadderTest, AddSell_SecondOrderSamePrice_PointersReferToTheSameLevel) {
    checkSecondOrderPointersReferToTheSameLevel<Side::sell>();
}

TEST_F(LadderTest, AddBuy_FiveOrdersSamePrice_QueueOrderIsArrivalOrder) {
    checkQueueOrderIsArrivalOrder<Side::buy>();
}

TEST_F(LadderTest, AddSell_FiveOrdersSamePrice_QueueOrderIsArrivalOrder) {
    checkQueueOrderIsArrivalOrder<Side::sell>();
}

TEST_F(LadderTest, AddBuy_PartiallyFilledOrder_TotalsUseRemainingQuantity) {
    checkPartiallyFilledOrderTotalsUseRemaining<Side::buy>();
}

TEST_F(LadderTest, AddSell_PartiallyFilledOrder_TotalsUseRemainingQuantity) {
    checkPartiallyFilledOrderTotalsUseRemaining<Side::sell>();
}

TEST_F(LadderTest, AddBuy_FirstAndLastPriceOfTheBand_LandInTheEndLevels) {
    checkOrdersAtFirstAndLastPrice<Side::buy>();
}

TEST_F(LadderTest, AddSell_FirstAndLastPriceOfTheBand_LandInTheEndLevels) {
    checkOrdersAtFirstAndLastPrice<Side::sell>();
}

TEST_F(LadderTest, Add_BuyAndSellAtDifferentPrices_LevelsAreIndependent) {
    OrderNode* buy = addNode(Side::buy, 40, 100);
    OrderNode* sell = addNode(Side::sell, 60, 70);

    EXPECT_EQ(levelAt(40).queue.head, buy);
    EXPECT_EQ(levelAt(40).data.qty_, 100u);
    EXPECT_EQ(levelAt(60).queue.head, sell);
    EXPECT_EQ(levelAt(60).data.qty_, 70u);
    EXPECT_EQ(buy->level, &levelAt(40).queue);
    EXPECT_EQ(sell->level, &levelAt(60).queue);
}

TEST_F(LadderTest, Add_BuyAndSellAtDifferentPrices_EachPriceIsContainedForItsOwnSide) {
    addNode(Side::buy, 40, 100);
    addNode(Side::sell, 60, 70);

    EXPECT_TRUE(ladder_.contains<Side::buy>(39));
    EXPECT_FALSE(ladder_.contains<Side::sell>(39));
    EXPECT_TRUE(ladder_.contains<Side::sell>(59));
    EXPECT_FALSE(ladder_.contains<Side::buy>(59));
}

TEST_F(LadderTest, Add_Order_NeighbouringPricesStayEmpty) {
    addNode(Side::buy, 40, 100);

    for (Price price : {39, 41}) {
        EXPECT_TRUE(levelAt(price).queue.empty()) << "price " << price;
        EXPECT_EQ(levelAt(price).data.qty_, 0u) << "price " << price;
        EXPECT_EQ(levelAt(price).data.count_, 0u) << "price " << price;
    }
}

TEST_F(LadderTest, Add_OrdersAtTwoPrices_NodePointersAreDistinct) {
    OrderNode* low = addNode(Side::buy, 40, 100);
    OrderNode* high = addNode(Side::buy, 41, 100);

    EXPECT_NE(low->level, high->level);
    EXPECT_NE(low->levelData, high->levelData);
}

TEST_F(LadderTest, Level_EmptyPrice_QueueIsEmptyAndTotalsAreZero) {
    const Level& level = levelAt(40);

    EXPECT_TRUE(level.queue.empty());
    EXPECT_EQ(level.queue.head, nullptr);
    EXPECT_EQ(level.queue.tail, nullptr);
    EXPECT_EQ(level.data.qty_, 0u);
    EXPECT_EQ(level.data.count_, 0u);
}

TEST_F(LadderTest, Level_EveryPriceOfTheBand_StartsEmpty) {
    for (std::size_t position {0}; position < 100; ++position) {
        EXPECT_TRUE(ladder_.level(position).queue.empty()) << "position " << position;
        EXPECT_EQ(ladder_.level(position).data.count_, 0u) << "position " << position;
    }
}

// --------------------------
// Fill
// --------------------------
TEST_F(LadderTest, FillBuy_Partially_QuantityDropsAndCountStays) {
    checkFillPartially<Side::buy>();
}

TEST_F(LadderTest, FillSell_Partially_QuantityDropsAndCountStays) {
    checkFillPartially<Side::sell>();
}

TEST_F(LadderTest, FillBuy_Fully_NodeStaysInTheQueue) {
    checkFillFullyKeepsTheNode<Side::buy>();
}

TEST_F(LadderTest, FillSell_Fully_NodeStaysInTheQueue) {
    checkFillFullyKeepsTheNode<Side::sell>();
}

TEST_F(LadderTest, FillBuy_SeveralTimes_TakesEachOffTheLevel) {
    checkFillSeveralTimes<Side::buy>();
}

TEST_F(LadderTest, FillSell_SeveralTimes_TakesEachOffTheLevel) {
    checkFillSeveralTimes<Side::sell>();
}

TEST_F(LadderTest, FillBuy_OneOfTwoOrders_OtherUntouched) {
    checkFillOneOfTwoLeavesTheOtherUntouched<Side::buy>();
}

TEST_F(LadderTest, FillSell_OneOfTwoOrders_OtherUntouched) {
    checkFillOneOfTwoLeavesTheOtherUntouched<Side::sell>();
}

TEST_F(LadderTest, FillBuy_Fully_OtherPricesUntouched) {
    checkFillDoesNotTouchOtherPrices<Side::buy>();
}

TEST_F(LadderTest, FillSell_Fully_OtherPricesUntouched) {
    checkFillDoesNotTouchOtherPrices<Side::sell>();
}

TEST_F(LadderTest, Fill_PartiallyFilledBeforeAdd_TakesOffTheRemaining) {
    OrderNode* node = makeNode(Side::buy, 40, 100);
    node->order.fill(30);
    place(ladder_, node);

    ladder_.fill(node, 70);

    EXPECT_EQ(levelAt(40).data.qty_, 0u);
    EXPECT_TRUE(node->order.isFilled());
}

// --------------------------
// Remove
// --------------------------
TEST_F(LadderTest, RemoveBuy_FrontOfThree_SecondBecomesHead) {
    checkRemoveFront<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_FrontOfThree_SecondBecomesHead) {
    checkRemoveFront<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_MiddleOfThree_NeighboursLinkUp) {
    checkRemoveMiddle<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_MiddleOfThree_NeighboursLinkUp) {
    checkRemoveMiddle<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_BackOfThree_SecondBecomesTail) {
    checkRemoveBack<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_BackOfThree_SecondBecomesTail) {
    checkRemoveBack<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_MiddleOfThree_NodeHasNoLinksLeft) {
    checkRemoveLeavesTheNodeUnlinked<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_MiddleOfThree_NodeHasNoLinksLeft) {
    checkRemoveLeavesTheNodeUnlinked<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_OnlyOrder_PriceIsEmptyAndNotContained) {
    checkRemoveOnlyOrderEmptiesThePrice<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_OnlyOrder_PriceIsEmptyAndNotContained) {
    checkRemoveOnlyOrderEmptiesThePrice<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_OneOfTwo_PriceStaysContained) {
    checkRemoveOneOfTwoKeepsThePriceContained<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_OneOfTwo_PriceStaysContained) {
    checkRemoveOneOfTwoKeepsThePriceContained<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_PartiallyFilled_TakesOffOnlyTheRemaining) {
    checkRemovePartiallyFilledTakesOffTheRemaining<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_PartiallyFilled_TakesOffOnlyTheRemaining) {
    checkRemovePartiallyFilledTakesOffTheRemaining<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_FullyFilled_QuantityStaysAndCountDrops) {
    checkRemoveFullyFilledTakesOffNothing<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_FullyFilled_QuantityStaysAndCountDrops) {
    checkRemoveFullyFilledTakesOffNothing<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_FullyFilledOnlyOrder_PriceIsEmptyAndNotContained) {
    checkRemoveFullyFilledOnlyOrderEmptiesThePrice<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_FullyFilledOnlyOrder_PriceIsEmptyAndNotContained) {
    checkRemoveFullyFilledOnlyOrderEmptiesThePrice<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_OnlyOrder_SellsUseTheSamePriceAfterwards) {
    checkPriceEmptiedThenUsedByTheOtherSide<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_OnlyOrder_BuysUseTheSamePriceAfterwards) {
    checkPriceEmptiedThenUsedByTheOtherSide<Side::sell>();
}

TEST_F(LadderTest, RemoveBuy_OnlyOrder_BuysUseTheSamePriceAgain) {
    checkPriceEmptiedThenUsedAgainBySameSide<Side::buy>();
}

TEST_F(LadderTest, RemoveSell_OnlyOrder_SellsUseTheSamePriceAgain) {
    checkPriceEmptiedThenUsedAgainBySameSide<Side::sell>();
}

TEST_F(LadderTest, Remove_OrderAtOnePrice_OtherPricesAndSideUntouched) {
    OrderNode* buy = addNode(Side::buy, 40, 100);
    addNode(Side::buy, 41, 50);
    addNode(Side::sell, 60, 70);

    ladder_.remove(buy);

    EXPECT_EQ(levelAt(41).data.qty_, 50u);
    EXPECT_EQ(levelAt(60).data.qty_, 70u);
    EXPECT_TRUE(ladder_.contains<Side::buy>(40));
    EXPECT_TRUE(ladder_.contains<Side::sell>(59));
}

TEST_F(LadderTest, Remove_NodeIsNotFreed_ItsOrderIsStillReadable) {
    OrderNode* node = addNode(Side::buy, 40, 100);
    OrderID id = node->order.getID();

    ladder_.remove(node);

    EXPECT_EQ(node->order.getID(), id);
    EXPECT_EQ(node->order.getRemainingQuantity(), 100u);
}

// --------------------------
// Best and after
// --------------------------
TEST_F(LadderTest, Best_NewLadder_IsNoneForBothSides) {
    EXPECT_EQ(ladder_.best<Side::buy>(), NONE);
    EXPECT_EQ(ladder_.best<Side::sell>(), NONE);
}

TEST_F(LadderTest, Best_BuyOrdersAdded_IsTheHighestPrice) {
    addNode(Side::buy, 10, 5);
    addNode(Side::buy, 30, 5);
    addNode(Side::buy, 20, 5);

    EXPECT_EQ(ladder_.best<Side::buy>(), 29u);
}

TEST_F(LadderTest, Best_SellOrdersAdded_IsTheLowestPrice) {
    addNode(Side::sell, 70, 5);
    addNode(Side::sell, 50, 5);
    addNode(Side::sell, 60, 5);

    EXPECT_EQ(ladder_.best<Side::sell>(), 49u);
}

TEST_F(LadderTest, Best_BuysAndSells_SidesAreIndependent) {
    addNode(Side::buy, 10, 5);
    addNode(Side::buy, 30, 5);
    addNode(Side::sell, 70, 5);
    addNode(Side::sell, 50, 5);

    EXPECT_EQ(ladder_.best<Side::buy>(), 29u);
    EXPECT_EQ(ladder_.best<Side::sell>(), 49u);
}

TEST_F(LadderTest, Best_OnlyBuysAdded_SellsHaveNone) {
    addNode(Side::buy, 10, 5);

    EXPECT_EQ(ladder_.best<Side::sell>(), NONE);
}

TEST_F(LadderTest, Best_OnlySellsAdded_BuysHaveNone) {
    addNode(Side::sell, 90, 5);

    EXPECT_EQ(ladder_.best<Side::buy>(), NONE);
}

TEST_F(LadderTest, Best_BestBuyRemoved_NextHighestBecomesBest) {
    OrderNode* best = addNode(Side::buy, 30, 5);
    addNode(Side::buy, 20, 5);
    addNode(Side::buy, 10, 5);

    ladder_.remove(best);

    EXPECT_EQ(ladder_.best<Side::buy>(), 19u);
}

TEST_F(LadderTest, Best_BestSellRemoved_NextLowestBecomesBest) {
    OrderNode* best = addNode(Side::sell, 50, 5);
    addNode(Side::sell, 60, 5);
    addNode(Side::sell, 70, 5);

    ladder_.remove(best);

    EXPECT_EQ(ladder_.best<Side::sell>(), 59u);
}

TEST_F(LadderTest, Best_BestBuyFullyFilledButNotRemoved_StaysBest) {
    OrderNode* best = addNode(Side::buy, 30, 5);
    addNode(Side::buy, 20, 5);

    ladder_.fill(best, 5);

    EXPECT_EQ(ladder_.best<Side::buy>(), 29u);
}

TEST_F(LadderTest, Best_BestSellFullyFilledButNotRemoved_StaysBest) {
    OrderNode* best = addNode(Side::sell, 50, 5);
    addNode(Side::sell, 60, 5);

    ladder_.fill(best, 5);

    EXPECT_EQ(ladder_.best<Side::sell>(), 49u);
}

TEST_F(LadderTest, Best_EverySideEmptied_BothAreNoneAgain) {
    OrderNode* buy = addNode(Side::buy, 30, 5);
    OrderNode* sell = addNode(Side::sell, 70, 5);

    ladder_.remove(buy);
    ladder_.remove(sell);

    EXPECT_EQ(ladder_.best<Side::buy>(), NONE);
    EXPECT_EQ(ladder_.best<Side::sell>(), NONE);
}

TEST_F(LadderTest, After_BuySide_StepsFromHighestToLowest) {
    addNode(Side::buy, 10, 5);
    addNode(Side::buy, 30, 5);
    addNode(Side::buy, 20, 5);

    EXPECT_EQ(walk<Side::buy>(ladder_), (Positions {29, 19, 9}));
}

TEST_F(LadderTest, After_SellSide_StepsFromLowestToHighest) {
    addNode(Side::sell, 70, 5);
    addNode(Side::sell, 50, 5);
    addNode(Side::sell, 60, 5);

    EXPECT_EQ(walk<Side::sell>(ladder_), (Positions {49, 59, 69}));
}

TEST_F(LadderTest, After_BuySide_FromAPositionNotContained) {
    addNode(Side::buy, 30, 5);
    addNode(Side::buy, 10, 5);

    EXPECT_EQ(ladder_.after<Side::buy>(24), 9u);
    EXPECT_EQ(ladder_.after<Side::buy>(99), 29u);
    EXPECT_EQ(ladder_.after<Side::buy>(5), NONE);
}

TEST_F(LadderTest, After_SellSide_FromAPositionNotContained) {
    addNode(Side::sell, 50, 5);
    addNode(Side::sell, 70, 5);

    EXPECT_EQ(ladder_.after<Side::sell>(54), 69u);
    EXPECT_EQ(ladder_.after<Side::sell>(0), 49u);
    EXPECT_EQ(ladder_.after<Side::sell>(80), NONE);
}

TEST_F(LadderTest, After_BuySide_IgnoresTheSells) {
    addNode(Side::buy, 30, 5);
    addNode(Side::sell, 20, 5);
    addNode(Side::sell, 25, 5);

    EXPECT_EQ(walk<Side::buy>(ladder_), (Positions {29}));
    EXPECT_EQ(walk<Side::sell>(ladder_), (Positions {19, 24}));
}

TEST_F(LadderTest, After_FarGapOnTheSellSide_CrossesIt) {
    Ladder wide(1, 100'000, 1);
    OrderNode* near = makeNode(Side::sell, 5, 10);
    OrderNode* far = makeNode(Side::sell, 99'990, 10);
    place(wide, near);
    place(wide, far);

    EXPECT_EQ(wide.best<Side::sell>(), 4u);
    EXPECT_EQ(wide.after<Side::sell>(4), 99'989u);
    EXPECT_EQ(wide.after<Side::sell>(99'989), NONE);
}

TEST_F(LadderTest, After_FarGapOnTheBuySide_CrossesIt) {
    Ladder wide(1, 100'000, 1);
    OrderNode* near = makeNode(Side::buy, 99'990, 10);
    OrderNode* far = makeNode(Side::buy, 5, 10);
    place(wide, near);
    place(wide, far);

    EXPECT_EQ(wide.best<Side::buy>(), 99'989u);
    EXPECT_EQ(wide.after<Side::buy>(99'989), 4u);
    EXPECT_EQ(wide.after<Side::buy>(4), NONE);
}

TEST_F(LadderTest, Contains_EmptyPrices_AreNotContainedForEitherSide) {
    for (std::size_t position {0}; position < 100; ++position) {
        EXPECT_FALSE(ladder_.contains<Side::buy>(position)) << "position " << position;
        EXPECT_FALSE(ladder_.contains<Side::sell>(position)) << "position " << position;
    }
}

// --------------------------
// Tick of 5
// --------------------------
TEST(LadderTickTest, AddWithTickOfFive_OrderLandsInTheLevelOfItsPosition) {
    Ladder ladder(10, 500, 5);
    OrderNode node(Order(1, OWNER, OrderType::GTC, Side::sell, 25, 40));

    place(ladder, &node);

    EXPECT_EQ(ladder.level(3).queue.head, &node);
    EXPECT_EQ(ladder.level(3).data.qty_, 40u);
    EXPECT_EQ(node.level, &ladder.level(3).queue);
    EXPECT_EQ(ladder.best<Side::sell>(), 3u);
    EXPECT_EQ(ladder.positionOf(25), 3u);
}

TEST(LadderTickTest, AddWithTickOfFive_FirstAndLastPrice_LandInTheEndLevels) {
    Ladder ladder(10, 500, 5);
    OrderNode first(Order(1, OWNER, OrderType::GTC, Side::buy, 10, 5));
    OrderNode last(Order(2, OWNER, OrderType::GTC, Side::buy, 500, 6));

    place(ladder, &first);
    place(ladder, &last);

    EXPECT_EQ(first.level, &ladder.level(0).queue);
    EXPECT_EQ(last.level, &ladder.level(98).queue);
    EXPECT_EQ(ladder.best<Side::buy>(), 98u);
    EXPECT_EQ(ladder.after<Side::buy>(98), 0u);
}

// --------------------------
// Move
// --------------------------
TEST_F(LadderTest, MoveConstruct_LevelsKeepTheirAddresses) {
    OrderNode* buy = addNode(Side::buy, 40, 100);
    OrderNode* sell = addNode(Side::sell, 60, 70);
    const Level* buyLevel = &levelAt(40);
    const Level* sellLevel = &levelAt(60);

    Ladder moved(std::move(ladder_));

    EXPECT_EQ(&moved.level(39), buyLevel);
    EXPECT_EQ(&moved.level(59), sellLevel);
    EXPECT_EQ(buy->level, &moved.level(39).queue);
    EXPECT_EQ(sell->levelData, &moved.level(59).data);
}

TEST_F(LadderTest, MoveConstruct_TargetKeepsOrdersTotalsAndSides) {
    OrderNode* first = addNode(Side::buy, 40, 100);
    OrderNode* second = addNode(Side::buy, 40, 30);
    addNode(Side::sell, 60, 70);

    Ladder moved(std::move(ladder_));

    EXPECT_EQ(forwardIds(moved.level(39).queue), (Ids {first->order.getID(), second->order.getID()}));
    EXPECT_EQ(moved.level(39).data.qty_, 130u);
    EXPECT_EQ(moved.level(39).data.count_, 2u);
    EXPECT_EQ(moved.level(59).data.qty_, 70u);
    EXPECT_EQ(moved.best<Side::buy>(), 39u);
    EXPECT_EQ(moved.best<Side::sell>(), 59u);
    EXPECT_TRUE(moved.contains<Side::buy>(39));
    EXPECT_FALSE(moved.contains<Side::sell>(39));
}

TEST_F(LadderTest, MoveConstruct_TargetKeepsTheTranslation) {
    Ladder source(10, 500, 5);

    Ladder moved(std::move(source));

    EXPECT_EQ(moved.positionOf(10), 0u);
    EXPECT_EQ(moved.positionOf(500), 98u);
    EXPECT_EQ(moved.positionOf(12), NONE);
    EXPECT_EQ(moved.priceOf(98), 500);
}

TEST_F(LadderTest, MoveConstruct_AddThroughTheNewOwner_QueuesBehindTheExistingOrder) {
    OrderNode* first = addNode(Side::buy, 40, 100);
    Ladder moved(std::move(ladder_));

    OrderNode* second = makeNode(Side::buy, 40, 30);
    place(moved, second);

    EXPECT_EQ(moved.level(39).queue.head, first);
    EXPECT_EQ(moved.level(39).queue.tail, second);
    EXPECT_EQ(moved.level(39).data.qty_, 130u);
    EXPECT_EQ(second->level, &moved.level(39).queue);
}

TEST_F(LadderTest, MoveConstruct_FillThroughTheNewOwner_TakesOffTheLevel) {
    OrderNode* node = addNode(Side::sell, 60, 100);
    Ladder moved(std::move(ladder_));

    moved.fill(node, 40);

    EXPECT_EQ(moved.level(59).data.qty_, 60u);
    EXPECT_EQ(node->order.getRemainingQuantity(), 60u);
}

TEST_F(LadderTest, MoveConstruct_RemoveThroughTheNewOwner_EmptiesThePrice) {
    OrderNode* node = addNode(Side::buy, 40, 100);
    Ladder moved(std::move(ladder_));

    moved.remove(node);

    EXPECT_TRUE(moved.level(39).queue.empty());
    EXPECT_EQ(moved.level(39).data.count_, 0u);
    EXPECT_FALSE(moved.contains<Side::buy>(39));
    EXPECT_EQ(moved.best<Side::buy>(), NONE);
}

TEST_F(LadderTest, MoveConstruct_MovedFrom_BestIsNoneForBothSides) {
    addNode(Side::buy, 40, 100);
    addNode(Side::sell, 60, 70);

    Ladder moved(std::move(ladder_));

    EXPECT_EQ(ladder_.best<Side::buy>(), NONE);
    EXPECT_EQ(ladder_.best<Side::sell>(), NONE);
}

TEST_F(LadderTest, MoveConstruct_MovedFrom_PositionOfIsNoneForEveryPrice) {
    addNode(Side::buy, 40, 100);

    Ladder moved(std::move(ladder_));

    for (Price price : {1, 40, 60, 100, 0, -1, 101})
        EXPECT_EQ(ladder_.positionOf(price), NONE) << "price " << price;
    EXPECT_EQ(ladder_.positionOf(std::numeric_limits<Price>::max()), NONE);
    EXPECT_EQ(ladder_.positionOf(std::numeric_limits<Price>::min()), NONE);
}

TEST_F(LadderTest, MoveConstruct_MovedFromAssignedNewLadder_IsUsable) {
    addNode(Side::buy, 40, 100);
    Ladder moved(std::move(ladder_));

    ladder_ = Ladder(10, 500, 5);
    OrderNode* node = addNode(Side::sell, 25, 40);

    EXPECT_EQ(ladder_.positionOf(25), 3u);
    EXPECT_EQ(ladder_.best<Side::sell>(), 3u);
    EXPECT_EQ(ladder_.best<Side::buy>(), NONE);
    EXPECT_EQ(ladder_.level(3).queue.head, node);
    EXPECT_EQ(node->level, &ladder_.level(3).queue);
}

TEST_F(LadderTest, MoveConstruct_MovedFromAssignedNewLadder_TargetIsUntouched) {
    addNode(Side::buy, 40, 100);
    Ladder moved(std::move(ladder_));

    ladder_ = Ladder(10, 500, 5);
    addNode(Side::sell, 25, 40);

    EXPECT_EQ(moved.best<Side::buy>(), 39u);
    EXPECT_EQ(moved.best<Side::sell>(), NONE);
    EXPECT_EQ(moved.level(39).data.qty_, 100u);
}

TEST_F(LadderTest, MoveAssign_LevelsKeepTheirAddresses) {
    OrderNode* buy = addNode(Side::buy, 40, 100);
    OrderNode* sell = addNode(Side::sell, 60, 70);
    const Level* buyLevel = &levelAt(40);
    const Level* sellLevel = &levelAt(60);
    Ladder target(1, 100, 1);

    target = std::move(ladder_);

    EXPECT_EQ(&target.level(39), buyLevel);
    EXPECT_EQ(&target.level(59), sellLevel);
    EXPECT_EQ(buy->level, &target.level(39).queue);
    EXPECT_EQ(sell->levelData, &target.level(59).data);
}

TEST_F(LadderTest, MoveAssign_TargetKeepsOrdersTotalsAndSides) {
    OrderNode* first = addNode(Side::buy, 40, 100);
    OrderNode* second = addNode(Side::buy, 40, 30);
    addNode(Side::sell, 60, 70);
    Ladder target(1, 100, 1);

    target = std::move(ladder_);

    EXPECT_EQ(forwardIds(target.level(39).queue), (Ids {first->order.getID(), second->order.getID()}));
    EXPECT_EQ(target.level(39).data.qty_, 130u);
    EXPECT_EQ(target.level(59).data.count_, 1u);
    EXPECT_EQ(target.best<Side::buy>(), 39u);
    EXPECT_EQ(target.best<Side::sell>(), 59u);
}

TEST_F(LadderTest, MoveAssign_TargetOldContentIsGone) {
    Ladder target(1, 200, 1);
    OrderNode oldBuy(Order(900, OWNER, OrderType::GTC, Side::buy, 150, 5));
    OrderNode oldSell(Order(901, OWNER, OrderType::GTC, Side::sell, 20, 5));
    place(target, &oldBuy);
    place(target, &oldSell);
    addNode(Side::buy, 40, 100);

    target = std::move(ladder_);

    EXPECT_EQ(target.best<Side::buy>(), 39u);
    EXPECT_EQ(target.best<Side::sell>(), NONE);
    EXPECT_FALSE(target.contains<Side::sell>(19));
}

TEST_F(LadderTest, MoveAssign_TargetTakesTheTranslationOfTheSource) {
    Ladder target(1, 200, 1);
    Ladder source(10, 500, 5);

    target = std::move(source);

    EXPECT_EQ(target.positionOf(10), 0u);
    EXPECT_EQ(target.positionOf(500), 98u);
    EXPECT_EQ(target.positionOf(150), 28u);
    EXPECT_EQ(target.positionOf(151), NONE);
    EXPECT_EQ(target.positionOf(1), NONE);
}

TEST_F(LadderTest, MoveAssign_AddFillRemoveThroughTheNewOwner_Work) {
    OrderNode* first = addNode(Side::buy, 40, 100);
    Ladder target(1, 100, 1);
    target = std::move(ladder_);

    OrderNode* second = makeNode(Side::buy, 40, 30);
    place(target, second);
    target.fill(first, 40);
    target.remove(first);

    EXPECT_EQ(target.level(39).queue.head, second);
    EXPECT_EQ(target.level(39).data.qty_, 30u);
    EXPECT_EQ(target.level(39).data.count_, 1u);
}

TEST_F(LadderTest, MoveAssign_MovedFrom_BestIsNoneForBothSides) {
    addNode(Side::buy, 40, 100);
    addNode(Side::sell, 60, 70);
    Ladder target(1, 100, 1);

    target = std::move(ladder_);

    EXPECT_EQ(ladder_.best<Side::buy>(), NONE);
    EXPECT_EQ(ladder_.best<Side::sell>(), NONE);
}

TEST_F(LadderTest, MoveAssign_MovedFrom_PositionOfIsNoneForEveryPrice) {
    Ladder target(1, 100, 1);

    target = std::move(ladder_);

    for (Price price : {1, 40, 60, 100, 0, -1, 101})
        EXPECT_EQ(ladder_.positionOf(price), NONE) << "price " << price;
    EXPECT_EQ(ladder_.positionOf(std::numeric_limits<Price>::max()), NONE);
    EXPECT_EQ(ladder_.positionOf(std::numeric_limits<Price>::min()), NONE);
}

TEST_F(LadderTest, MoveAssign_MovedFromAssignedNewLadder_IsUsable) {
    addNode(Side::buy, 40, 100);
    Ladder target(1, 100, 1);
    target = std::move(ladder_);

    ladder_ = Ladder(5, 50, 5);
    OrderNode* node = makeNode(Side::buy, 20, 9);
    place(ladder_, node);

    EXPECT_EQ(ladder_.positionOf(20), 3u);
    EXPECT_EQ(ladder_.best<Side::buy>(), 3u);
    EXPECT_EQ(ladder_.level(3).data.qty_, 9u);
    EXPECT_EQ(node->level, &ladder_.level(3).queue);
}

TEST(LadderMoveTest, Traits_CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<Ladder>);
    EXPECT_FALSE(std::is_copy_assignable_v<Ladder>);
    EXPECT_TRUE(std::is_move_constructible_v<Ladder>);
    EXPECT_TRUE(std::is_move_assignable_v<Ladder>);
}

// --------------------------
// Random run against a model
// --------------------------
namespace {

struct Resting {
    OrderNode* node;
    Quantity remaining;
};

using ModelSide = std::map<Price, std::vector<Resting>>;

struct Model {
    ModelSide buys;
    ModelSide sells;

    ModelSide& of(Side side) {
        return (side == Side::buy) ? buys : sells;
    }
};

} // namespace

// Bands of 60 prices with a tick of 5, so prices empty and change side often
TEST(LadderRandomTest, RandomOperations_MatchAModel) {
    constexpr Price MIN_PRICE = 10;
    constexpr Price TICK = 5;
    constexpr Price MAX_PRICE = MIN_PRICE + 59 * TICK;
    constexpr std::size_t STEPS = 30'000;

    Ladder ladder(MIN_PRICE, MAX_PRICE, TICK);
    std::deque<OrderNode> nodes;
    std::vector<OrderNode*> live;
    Model model;
    std::mt19937 generator(2024);
    std::uniform_int_distribution<int> percent(0, 99);
    std::uniform_int_distribution<Price> anyPrice(0, 59);
    OrderID nextID {1};
    std::size_t sideChanges {0};
    std::map<Price, Side> lastSide;

    auto positionOfPrice = [&](Price price) {
        return static_cast<std::size_t>((price - MIN_PRICE) / TICK);
    };

    auto bestOf = [&](Side side) {
        const ModelSide& modelSide = model.of(side);
        if (modelSide.empty()) return NONE;
        Price price = (side == Side::buy) ? modelSide.rbegin()->first : modelSide.begin()->first;
        return positionOfPrice(price);
    };

    auto checkPrice = [&](Price price, std::size_t step) {
        const Level& level = ladder.level(positionOfPrice(price));
        Ids expectedIds;
        Quantity expectedQuantity {0};
        Quantity expectedCount {0};
        for (Side side : {Side::buy, Side::sell}) {
            auto it = model.of(side).find(price);
            if (it == model.of(side).end()) continue;
            for (const Resting& resting : it->second) {
                expectedIds.push_back(resting.node->order.getID());
                expectedQuantity += resting.remaining;
                ++expectedCount;
            }
        }
        EXPECT_EQ(forwardIds(level.queue), expectedIds) << "step " << step << " price " << price;
        EXPECT_EQ(level.data.qty_, expectedQuantity) << "step " << step << " price " << price;
        EXPECT_EQ(level.data.count_, expectedCount) << "step " << step << " price " << price;
        EXPECT_EQ(ladder.contains<Side::buy>(positionOfPrice(price)), model.buys.contains(price)) << "step " << step;
        EXPECT_EQ(ladder.contains<Side::sell>(positionOfPrice(price)), model.sells.contains(price)) << "step " << step;
        EXPECT_EQ(ladder.best<Side::buy>(), bestOf(Side::buy)) << "step " << step;
        EXPECT_EQ(ladder.best<Side::sell>(), bestOf(Side::sell)) << "step " << step;
    };

    for (std::size_t step {0}; step < STEPS; ++step) {
        int choice = percent(generator);
        Price touched {0};

        if (choice < 40 || live.empty()) {
            Side side = (percent(generator) < 50) ? Side::buy : Side::sell;
            Price price = MIN_PRICE + anyPrice(generator) * TICK;
            bool crosses = (side == Side::buy)
                ? (!model.sells.empty() && model.sells.begin()->first <= price)
                : (!model.buys.empty() && model.buys.rbegin()->first >= price);
            if (crosses) continue;

            Quantity quantity = static_cast<Quantity>(percent(generator)) + 1;
            nodes.emplace_back(Order(nextID++, OWNER, OrderType::GTC, side, price, quantity));
            OrderNode* node = &nodes.back();
            place(ladder, node);
            model.of(side)[price].push_back({node, quantity});
            live.push_back(node);
            touched = price;

            auto last = lastSide.find(price);
            if (last != lastSide.end() && last->second != side) ++sideChanges;
            lastSide[price] = side;

            EXPECT_EQ(node->level, &ladder.level(positionOfPrice(price)).queue) << "step " << step;
            EXPECT_EQ(node->levelData, &ladder.level(positionOfPrice(price)).data) << "step " << step;
        } else if (choice < 70) {
            OrderNode* node = live[std::uniform_int_distribution<std::size_t>(0, live.size() - 1)(generator)];
            Quantity remaining = node->order.getRemainingQuantity();
            if (remaining == 0) continue;

            Quantity quantity = std::uniform_int_distribution<Quantity>(1, remaining)(generator);
            ladder.fill(node, quantity);
            for (Resting& resting : model.of(node->order.getSide())[node->order.getPrice()])
                if (resting.node == node) resting.remaining -= quantity;
            touched = node->order.getPrice();
        } else {
            std::size_t index = std::uniform_int_distribution<std::size_t>(0, live.size() - 1)(generator);
            OrderNode* node = live[index];
            live[index] = live.back();
            live.pop_back();

            touched = node->order.getPrice();
            ModelSide& modelSide = model.of(node->order.getSide());
            std::vector<Resting>& queue = modelSide[touched];
            queue.erase(std::find_if(queue.begin(), queue.end(),
                                     [&](const Resting& resting) { return resting.node == node; }));
            if (queue.empty()) modelSide.erase(touched);

            ladder.remove(node);
        }

        checkPrice(touched, step);
        if (HasFailure()) FAIL() << "stopped at step " << step;
    }

    // The run must have moved prices between sides
    EXPECT_GT(sideChanges, 50u);

    Positions expectedBuys;
    for (auto it = model.buys.rbegin(); it != model.buys.rend(); ++it)
        expectedBuys.push_back(positionOfPrice(it->first));
    Positions expectedSells;
    for (const auto& [price, queue] : model.sells)
        expectedSells.push_back(positionOfPrice(price));
    EXPECT_EQ(walk<Side::buy>(ladder), expectedBuys);
    EXPECT_EQ(walk<Side::sell>(ladder), expectedSells);
}
