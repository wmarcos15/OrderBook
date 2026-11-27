#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>
#include "order.hpp"
#include "order_queue.hpp"
#include "price_levels.hpp"
#include "queue_test_utils.hpp"
#include "test_utils.hpp"
#include "types.hpp"
#include "window.hpp"

using namespace orderbook;
using namespace orderbook::detail;
using namespace orderbook::test;

using Cursor = PriceLevels::Cursor;

constexpr std::size_t NONE = Window::NONE;

constexpr OwnerID OWNER = 1;

// 10'000 prices, with the window in the middle so that both maps get used
constexpr Price TICK = 3;
constexpr Price MIN_PRICE = 3;
constexpr Price MAX_PRICE = 30'000;
constexpr std::size_t SIZE = 8;
constexpr Price SPAN = 8;
constexpr Price LAST_INDEX = 9'999;

// The index of the first position of the window
constexpr Price FIRST = 5'000;

// Stops a walk that never ends, which a broken after would give
constexpr std::size_t WALK_LIMIT = 20'000;

static Cursor bestAt(const PriceLevels& levels, Side side) {
    return (side == Side::buy) ? levels.best<Side::buy>() : levels.best<Side::sell>();
}

static Cursor afterAt(const PriceLevels& levels, Side side, const Cursor& cursor) {
    return (side == Side::buy) ? levels.after<Side::buy>(cursor) : levels.after<Side::sell>(cursor);
}

static std::vector<Cursor> walk(const PriceLevels& levels, Side side) {
    std::vector<Cursor> found;
    for (Cursor cursor = bestAt(levels, side); cursor.level != nullptr && found.size() < WALK_LIMIT; cursor = afterAt(levels, side, cursor))
        found.push_back(cursor);
    return found;
}

static Prices walkedPrices(const PriceLevels& levels, Side side) {
    Prices found;
    for (const Cursor& cursor : walk(levels, side))
        found.push_back(cursor.price);
    return found;
}

// Every test runs once per side. The indices are written as the buys see them: index 0 is
// the lowest price of the band, and the sells get them mirrored. For both sides the higher
// the index the better, and the window holds the indices FIRST to FIRST + 7.
// The side being tested rests below the window, the other one above it.
class PriceLevelsTest : public ::testing::TestWithParam<Side> {
protected:
    Side side_ {GetParam()};
    Side other_ {otherSide(side_)};
    PriceLevels levels_ {TICK, MIN_PRICE, MAX_PRICE, SIZE};

    // A deque, so that the nodes never move
    std::deque<OrderNode> nodes_;
    OrderID nextID_ {1};

    PriceLevelsTest() {
        placeWindow(FIRST);
    }

    Price priceAt(Price index) const {
        return (side_ == Side::buy) ? MIN_PRICE + index * TICK : MAX_PRICE - index * TICK;
    }

    Prices pricesAt(const Prices& indices) const {
        Prices prices;
        for (Price index : indices)
            prices.push_back(priceAt(index));
        return prices;
    }

    // Positions follow the price order, so the sells see them reversed
    std::size_t positionAt(Price index) const {
        return static_cast<std::size_t>((side_ == Side::buy) ? index - FIRST : FIRST + SPAN - 1 - index);
    }

    static bool isInside(Price index) {
        return index >= FIRST && index < FIRST + SPAN;
    }

    void placeWindow(Price firstIndex) {
        levels_.placeWindowAt(priceAt((side_ == Side::buy) ? firstIndex : firstIndex + SPAN - 1));
    }

    OrderNode* makeNode(Side side, Price index, Quantity quantity) {
        nodes_.emplace_back(Order(nextID_++, OWNER, OrderType::GTC, side, priceAt(index), quantity));
        return &nodes_.back();
    }

    OrderNode* addNode(Side side, Price index, Quantity quantity) {
        OrderNode* node = makeNode(side, index, quantity);
        levels_.add(node);
        return node;
    }

    // In both directions, and every order points to its level
    void expectLevel(const PriceLevels& levels, Side side, Price index, const IDs& ids, Quantity quantity) const {
        const Level* level = levels.levelOf(side, priceAt(index));
        ASSERT_NE(level, nullptr) << "index " << index;
        EXPECT_EQ(forwardIDs(level->queue), ids);
        EXPECT_EQ(backwardIDs(level->queue), IDs(ids.rbegin(), ids.rend()));
        EXPECT_EQ(level->data.qty_, quantity);
        EXPECT_EQ(level->data.count_, ids.size());
        for (const OrderNode* node = level->queue.head; node != nullptr; node = node->next) {
            EXPECT_EQ(node->level, &level->queue);
            EXPECT_EQ(node->levelData, &level->data);
        }
    }

    void expectLevel(Side side, Price index, const IDs& ids, Quantity quantity) const {
        expectLevel(levels_, side, index, ids, quantity);
    }

    void expectNoLevel(Side side, Price index) const {
        EXPECT_EQ(levels_.levelOf(side, priceAt(index)), nullptr) << "index " << index;
    }

    // The indices from the best to the worst, with the cursors telling where each one is
    void expectWalk(const PriceLevels& levels, Side side, const Prices& indices) const {
        std::vector<Cursor> cursors = walk(levels, side);
        ASSERT_EQ(cursors.size(), indices.size());
        for (std::size_t i {0}; i < indices.size(); ++i) {
            EXPECT_EQ(cursors[i].price, priceAt(indices[i])) << "step " << i;
            EXPECT_EQ(cursors[i].level, levels.levelOf(side, cursors[i].price)) << "step " << i;
            EXPECT_EQ(cursors[i].position, isInside(indices[i]) ? positionAt(indices[i]) : NONE) << "step " << i;
        }
        EXPECT_TRUE(levels.outsideRuleHolds());
    }

    void expectWalk(Side side, const Prices& indices) const {
        expectWalk(levels_, side, indices);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(PriceLevelsTest);

// --------------------------
// Routing
// --------------------------
TEST_P(PriceLevelsTest, Add_EveryPriceOfTheWindow_RestsInsideIt) {
    for (Price index {FIRST}; index < FIRST + SPAN; ++index) {
        OrderNode* node = addNode(side_, index, 10);

        Cursor best = bestAt(levels_, side_);
        EXPECT_EQ(best.price, priceAt(index));
        EXPECT_EQ(best.position, positionAt(index)) << "index " << index;
        EXPECT_EQ(node->level, &best.level->queue);
        levels_.remove(node);
    }
}

TEST_P(PriceLevelsTest, Add_PricesAroundTheWindow_OnlyTheEdgesAreInside) {
    addNode(side_, FIRST - 1, 10);
    addNode(side_, FIRST, 10);
    addNode(other_, FIRST + SPAN - 1, 10);
    addNode(other_, FIRST + SPAN, 10);

    expectWalk(side_, {FIRST, FIRST - 1});
    expectWalk(other_, {FIRST + SPAN - 1, FIRST + SPAN});
}

TEST_P(PriceLevelsTest, Add_FirstAndLastPriceOfTheBand_RestOutside) {
    addNode(side_, 0, 10);
    addNode(other_, LAST_INDEX, 20);

    expectLevel(side_, 0, {1}, 10);
    expectLevel(other_, LAST_INDEX, {2}, 20);
    expectWalk(side_, {0});
    expectWalk(other_, {LAST_INDEX});
}

// --------------------------
// Add outside
// --------------------------
TEST_P(PriceLevelsTest, Add_FirstOrderOutside_LevelHoldsOnlyIt) {
    OrderNode* node = addNode(side_, 100, 50);

    const Level* level = levels_.levelOf(side_, priceAt(100));
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->queue.head, node);
    EXPECT_EQ(level->queue.tail, node);
    expectLevel(side_, 100, {1}, 50);
    expectNoLevel(other_, 100);
    expectWalk(side_, {100});
    EXPECT_EQ(bestAt(levels_, other_).level, nullptr);
}

TEST_P(PriceLevelsTest, Add_SeveralOutsideAtOnePrice_QueuedByArrival) {
    for (Quantity quantity : {10, 20, 30, 40, 50})
        addNode(side_, 100, quantity);

    expectLevel(side_, 100, {1, 2, 3, 4, 5}, 150);
    expectWalk(side_, {100});
}

TEST_P(PriceLevelsTest, Add_PartiallyFilledOutside_TotalUsesWhatIsLeft) {
    OrderNode* node = makeNode(side_, 100, 100);
    node->order.fill(30);

    levels_.add(node);

    expectLevel(side_, 100, {1}, 70);
}

TEST_P(PriceLevelsTest, Add_OneOrderAtEveryOutsidePrice_NoTwoShareALevel) {
    std::set<const Level*> found;
    IDs ids;
    for (Price index {100}; index < 110; ++index) {
        addNode(side_, index, 10 + index);
        found.insert(levels_.levelOf(side_, priceAt(index)));
        ids.push_back(static_cast<OrderID>(index - 99));
    }

    EXPECT_EQ(found.size(), 10u);
    for (Price index {100}; index < 110; ++index)
        expectLevel(side_, index, {ids[static_cast<std::size_t>(index - 100)]}, 10 + index);
}

TEST_P(PriceLevelsTest, Add_OneOrderOnEachSideOutside_SidesAreIndependent) {
    addNode(side_, 100, 100);
    addNode(other_, 6'000, 70);

    expectLevel(side_, 100, {1}, 100);
    expectLevel(other_, 6'000, {2}, 70);
    expectNoLevel(other_, 100);
    expectNoLevel(side_, 6'000);
    expectWalk(side_, {100});
    expectWalk(other_, {6'000});
}

TEST_P(PriceLevelsTest, Add_InsideAndOutsideSameSide_EachGoesToItsPlace) {
    OrderNode* inside = addNode(side_, FIRST + 2, 10);
    OrderNode* outside = addNode(side_, 100, 20);

    EXPECT_EQ(inside->level, &levels_.levelOf(side_, priceAt(FIRST + 2))->queue);
    EXPECT_EQ(outside->level, &levels_.levelOf(side_, priceAt(100))->queue);
    expectLevel(side_, FIRST + 2, {1}, 10);
    expectLevel(side_, 100, {2}, 20);
    EXPECT_NE(inside->level, outside->level);
}

// --------------------------
// Fill
// --------------------------
TEST_P(PriceLevelsTest, Fill_SeveralTimesOutside_EachComesOffTheLevel) {
    OrderNode* node = addNode(side_, 100, 100);

    PriceLevels::fill(node, 30);
    expectLevel(side_, 100, {1}, 70);
    EXPECT_EQ(node->order.getRemainingQuantity(), 70u);

    PriceLevels::fill(node, 55);
    expectLevel(side_, 100, {1}, 15);
    EXPECT_EQ(node->order.getRemainingQuantity(), 15u);
}

TEST_P(PriceLevelsTest, Fill_FullyOutside_OrderAndLevelStay) {
    OrderNode* node = addNode(side_, 100, 100);

    PriceLevels::fill(node, 100);

    EXPECT_TRUE(node->order.isFilled());
    expectLevel(side_, 100, {1}, 0);
    expectWalk(side_, {100});
}

TEST_P(PriceLevelsTest, Fill_OneOfTwoOutside_OtherOrderAndLevelsUntouched) {
    OrderNode* first = addNode(side_, 100, 100);
    OrderNode* second = addNode(side_, 100, 60);
    addNode(side_, 200, 50);
    addNode(side_, FIRST, 40);

    PriceLevels::fill(second, 25);

    EXPECT_EQ(first->order.getRemainingQuantity(), 100u);
    EXPECT_EQ(second->order.getRemainingQuantity(), 35u);
    expectLevel(side_, 100, {1, 2}, 135);
    expectLevel(side_, 200, {3}, 50);
    expectLevel(side_, FIRST, {4}, 40);
}

TEST_P(PriceLevelsTest, Fill_InsideTheWindow_ComesOffTheLevel) {
    OrderNode* node = addNode(side_, FIRST + 3, 100);

    PriceLevels::fill(node, 40);

    expectLevel(side_, FIRST + 3, {1}, 60);
}

// --------------------------
// Remove outside
// --------------------------
TEST_P(PriceLevelsTest, Remove_EachOfThreeOutside_OthersKeepTheirOrder) {
    for (std::size_t removed {0}; removed < 3; ++removed) {
        std::vector<OrderNode*> added;
        IDs left;
        for (Quantity quantity : {10, 20, 30}) {
            added.push_back(addNode(side_, 100, quantity));
            left.push_back(added.back()->order.getID());
        }

        levels_.remove(added[removed]);

        left.erase(left.begin() + static_cast<std::ptrdiff_t>(removed));
        expectLevel(side_, 100, left, 60 - 10 * static_cast<Quantity>(removed + 1));
        EXPECT_EQ(added[removed]->prev, nullptr);
        EXPECT_EQ(added[removed]->next, nullptr);
        expectWalk(side_, {100});

        added.erase(added.begin() + static_cast<std::ptrdiff_t>(removed));
        for (OrderNode* node : added)
            levels_.remove(node);
    }
}

TEST_P(PriceLevelsTest, Remove_OnlyOrderOutside_LevelIsErased) {
    OrderNode* node = addNode(side_, 100, 100);
    addNode(side_, 200, 50);

    levels_.remove(node);

    expectNoLevel(side_, 100);
    expectLevel(side_, 200, {2}, 50);
    expectWalk(side_, {200});
}

TEST_P(PriceLevelsTest, Remove_PartiallyFilledOutside_OnlyWhatIsLeftComesOff) {
    OrderNode* first = addNode(side_, 100, 100);
    addNode(side_, 100, 50);
    PriceLevels::fill(first, 30);

    levels_.remove(first);

    expectLevel(side_, 100, {2}, 50);
}

TEST_P(PriceLevelsTest, Remove_FullyFilledOutside_OnlyTheCountDrops) {
    OrderNode* first = addNode(side_, 100, 100);
    OrderNode* second = addNode(side_, 100, 50);
    PriceLevels::fill(first, 100);
    PriceLevels::fill(second, 50);

    levels_.remove(first);
    expectLevel(side_, 100, {2}, 0);

    levels_.remove(second);
    expectNoLevel(side_, 100);
}

TEST_P(PriceLevelsTest, Remove_OutsideAndInside_EachLeavesTheOtherUntouched) {
    OrderNode* outside = addNode(side_, 100, 100);
    addNode(side_, FIRST + 1, 50);
    addNode(other_, 6'000, 70);

    levels_.remove(outside);

    expectNoLevel(side_, 100);
    expectLevel(side_, FIRST + 1, {2}, 50);
    expectLevel(other_, 6'000, {3}, 70);
    expectWalk(side_, {FIRST + 1});
    expectWalk(other_, {6'000});
}

TEST_P(PriceLevelsTest, Remove_OnlyOrderOutside_EitherSideCanUseThePriceAgain) {
    // Inside the window, where both sides can rest
    for (Price index : {FIRST + 1, FIRST + 5}) {
        for (Side next : {side_, other_}) {
            OrderNode* old = addNode(side_, index, 100);
            levels_.remove(old);

            OrderNode* fresh = addNode(next, index, 70);

            expectLevel(next, index, {fresh->order.getID()}, 70);
            expectNoLevel(otherSide(next), index);
            levels_.remove(fresh);
        }
    }
}

TEST_P(PriceLevelsTest, Remove_EveryOrder_NothingIsLeft) {
    std::vector<OrderNode*> added;
    for (Price index : {0, 100, 100, FIRST - 1, FIRST, FIRST + 7})
        added.push_back(addNode(side_, index, 10));
    for (Price index : {FIRST + 3, FIRST + 8, 9'000, 9'000})
        added.push_back(addNode(other_, index, 10));

    for (OrderNode* node : added)
        levels_.remove(node);

    EXPECT_EQ(bestAt(levels_, side_).level, nullptr);
    EXPECT_EQ(bestAt(levels_, other_).level, nullptr);
    EXPECT_TRUE(levels_.outsideRuleHolds());
}

// --------------------------
// Stable levels
// --------------------------
TEST_P(PriceLevelsTest, Add_FarEntriesComeAndGo_NodesKeepPointingToTheirLevel) {
    OrderNode* first = addNode(side_, 2'000, 10);
    OrderNode* second = addNode(side_, 2'000, 20);
    OrderNode* far = addNode(other_, 7'000, 30);
    const Level* level = levels_.levelOf(side_, priceAt(2'000));
    const Level* farLevel = levels_.levelOf(other_, priceAt(7'000));

    std::vector<OrderNode*> others;
    for (Price index {0}; index < 1'500; ++index)
        others.push_back(addNode(side_, index, 5));
    for (Price index {7'001}; index < 8'500; ++index)
        others.push_back(addNode(other_, index, 5));
    for (std::size_t i {0}; i < others.size(); i += 2)
        levels_.remove(others[i]);

    EXPECT_EQ(levels_.levelOf(side_, priceAt(2'000)), level);
    EXPECT_EQ(levels_.levelOf(other_, priceAt(7'000)), farLevel);
    EXPECT_EQ(first->level, &level->queue);
    EXPECT_EQ(second->levelData, &level->data);
    EXPECT_EQ(far->level, &farLevel->queue);
    expectLevel(side_, 2'000, {1, 2}, 30);
    expectLevel(other_, 7'000, {3}, 30);

    for (std::size_t i {1}; i < others.size(); i += 2)
        levels_.remove(others[i]);

    EXPECT_EQ(levels_.levelOf(side_, priceAt(2'000)), level);
    expectWalk(side_, {2'000});
    expectWalk(other_, {7'000});
}

TEST_P(PriceLevelsTest, Remove_EmptiedFarLevel_ItIsErasedAndCanBeRebuilt) {
    OrderNode* node = addNode(side_, 100, 10);
    addNode(side_, 200, 10);
    levels_.remove(node);

    expectNoLevel(side_, 100);
    expectWalk(side_, {200});

    OrderNode* again = addNode(side_, 100, 25);

    expectLevel(side_, 100, {3}, 25);
    expectWalk(side_, {200, 100});
    EXPECT_EQ(again->level, &levels_.levelOf(side_, priceAt(100))->queue);
}

// --------------------------
// Level of
// --------------------------
TEST_P(PriceLevelsTest, LevelOf_OtherSide_IsNull) {
    addNode(side_, FIRST + 2, 10);
    addNode(side_, 100, 10);
    addNode(other_, FIRST + 4, 10);
    addNode(other_, 6'000, 10);

    for (Price index : {FIRST + 2, 100}) {
        EXPECT_NE(levels_.levelOf(side_, priceAt(index)), nullptr);
        expectNoLevel(other_, index);
    }
    for (Price index : {FIRST + 4, 6'000}) {
        EXPECT_NE(levels_.levelOf(other_, priceAt(index)), nullptr);
        expectNoLevel(side_, index);
    }
}

TEST_P(PriceLevelsTest, LevelOf_PriceWithoutOrders_IsNull) {
    addNode(side_, 100, 10);

    for (Price index : {FIRST, FIRST + 7, 99, 101, 0, 6'000, LAST_INDEX}) {
        expectNoLevel(side_, index);
        expectNoLevel(other_, index);
    }
    // Off the tick and out of the band
    for (Side side : {Side::buy, Side::sell}) {
        EXPECT_EQ(levels_.levelOf(side, priceAt(100) + 1), nullptr);
        EXPECT_EQ(levels_.levelOf(side, 0), nullptr);
        EXPECT_EQ(levels_.levelOf(side, MAX_PRICE + TICK), nullptr);
    }
}

TEST_P(PriceLevelsTest, LevelOf_RemovedOrder_IsNull) {
    OrderNode* inside = addNode(side_, FIRST + 1, 10);
    OrderNode* outside = addNode(side_, 100, 10);

    levels_.remove(inside);
    levels_.remove(outside);

    expectNoLevel(side_, FIRST + 1);
    expectNoLevel(side_, 100);
}

// --------------------------
// Best and after
// --------------------------
TEST_P(PriceLevelsTest, Walk_WindowThenMap_InExactPriceOrder) {
    for (Price index : {4'000, 5'000, 3, 5'003, 4'999, 0, 5'007})
        addNode(side_, index, 5);
    for (Price index : {6'000, 5'005, 5'008, 9'999, 5'001, 5'002})
        addNode(other_, index, 5);

    expectWalk(side_, {5'007, 5'003, 5'000, 4'999, 4'000, 3, 0});
    expectWalk(other_, {5'001, 5'002, 5'005, 5'008, 6'000, 9'999});
}

TEST_P(PriceLevelsTest, Walk_WindowEmpty_StartsInTheMap) {
    for (Price index : {300, 100, 200})
        addNode(side_, index, 5);
    for (Price index : {7'000, 6'000})
        addNode(other_, index, 5);

    expectWalk(side_, {300, 200, 100});
    expectWalk(other_, {6'000, 7'000});
}

TEST_P(PriceLevelsTest, Walk_MapEmpty_EndsWithTheWindow) {
    for (Price index : {5'001, 5'006, 5'000})
        addNode(side_, index, 5);
    for (Price index : {5'007, 5'002})
        addNode(other_, index, 5);

    expectWalk(side_, {5'006, 5'001, 5'000});
    expectWalk(other_, {5'002, 5'007});
}

TEST_P(PriceLevelsTest, Walk_NothingResting_BestIsNone) {
    for (Side side : {side_, other_}) {
        Cursor best = bestAt(levels_, side);
        EXPECT_EQ(best.level, nullptr);
        EXPECT_EQ(best.position, NONE);
        EXPECT_TRUE(walk(levels_, side).empty());
    }
}

TEST_P(PriceLevelsTest, Walk_EmptyWindowAndMapAfterRemoving_BestIsNone) {
    OrderNode* inside = addNode(side_, FIRST + 4, 5);
    OrderNode* outside = addNode(side_, 100, 5);

    levels_.remove(inside);
    levels_.remove(outside);

    EXPECT_EQ(bestAt(levels_, side_).level, nullptr);
    EXPECT_EQ(bestAt(levels_, other_).level, nullptr);
}

TEST_P(PriceLevelsTest, After_PriceWithoutOrders_IsTheNextOne) {
    addNode(side_, 4'000, 5);
    addNode(side_, 100, 5);
    addNode(side_, FIRST + 6, 5);
    addNode(side_, FIRST + 1, 5);

    // From the map
    Cursor fromMap {priceAt(3'000), nullptr, NONE};
    EXPECT_EQ(afterAt(levels_, side_, fromMap).price, priceAt(100));
    Cursor beyond {priceAt(50), nullptr, NONE};
    EXPECT_EQ(afterAt(levels_, side_, beyond).level, nullptr);

    // From the window
    Cursor fromWindow {priceAt(FIRST + 4), nullptr, positionAt(FIRST + 4)};
    EXPECT_EQ(afterAt(levels_, side_, fromWindow).price, priceAt(FIRST + 1));
    Cursor lastPosition {priceAt(FIRST), nullptr, positionAt(FIRST)};
    Cursor next = afterAt(levels_, side_, lastPosition);
    EXPECT_EQ(next.price, priceAt(4'000));
    EXPECT_EQ(next.position, NONE);
}

TEST_P(PriceLevelsTest, After_LastOfTheWindowWithNothingOutside_IsNone) {
    addNode(side_, FIRST + 2, 5);

    Cursor last = bestAt(levels_, side_);

    EXPECT_EQ(afterAt(levels_, side_, last).level, nullptr);
}

TEST_P(PriceLevelsTest, After_LastOfTheMap_IsNone) {
    addNode(side_, 100, 5);

    Cursor last = bestAt(levels_, side_);

    ASSERT_EQ(last.position, NONE);
    EXPECT_EQ(afterAt(levels_, side_, last).level, nullptr);
}

TEST_P(PriceLevelsTest, Walk_BothSides_AreIndependent) {
    OrderNode* node = addNode(side_, FIRST + 1, 5);
    OrderNode* otherNode = addNode(other_, FIRST + 5, 5);
    addNode(other_, 7'000, 5);

    expectWalk(side_, {FIRST + 1});
    expectWalk(other_, {FIRST + 5, 7'000});

    levels_.remove(node);
    EXPECT_EQ(bestAt(levels_, side_).level, nullptr);
    expectWalk(other_, {FIRST + 5, 7'000});

    levels_.remove(otherNode);
    expectWalk(other_, {7'000});
}

TEST_P(PriceLevelsTest, Walk_RemovingTheBestOnTheWay_SweepsInOrder) {
    std::map<Price, std::vector<OrderNode*>> resting;
    for (Price index : {5'004, 5'004, 5'001, 5'000, 4'500, 4'500, 4'999, 100, 0})
        resting[index].push_back(addNode(side_, index, 5));

    Prices swept;
    for (Cursor cursor = bestAt(levels_, side_); cursor.level != nullptr; ) {
        Cursor current = cursor;
        Price index = (side_ == Side::buy) ? (current.price - MIN_PRICE) / TICK : (MAX_PRICE - current.price) / TICK;
        swept.push_back(index);
        for (OrderNode* node : resting[index])
            levels_.remove(node);

        // The level is gone, and the cursor still says where to go on
        EXPECT_EQ(levels_.levelOf(side_, current.price), nullptr);
        cursor = afterAt(levels_, side_, current);
    }

    EXPECT_EQ(swept, (Prices {5'004, 5'001, 5'000, 4'999, 4'500, 100, 0}));
    EXPECT_EQ(bestAt(levels_, side_).level, nullptr);
    EXPECT_TRUE(levels_.outsideRuleHolds());
}

TEST_P(PriceLevelsTest, Walk_RemovingTheBestEachTime_NextOneTakesOver) {
    OrderNode* inside = addNode(side_, FIRST + 2, 5);
    OrderNode* edge = addNode(side_, FIRST, 5);
    OrderNode* edgeOutside = addNode(side_, FIRST - 1, 5);
    OrderNode* deep = addNode(side_, 10, 5);

    levels_.remove(inside);
    expectWalk(side_, {FIRST, FIRST - 1, 10});

    levels_.remove(edge);
    expectWalk(side_, {FIRST - 1, 10});

    levels_.remove(edgeOutside);
    expectWalk(side_, {10});

    levels_.remove(deep);
    EXPECT_EQ(bestAt(levels_, side_).level, nullptr);
}

// --------------------------
// Outside rule
// --------------------------
TEST_P(PriceLevelsTest, OutsideRule_EveryState_Holds) {
    EXPECT_TRUE(levels_.outsideRuleHolds());

    OrderNode* low = addNode(side_, FIRST - 1, 5);
    EXPECT_TRUE(levels_.outsideRuleHolds());
    OrderNode* high = addNode(other_, FIRST + SPAN, 5);
    EXPECT_TRUE(levels_.outsideRuleHolds());
    OrderNode* edge = addNode(other_, FIRST, 5);
    EXPECT_TRUE(levels_.outsideRuleHolds());

    levels_.remove(edge);
    levels_.remove(high);
    levels_.remove(low);
    EXPECT_TRUE(levels_.outsideRuleHolds());
}

TEST_P(PriceLevelsTest, OutsideRule_WindowPlacedOverOutsideOrders_DoesNotHold) {
    OrderNode* low = addNode(side_, 4'990, 5);
    OrderNode* high = addNode(other_, 5'100, 5);
    EXPECT_TRUE(levels_.outsideRuleHolds());

    // Covers 4'990
    placeWindow(4'985);
    EXPECT_FALSE(levels_.outsideRuleHolds());

    // Covers 5'100
    placeWindow(5'095);
    EXPECT_FALSE(levels_.outsideRuleHolds());

    placeWindow(FIRST);
    EXPECT_TRUE(levels_.outsideRuleHolds());
    levels_.remove(low);
    levels_.remove(high);
}

TEST_P(PriceLevelsTest, PlaceWindowAt_EmptyWindow_PricesInsideChange) {
    placeWindow(2'000);

    addNode(side_, 2'003, 5);
    addNode(side_, 1'999, 5);
    addNode(other_, 2'007, 5);
    addNode(other_, 2'008, 5);

    Cursor best = bestAt(levels_, side_);
    EXPECT_EQ(best.price, priceAt(2'003));
    EXPECT_NE(best.position, NONE);
    EXPECT_EQ(afterAt(levels_, side_, best).position, NONE);
    EXPECT_EQ(bestAt(levels_, other_).price, priceAt(2'007));
    EXPECT_NE(bestAt(levels_, other_).position, NONE);
    EXPECT_TRUE(levels_.outsideRuleHolds());
}

// --------------------------
// Largest price
// --------------------------
TEST(PriceLevelsLargestPriceTest, WindowPastTheLargestPrice_RuleHoldsWithoutOverflow) {
    constexpr Price MAX = std::numeric_limits<Price>::max();

    // 100 prices up to the largest, and a window that ends 4 ticks past it
    PriceLevels levels(1, MAX - 99, MAX, SIZE);
    levels.placeWindowAt(MAX - 3);
    OrderNode bid(Order(1, OWNER, OrderType::GTC, Side::buy, MAX - 50, 10));
    OrderNode deepBid(Order(2, OWNER, OrderType::GTC, Side::buy, MAX - 99, 10));
    OrderNode inside(Order(3, OWNER, OrderType::GTC, Side::sell, MAX, 10));
    levels.add(&bid);
    levels.add(&deepBid);
    levels.add(&inside);

    EXPECT_TRUE(levels.outsideRuleHolds());
    EXPECT_EQ(walkedPrices(levels, Side::buy), (Prices {MAX - 50, MAX - 99}));
    EXPECT_EQ(walkedPrices(levels, Side::sell), (Prices {MAX}));

    levels.remove(&bid);
    levels.remove(&inside);
    levels.remove(&deepBid);
    EXPECT_TRUE(levels.outsideRuleHolds());
}

TEST(PriceLevelsLargestPriceTest, WindowRoundedPastTheBand_RuleHolds) {
    constexpr Price MAX = std::numeric_limits<Price>::max();

    // 10 prices round up to a window of 16 that ends past the largest price
    PriceLevels levels(1, MAX - 9, MAX, 64);
    OrderNode ask(Order(1, OWNER, OrderType::GTC, Side::sell, MAX, 10));
    levels.add(&ask);

    EXPECT_TRUE(levels.outsideRuleHolds());
    EXPECT_EQ(walkedPrices(levels, Side::sell), (Prices {MAX}));
    levels.remove(&ask);
}

// --------------------------
// Small windows
// --------------------------
TEST_P(PriceLevelsTest, SmallBand_WindowIsSizedToIt_NothingIsOutside) {
    // 8 prices, and a window of 64 asked for
    PriceLevels small(5, 10, 45, 64);
    std::deque<OrderNode> smallNodes;
    for (Price price {10}; price <= 45; price += 5) {
        smallNodes.emplace_back(Order(static_cast<OrderID>(price), OWNER, OrderType::GTC, side_, price, 10));
        small.add(&smallNodes.back());
    }

    std::vector<Cursor> cursors = walk(small, side_);
    ASSERT_EQ(cursors.size(), 8u);
    for (const Cursor& cursor : cursors)
        EXPECT_NE(cursor.position, NONE) << "price " << cursor.price;
    EXPECT_EQ(cursors.front().price, (side_ == Side::buy) ? 45 : 10);
    EXPECT_EQ(cursors.back().price, (side_ == Side::buy) ? 10 : 45);
    EXPECT_TRUE(small.outsideRuleHolds());
}

TEST_P(PriceLevelsTest, SmallBand_NotAPowerOfTwo_WindowCoversAllOfIt) {
    // 5 prices round up to a window of 8
    PriceLevels small(1, 1, 5, 64);
    std::deque<OrderNode> smallNodes;
    for (Price price {1}; price <= 5; ++price) {
        smallNodes.emplace_back(Order(static_cast<OrderID>(price), OWNER, OrderType::GTC, side_, price, 10));
        small.add(&smallNodes.back());
    }

    for (const Cursor& cursor : walk(small, side_))
        EXPECT_NE(cursor.position, NONE) << "price " << cursor.price;
    EXPECT_EQ(walk(small, side_).size(), 5u);
}

TEST_P(PriceLevelsTest, WindowOfOne_OnlyItsPriceIsInside) {
    PriceLevels one(5, 5, 500, 1);
    one.placeWindowAt(250);
    Price outside = (side_ == Side::buy) ? 100 : 400;
    Price otherOutside = (side_ == Side::buy) ? 400 : 100;
    OrderNode inside(Order(1, OWNER, OrderType::GTC, side_, 250, 10));
    OrderNode away(Order(2, OWNER, OrderType::GTC, side_, outside, 10));
    OrderNode otherAway(Order(3, OWNER, OrderType::GTC, other_, otherOutside, 10));
    one.add(&inside);
    one.add(&away);
    one.add(&otherAway);

    std::vector<Cursor> cursors = walk(one, side_);
    ASSERT_EQ(cursors.size(), 2u);
    EXPECT_EQ(cursors[0].price, 250);
    EXPECT_EQ(cursors[0].position, 0u);
    EXPECT_EQ(cursors[1].price, outside);
    EXPECT_EQ(cursors[1].position, NONE);
    EXPECT_EQ(walkedPrices(one, other_), (Prices {otherOutside}));
    EXPECT_TRUE(one.outsideRuleHolds());

    one.remove(&inside);
    one.remove(&away);
    one.remove(&otherAway);
    EXPECT_EQ(bestAt(one, side_).level, nullptr);
}

// --------------------------
// Copy and move
// --------------------------
TEST(PriceLevelsTraitsTest, CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<PriceLevels>);
    EXPECT_FALSE(std::is_copy_assignable_v<PriceLevels>);
    EXPECT_TRUE(std::is_move_constructible_v<PriceLevels>);
    EXPECT_TRUE(std::is_move_assignable_v<PriceLevels>);
}

// Orders inside and outside the window on both sides, then moved away from levels_
class MovedPriceLevelsTest : public PriceLevelsTest {
protected:
    PriceLevels target_ {TICK, MIN_PRICE, MAX_PRICE, SIZE};
    OrderNode old_ {Order(900, OWNER, OrderType::GTC, Side::sell, MIN_PRICE + 100 * TICK, 5)};
    OrderNode* insideFirst_ {nullptr};
    OrderNode* insideSecond_ {nullptr};
    OrderNode* outsideFirst_ {nullptr};
    OrderNode* outsideSecond_ {nullptr};
    OrderNode* otherInside_ {nullptr};
    OrderNode* otherOutside_ {nullptr};
    const Level* insideLevel_ {nullptr};
    const Level* outsideLevel_ {nullptr};
    const Level* otherOutsideLevel_ {nullptr};

    void SetUp() override {
        // To be replaced by the move
        target_.add(&old_);

        insideFirst_ = addNode(side_, FIRST + 1, 100);
        insideSecond_ = addNode(side_, FIRST + 1, 30);
        outsideFirst_ = addNode(side_, 100, 50);
        outsideSecond_ = addNode(side_, 100, 20);
        otherInside_ = addNode(other_, FIRST + 5, 70);
        otherOutside_ = addNode(other_, 6'000, 80);
        insideLevel_ = levels_.levelOf(side_, priceAt(FIRST + 1));
        outsideLevel_ = levels_.levelOf(side_, priceAt(100));
        otherOutsideLevel_ = levels_.levelOf(other_, priceAt(6'000));
    }

    // The orders point to their level, so it must not have moved
    void expectHoldsEverything(const PriceLevels& levels) const {
        expectLevel(levels, side_, FIRST + 1, {1, 2}, 130);
        expectLevel(levels, side_, 100, {3, 4}, 70);
        expectLevel(levels, other_, FIRST + 5, {5}, 70);
        expectLevel(levels, other_, 6'000, {6}, 80);
        EXPECT_EQ(levels.levelOf(side_, priceAt(FIRST + 1)), insideLevel_);
        EXPECT_EQ(levels.levelOf(side_, priceAt(100)), outsideLevel_);
        EXPECT_EQ(levels.levelOf(other_, priceAt(6'000)), otherOutsideLevel_);
        EXPECT_EQ(outsideFirst_->level, &outsideLevel_->queue);
        EXPECT_EQ(otherOutside_->levelData, &otherOutsideLevel_->data);
        expectWalk(levels, side_, {FIRST + 1, 100});
        expectWalk(levels, other_, {FIRST + 5, 6'000});
    }

    void expectMovedFrom(const PriceLevels& levels) const {
        for (Side side : {side_, other_}) {
            EXPECT_EQ(bestAt(levels, side).level, nullptr);
            EXPECT_EQ(bestAt(levels, side).position, NONE);
            for (Price index : {FIRST + 1, FIRST + 5, 100, 6'000, 0})
                EXPECT_EQ(levels.levelOf(side, priceAt(index)), nullptr);
        }
        EXPECT_TRUE(levels.outsideRuleHolds());
    }

    void expectUsable(PriceLevels& levels) {
        OrderNode* fresh = makeNode(side_, 100, 10);
        levels.add(fresh);
        PriceLevels::fill(outsideFirst_, 20);
        expectLevel(levels, side_, 100, {3, 4, 7}, 60);

        levels.remove(outsideFirst_);
        levels.remove(fresh);
        levels.remove(outsideSecond_);
        expectNoLevel(side_, 100);

        levels.remove(insideFirst_);
        levels.remove(insideSecond_);
        levels.remove(otherInside_);
        levels.remove(otherOutside_);
        EXPECT_EQ(bestAt(levels, side_).level, nullptr);
        EXPECT_EQ(bestAt(levels, other_).level, nullptr);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(MovedPriceLevelsTest);

TEST_P(MovedPriceLevelsTest, MoveConstruct_TargetHoldsEverythingAndStaysUsable) {
    PriceLevels target(std::move(levels_));

    expectHoldsEverything(target);
    expectMovedFrom(levels_);
    expectUsable(target);
}

TEST_P(MovedPriceLevelsTest, MoveAssign_TargetLosesItsOldContentAndStaysUsable) {
    target_ = std::move(levels_);

    expectHoldsEverything(target_);
    expectMovedFrom(levels_);
    expectUsable(target_);
}

TEST_P(MovedPriceLevelsTest, MoveAssign_OldOutsideLevelOfTheTarget_IsGone) {
    ASSERT_NE(target_.levelOf(Side::sell, MIN_PRICE + 100 * TICK), nullptr);

    PriceLevels source(TICK, MIN_PRICE, MAX_PRICE, SIZE);
    target_ = std::move(source);

    EXPECT_EQ(target_.levelOf(Side::sell, MIN_PRICE + 100 * TICK), nullptr);
    EXPECT_EQ(bestAt(target_, Side::sell).level, nullptr);
}

TEST_P(MovedPriceLevelsTest, MovedFrom_AssignedANewOne_IsUsableAndTargetIsUntouched) {
    target_ = std::move(levels_);

    levels_ = PriceLevels(TICK, MIN_PRICE, MAX_PRICE, SIZE);
    placeWindow(FIRST);
    OrderNode* node = addNode(side_, 100, 40);
    OrderNode* inside = addNode(side_, FIRST + 3, 40);

    EXPECT_EQ(node->level, &levels_.levelOf(side_, priceAt(100))->queue);
    EXPECT_EQ(walkedPrices(levels_, side_), pricesAt({FIRST + 3, 100}));
    EXPECT_EQ(bestAt(levels_, other_).level, nullptr);
    expectHoldsEverything(target_);
    levels_.remove(node);
    levels_.remove(inside);
}

TEST_P(MovedPriceLevelsTest, MovedFromTwice_StaysEmpty) {
    target_ = std::move(levels_);
    PriceLevels again(std::move(levels_));

    expectMovedFrom(again);
    expectMovedFrom(levels_);
    expectHoldsEverything(target_);
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

// Index i is the price MIN_PRICE + i * TICK. Buys rest below the window and sells above it,
// and both can rest inside it
TEST(PriceLevelsRandomTest, RandomOperations_MatchAModel) {
    constexpr std::size_t STEPS = 10'000;

    PriceLevels levels(TICK, MIN_PRICE, MAX_PRICE, SIZE);
    Price windowBase = MIN_PRICE + FIRST * TICK;
    levels.placeWindowAt(windowBase);
    Price windowLast = windowBase + (SPAN - 1) * TICK;

    std::deque<OrderNode> nodes;
    std::vector<OrderNode*> live;
    Model model;
    std::mt19937 generator(2025);
    std::uniform_int_distribution<int> percent(0, 99);
    OrderID nextID {1};
    std::size_t addedInside {0};
    std::size_t addedOutside {0};

    auto priceOfIndex = [&](Price index) {
        return MIN_PRICE + index * TICK;
    };

    auto isInside = [&](Price price) {
        return price >= windowBase && price <= windowLast;
    };

    // Half of the prices are close to the window, so that levels are reused
    auto randomIndex = [&](Side side) {
        bool isLower = side == Side::buy;
        Price low = isLower ? 0 : FIRST + SPAN;
        Price high = isLower ? FIRST - 1 : LAST_INDEX;
        if (percent(generator) < 12) return static_cast<Price>(FIRST + std::uniform_int_distribution<Price>(0, SPAN - 1)(generator));
        if (percent(generator) < 50) {
            Price near = std::uniform_int_distribution<Price>(0, 20)(generator);
            return isLower ? FIRST - 1 - near : FIRST + SPAN + near;
        }
        return std::uniform_int_distribution<Price>(low, high)(generator);
    };

    auto expectedPrices = [&](Side side) {
        Prices prices;
        const ModelSide& modelSide = model.of(side);
        // The buys have the highest price first and the sells the lowest
        if (side == Side::buy) {
            for (auto it = modelSide.rbegin(); it != modelSide.rend(); ++it)
                prices.push_back(it->first);
        } else {
            for (const auto& [price, queue] : modelSide)
                prices.push_back(price);
        }
        return prices;
    };

    auto checkSide = [&](Side side, std::size_t step) {
        std::vector<Cursor> cursors = walk(levels, side);
        Prices expected = expectedPrices(side);
        ASSERT_EQ(cursors.size(), expected.size()) << "step " << step;
        for (std::size_t i {0}; i < expected.size(); ++i) {
            const Cursor& cursor = cursors[i];
            ASSERT_EQ(cursor.price, expected[i]) << "step " << step;
            ASSERT_EQ(cursor.level, levels.levelOf(side, cursor.price)) << "step " << step;
            ASSERT_EQ(cursor.position == NONE, !isInside(cursor.price)) << "step " << step;

            IDs expectedIDs;
            Quantity expectedQuantity {0};
            for (const Resting& resting : model.of(side)[cursor.price]) {
                expectedIDs.push_back(resting.node->order.getID());
                expectedQuantity += resting.remaining;
            }
            ASSERT_EQ(forwardIDs(cursor.level->queue), expectedIDs) << "step " << step;
            ASSERT_EQ(cursor.level->data.qty_, expectedQuantity) << "step " << step;
            ASSERT_EQ(cursor.level->data.count_, expectedIDs.size()) << "step " << step;
        }
    };

    for (std::size_t step {0}; step < STEPS; ++step) {
        int choice = percent(generator);
        Price touched {0};

        if (choice < 40 || live.empty()) {
            Side side = (percent(generator) < 50) ? Side::buy : Side::sell;
            Price price = priceOfIndex(randomIndex(side));
            // A price holds one side at a time
            if (model.of(otherSide(side)).contains(price)) continue;

            Quantity quantity = static_cast<Quantity>(percent(generator)) + 1;
            nodes.emplace_back(Order(nextID++, OWNER, OrderType::GTC, side, price, quantity));
            OrderNode* node = &nodes.back();
            levels.add(node);
            model.of(side)[price].push_back({node, quantity});
            live.push_back(node);
            touched = price;
            ++(isInside(price) ? addedInside : addedOutside);
        } else if (choice < 60) {
            OrderNode* node = live[std::uniform_int_distribution<std::size_t>(0, live.size() - 1)(generator)];
            Quantity remaining = node->order.getRemainingQuantity();
            if (remaining == 0) continue;

            Quantity quantity = std::uniform_int_distribution<Quantity>(1, remaining)(generator);
            PriceLevels::fill(node, quantity);
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

            levels.remove(node);
        }

        // The touched price is on one side only
        for (Side side : {Side::buy, Side::sell})
            EXPECT_EQ(levels.levelOf(side, touched) != nullptr, model.of(side).contains(touched)) << "step " << step;
        EXPECT_TRUE(levels.outsideRuleHolds()) << "step " << step;
        checkSide(Side::buy, step);
        checkSide(Side::sell, step);
        if (HasFailure()) FAIL() << "stopped at step " << step;
    }

    // Most of the run happens outside the window
    EXPECT_GT(addedOutside, 3 * addedInside);
    EXPECT_GT(addedInside, 100u);
}
