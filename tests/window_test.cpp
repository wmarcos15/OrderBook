#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
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
#include "queue_test_utils.hpp"
#include "test_utils.hpp"
#include "types.hpp"
#include "window.hpp"

using namespace orderbook;
using namespace orderbook::detail;
using namespace orderbook::test;

using Positions = std::vector<std::size_t>;

constexpr std::size_t NONE = Window::NONE;

constexpr OwnerID OWNER = 1;

constexpr Price MAX = std::numeric_limits<Price>::max();
constexpr Price MIN = std::numeric_limits<Price>::min();

// Stops a walk that never ends, which a broken after would give
constexpr std::size_t WALK_LIMIT = 1'000;

static bool containsAt(const Window& window, Side side, std::size_t position) {
    return (side == Side::buy) ? window.contains<Side::buy>(position) : window.contains<Side::sell>(position);
}

static std::size_t bestAt(const Window& window, Side side) {
    return (side == Side::buy) ? window.best<Side::buy>() : window.best<Side::sell>();
}

static std::size_t afterAt(const Window& window, Side side, std::size_t position) {
    return (side == Side::buy) ? window.after<Side::buy>(position) : window.after<Side::sell>(position);
}

static Positions walk(const Window& window, Side side) {
    Positions found;
    for (std::size_t position = bestAt(window, side); position != NONE && found.size() < WALK_LIMIT; position = afterAt(window, side, position))
        found.push_back(position);
    return found;
}

static void place(Window& window, OrderNode* node) {
    window.add(window.positionOf(node->order.getPrice()), node);
}

// Every test runs once per side. The positions are written as the buys see them, and
// the sells get them mirrored. Base tick 102: positions 0 to 7 are slots 6, 7, 0 to 5.
class WindowTest : public ::testing::TestWithParam<Side> {
protected:
    static constexpr Price TICK = 5;
    static constexpr Price BASE_PRICE = 510;
    static constexpr std::size_t SIZE = 8;

    Side side_ {GetParam()};
    Side other_ {otherSide(side_)};
    Window window_ {TICK, BASE_PRICE, SIZE};

    // A deque, so that the nodes never move
    std::deque<OrderNode> nodes_;
    OrderID nextID_ {1};

    std::size_t at(std::size_t position) const {
        return (side_ == Side::buy) ? position : SIZE - 1 - position;
    }

    Positions positionsAt(const Positions& positions) const {
        Positions mirrored;
        for (std::size_t position : positions)
            mirrored.push_back(at(position));
        return mirrored;
    }

    OrderNode* makeNode(Side side, std::size_t position, Quantity quantity) {
        Price price = BASE_PRICE + static_cast<Price>(at(position)) * TICK;
        nodes_.emplace_back(Order(nextID_++, OWNER, OrderType::GTC, side, price, quantity));
        return &nodes_.back();
    }

    OrderNode* addNode(Side side, std::size_t position, Quantity quantity) {
        OrderNode* node = makeNode(side, position, quantity);
        place(window_, node);
        return node;
    }

    const Level& levelAt(std::size_t position) const {
        return window_.level(at(position));
    }

    // In both directions, and every order points to its level
    void expectLevel(const Window& window, std::size_t position, const IDs& ids, Quantity quantity) const {
        const Level& level = window.level(at(position));
        EXPECT_EQ(forwardIDs(level.queue), ids);
        EXPECT_EQ(backwardIDs(level.queue), IDs(ids.rbegin(), ids.rend()));
        EXPECT_EQ(level.data.qty_, quantity);
        EXPECT_EQ(level.data.count_, ids.size());
        for (const OrderNode* node = level.queue.head; node != nullptr; node = node->next) {
            EXPECT_EQ(node->level, &level.queue);
            EXPECT_EQ(node->levelData, &level.data);
        }
    }

    void expectLevel(std::size_t position, const IDs& ids, Quantity quantity) const {
        expectLevel(window_, position, ids, quantity);
    }

    void expectOnlySide(Side side, std::size_t position) const {
        EXPECT_TRUE(containsAt(window_, side, at(position)));
        EXPECT_FALSE(containsAt(window_, otherSide(side), at(position)));
    }

    void expectNeitherSide(std::size_t position) const {
        EXPECT_FALSE(containsAt(window_, side_, at(position)));
        EXPECT_FALSE(containsAt(window_, other_, at(position)));
    }
};

INSTANTIATE_FOR_BOTH_SIDES(WindowTest);

// --------------------------
// Price and position
// --------------------------
static void expectRoundTrip(const Window& window, Price basePrice, Price tick, std::size_t size) {
    ASSERT_EQ(window.size(), size);
    ASSERT_EQ(window.basePrice(), basePrice);
    for (std::size_t position {0}; position < size; ++position) {
        Price price = basePrice + static_cast<Price>(position) * tick;
        ASSERT_EQ(window.priceOf(position), price);
        ASSERT_EQ(window.positionOf(price), position);
    }
}

// slotOf is private, so the slot is read from the distance to position 0
static void expectSlots(const Window& window, std::int64_t baseTick) {
    std::int64_t size = static_cast<std::int64_t>(window.size());
    for (std::int64_t position {0}; position < size; ++position) {
        std::int64_t expected = ((baseTick + position) & (size - 1)) - (baseTick & (size - 1));
        ASSERT_EQ(&window.level(static_cast<std::size_t>(position)) - &window.level(0), expected) << "position " << position;
    }
}

static void expectNone(const Window& window, const std::vector<Price>& prices) {
    for (Price price : prices)
        EXPECT_EQ(window.positionOf(price), NONE) << "price " << price;
}

TEST(WindowTranslationTest, LegalPrices_EachHasItsPosition) {
    expectRoundTrip(Window(1, 1, 8), 1, 1, 8);                  // low tick
    expectRoundTrip(Window(5, 510, 8), 510, 5, 8);              // the wrap is inside the window
    expectRoundTrip(Window(1, 1'000, 16'384), 1'000, 1, 16'384);
    expectRoundTrip(Window(1, MAX - 10, 8), MAX - 10, 1, 8);
    expectRoundTrip(Window(1, MAX - 7, 8), MAX - 7, 1, 8);      // the top is MAX itself
    expectRoundTrip(Window(1'000, 2'147'476'000, 8), 2'147'476'000, 1'000, 8);
    expectRoundTrip(Window(1'000, 1'000, 1), 1'000, 1'000, 1);
    expectRoundTrip(Window(7, 7, 1), 7, 7, 1);
    expectRoundTrip(Window(1, 5, 1), 5, 1, 1);
}

TEST(WindowTranslationTest, Slots_FollowTheTickAndWrap) {
    expectSlots(Window(1, 1, 8), 1);
    expectSlots(Window(5, 510, 8), 102);
    expectSlots(Window(1, 1'000, 16'384), 1'000);
    expectSlots(Window(1, 16'000, 16'384), 16'000);
    expectSlots(Window(1, MAX - 10, 8), MAX - 10);
    expectSlots(Window(1, MAX - 7, 8), MAX - 7);
    expectSlots(Window(1'000, 2'147'476'000, 8), 2'147'476);
    expectSlots(Window(5, 5, 1), 1);
}

TEST(WindowTranslationTest, PricesOutsideTheWindow_AreNone) {
    expectNone(Window(1, 100, 8), {99, 108, 0, -1, 1'000, MAX, MIN, MIN + 1});
    expectNone(Window(5, 510, 8), {505, 550, 0, -5, 5, MAX, MIN});
    expectNone(Window(5, 10, 1), {5, 15, 0, MAX, MIN});
    expectNone(Window(1, 1, 16'384), {0, -1, 16'385, MAX, MIN});
    expectNone(Window(1, MAX - 10, 8), {MAX - 11, MAX - 2, MAX, 0, -1, MIN});
    expectNone(Window(1, MAX - 7, 8), {MAX - 8, 1, 0, MIN});
    expectNone(Window(1'000, 2'147'476'000, 8), {2'147'475'000, 1'000, 0, -1'000, MAX, MIN});
}

TEST(WindowTranslationTest, PricesOffTheTick_AreNone) {
    Window window(5, 100, 8);
    for (Price price {101}; price < 135; ++price)
        EXPECT_EQ(window.positionOf(price) == NONE, price % 5 != 0) << "price " << price;

    expectNone(Window(1'000, 2'147'476'000, 8), {2'147'476'001, 2'147'479'500, 2'147'482'999});
    expectNone(window, {-3, -1, 98});
}

// --------------------------
// New window
// --------------------------
TEST_P(WindowTest, NewWindow_EveryPositionIsEmpty) {
    EXPECT_EQ(bestAt(window_, side_), NONE);
    EXPECT_EQ(bestAt(window_, other_), NONE);
    for (std::size_t position {0}; position < SIZE; ++position) {
        EXPECT_TRUE(levelAt(position).queue.empty()) << "position " << position;
        expectLevel(position, {}, 0);
        expectNeitherSide(position);
    }
}

// --------------------------
// Place at
// --------------------------
TEST_P(WindowTest, PlaceAt_EmptyWindow_PositionZeroIsTheNewBase) {
    window_.placeAt(1'000);

    expectRoundTrip(window_, 1'000, TICK, SIZE);
    expectSlots(window_, 200);
    expectNone(window_, {BASE_PRICE, 995, 1'040, 1'045});
}

TEST_P(WindowTest, PlaceAt_BaseOnAWrapPoint_SlotsStartAtTheFirstLevel) {
    window_.placeAt(40);

    expectRoundTrip(window_, 40, TICK, SIZE);
    expectSlots(window_, 8);
}

TEST_P(WindowTest, PlaceAt_SameBaseOrLowerBase_Works) {
    window_.placeAt(BASE_PRICE);
    expectRoundTrip(window_, BASE_PRICE, TICK, SIZE);

    window_.placeAt(TICK);
    expectRoundTrip(window_, TICK, TICK, SIZE);
    expectSlots(window_, 1);
}

TEST_P(WindowTest, PlaceAt_NearTheLargestPrice_TopIsInside) {
    Window high(1, 1, 8);

    high.placeAt(MAX - 7);

    expectRoundTrip(high, MAX - 7, 1, 8);
    expectSlots(high, MAX - 7);
}

TEST_P(WindowTest, PlaceAt_AfterTheOrdersLeft_NewPositionsAreEmpty) {
    window_.remove(addNode(side_, 3, 100));
    window_.remove(addNode(other_, 6, 100));

    window_.placeAt(1'000);

    for (std::size_t position {0}; position < SIZE; ++position)
        expectLevel(position, {}, 0);
    EXPECT_EQ(bestAt(window_, side_), NONE);
    EXPECT_EQ(bestAt(window_, other_), NONE);
}

TEST_P(WindowTest, PlaceAt_ThenAdd_OrdersGoToTheNewPositions) {
    window_.placeAt(1'000);
    OrderNode node(Order(1, OWNER, OrderType::GTC, side_, 1'015, 40));
    OrderNode otherNode(Order(2, OWNER, OrderType::GTC, other_, 1'030, 10));

    place(window_, &node);
    place(window_, &otherNode);

    EXPECT_EQ(node.level, &window_.level(3).queue);
    EXPECT_EQ(otherNode.level, &window_.level(6).queue);
    EXPECT_EQ(walk(window_, side_), (Positions {3}));
    EXPECT_EQ(walk(window_, other_), (Positions {6}));

    window_.remove(&node);
    window_.remove(&otherNode);
}

// --------------------------
// Holds
// --------------------------
TEST_P(WindowTest, Holds_NodeOfTheWindow_IsTrueAtEveryPosition) {
    for (std::size_t position {0}; position < SIZE; ++position) {
        OrderNode* node = addNode(side_, position, 10);
        EXPECT_TRUE(window_.holds(node)) << "position " << position;
    }
}

TEST_P(WindowTest, Holds_NodeOfAnotherWindowOrOfNoLevel_IsFalse) {
    Window other(TICK, BASE_PRICE, SIZE);
    OrderNode inOther(Order(1, OWNER, OrderType::GTC, side_, BASE_PRICE, 10));
    place(other, &inOther);
    OrderNode unplaced(Order(2, OWNER, OrderType::GTC, side_, BASE_PRICE, 10));
    Level elsewhere;
    OrderNode onTheStack(Order(3, OWNER, OrderType::GTC, side_, BASE_PRICE, 10));
    onTheStack.levelData = &elsewhere.data;

    EXPECT_FALSE(window_.holds(&inOther));
    EXPECT_FALSE(window_.holds(&unplaced));
    EXPECT_FALSE(window_.holds(&onTheStack));

    other.remove(&inOther);
}

TEST_P(WindowTest, Holds_MovedFromWindow_IsFalse) {
    OrderNode* node = addNode(side_, 2, 10);
    Window target(std::move(window_));

    EXPECT_TRUE(target.holds(node));
    EXPECT_FALSE(window_.holds(node));
}

// --------------------------
// Add
// --------------------------
TEST_P(WindowTest, Add_FirstOrder_LevelHoldsOnlyIt) {
    OrderNode* node = addNode(side_, 3, 100);

    EXPECT_FALSE(levelAt(3).queue.empty());
    EXPECT_EQ(levelAt(3).queue.head, node);
    EXPECT_EQ(levelAt(3).queue.tail, node);
    expectLevel(3, {1}, 100);
    expectOnlySide(side_, 3);
    EXPECT_EQ(bestAt(window_, side_), at(3));
    EXPECT_EQ(bestAt(window_, other_), NONE);
}

TEST_P(WindowTest, Add_AtEachPosition_NodeUsesTheLevelOfItsPosition) {
    for (std::size_t position {0}; position < SIZE; ++position) {
        OrderNode* node = addNode(side_, position, 10);

        EXPECT_EQ(node->level, &window_.level(at(position)).queue) << "position " << position;
        EXPECT_EQ(node->levelData, &window_.level(at(position)).data) << "position " << position;
        expectLevel(position, {node->order.getID()}, 10);
        window_.remove(node);
    }
}

TEST_P(WindowTest, Add_OneOrderAtEveryPosition_NoTwoShareALevel) {
    std::set<const Level*> levels;
    IDs ids;
    for (std::size_t position {0}; position < SIZE; ++position) {
        addNode(side_, position, 10 + static_cast<Quantity>(position));
        levels.insert(&levelAt(position));
        ids.push_back(position + 1);
    }

    EXPECT_EQ(levels.size(), SIZE);
    for (std::size_t position {0}; position < SIZE; ++position)
        expectLevel(position, {ids[position]}, 10 + static_cast<Quantity>(position));
    EXPECT_EQ(walk(window_, side_).size(), SIZE);
}

TEST_P(WindowTest, Add_AcrossTheWrap_NeighboursStayEmpty) {
    addNode(side_, 1, 100);
    addNode(side_, 2, 50);

    for (std::size_t position : {0, 3}) {
        expectLevel(position, {}, 0);
        expectNeitherSide(position);
    }
}

TEST_P(WindowTest, Add_SeveralAtOnePrice_QueuedByArrival) {
    for (Quantity quantity : {10, 20, 30, 40, 50})
        addNode(side_, 2, quantity);

    expectLevel(2, {1, 2, 3, 4, 5}, 150);
    EXPECT_EQ(walk(window_, side_), positionsAt({2}));
}

TEST_P(WindowTest, Add_PartiallyFilledOrder_TotalUsesWhatIsLeft) {
    OrderNode* node = makeNode(side_, 2, 100);
    node->order.fill(30);

    place(window_, node);

    expectLevel(2, {1}, 70);
}

TEST_P(WindowTest, Add_FirstAndLastPosition_LandInTheEndLevels) {
    addNode(side_, 0, 10);
    addNode(side_, 7, 20);

    expectLevel(0, {1}, 10);
    expectLevel(7, {2}, 20);
    EXPECT_EQ(walk(window_, side_), positionsAt({7, 0}));
}

TEST_P(WindowTest, Add_OneOrderOnEachSide_LevelsAndSidesAreIndependent) {
    addNode(side_, 1, 100);
    addNode(other_, 2, 70);

    expectLevel(1, {1}, 100);
    expectLevel(2, {2}, 70);
    expectOnlySide(side_, 1);
    expectOnlySide(other_, 2);
    EXPECT_EQ(bestAt(window_, side_), at(1));
    EXPECT_EQ(bestAt(window_, other_), at(2));
}

// --------------------------
// Fill
// --------------------------
TEST_P(WindowTest, Fill_SeveralTimes_EachComesOffTheLevel) {
    OrderNode* node = addNode(side_, 3, 100);

    window_.fill(node, 30);
    expectLevel(3, {1}, 70);
    EXPECT_EQ(node->order.getRemainingQuantity(), 70u);

    window_.fill(node, 55);
    expectLevel(3, {1}, 15);
    EXPECT_EQ(node->order.getRemainingQuantity(), 15u);
}

TEST_P(WindowTest, Fill_Fully_OrderStaysUntilItIsRemoved) {
    OrderNode* node = addNode(side_, 1, 100);
    addNode(side_, 0, 10);

    window_.fill(node, 100);

    EXPECT_TRUE(node->order.isFilled());
    expectLevel(1, {1}, 0);
    expectOnlySide(side_, 1);
    EXPECT_EQ(bestAt(window_, side_), at(1));
}

TEST_P(WindowTest, Fill_OneOfTwo_OtherOrderAndPositionsUntouched) {
    OrderNode* first = addNode(side_, 1, 100);
    OrderNode* second = addNode(side_, 1, 60);
    addNode(side_, 2, 50);

    window_.fill(second, 25);

    EXPECT_EQ(first->order.getRemainingQuantity(), 100u);
    EXPECT_EQ(second->order.getRemainingQuantity(), 35u);
    expectLevel(1, {1, 2}, 135);
    expectLevel(2, {3}, 50);
}

TEST_P(WindowTest, Fill_OrderPartiallyFilledBeforeItWasAdded_LevelReachesZero) {
    OrderNode* node = makeNode(side_, 2, 100);
    node->order.fill(30);
    place(window_, node);

    window_.fill(node, 70);

    EXPECT_TRUE(node->order.isFilled());
    expectLevel(2, {1}, 0);
}

// --------------------------
// Remove
// --------------------------
TEST_P(WindowTest, Remove_EachOfThree_OthersKeepTheirOrder) {
    for (std::size_t position : {1, 2}) {
        for (std::size_t removed {0}; removed < 3; ++removed) {
            std::vector<OrderNode*> nodes;
            IDs left;
            for (Quantity quantity : {10, 20, 30}) {
                nodes.push_back(addNode(side_, position, quantity));
                left.push_back(nodes.back()->order.getID());
            }

            window_.remove(nodes[removed]);

            left.erase(left.begin() + static_cast<std::ptrdiff_t>(removed));
            expectLevel(position, left, 60 - 10 * static_cast<Quantity>(removed + 1));
            EXPECT_EQ(nodes[removed]->prev, nullptr);
            EXPECT_EQ(nodes[removed]->next, nullptr);
            expectOnlySide(side_, position);
            EXPECT_EQ(bestAt(window_, side_), at(position));

            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(removed));
            for (OrderNode* node : nodes)
                window_.remove(node);
        }
    }
}

TEST_P(WindowTest, Remove_OnlyOrder_PositionIsEmpty) {
    OrderNode* node = addNode(side_, 2, 100);

    window_.remove(node);

    EXPECT_TRUE(levelAt(2).queue.empty());
    expectLevel(2, {}, 0);
    expectNeitherSide(2);
    EXPECT_EQ(bestAt(window_, side_), NONE);
}

TEST_P(WindowTest, Remove_PartiallyFilled_OnlyWhatIsLeftComesOff) {
    OrderNode* first = addNode(side_, 2, 100);
    addNode(side_, 2, 50);
    window_.fill(first, 30);

    window_.remove(first);

    expectLevel(2, {2}, 50);
}

TEST_P(WindowTest, Remove_FullyFilled_OnlyTheCountDrops) {
    OrderNode* first = addNode(side_, 1, 100);
    OrderNode* second = addNode(side_, 1, 50);
    window_.fill(first, 100);
    window_.fill(second, 50);

    window_.remove(first);
    expectLevel(1, {2}, 0);

    window_.remove(second);
    expectLevel(1, {}, 0);
    expectNeitherSide(1);
}

TEST_P(WindowTest, Remove_AtOnePosition_OtherPositionsAndSideUntouched) {
    OrderNode* node = addNode(side_, 1, 100);
    addNode(side_, 2, 50);
    addNode(other_, 6, 70);

    window_.remove(node);

    expectLevel(2, {2}, 50);
    expectLevel(6, {3}, 70);
    expectOnlySide(side_, 2);
    expectOnlySide(other_, 6);
}

TEST_P(WindowTest, Remove_OnlyOrder_EitherSideCanUseThePositionAgain) {
    for (std::size_t position : {1, 2}) {
        for (Side next : {side_, other_}) {
            OrderNode* old = addNode(side_, position, 100);
            window_.remove(old);

            OrderNode* fresh = addNode(next, position, 70);

            expectLevel(position, {fresh->order.getID()}, 70);
            expectOnlySide(next, position);
            EXPECT_EQ(bestAt(window_, next), at(position));
            EXPECT_EQ(bestAt(window_, otherSide(next)), NONE);
            window_.remove(fresh);
        }
    }
}

// --------------------------
// Best and after
// --------------------------
TEST_P(WindowTest, Best_AcrossTheWrap_FollowsThePositionsNotTheSlots) {
    // The slots of these are 7, 0 and 1
    addNode(side_, 1, 5);
    addNode(side_, 2, 5);
    addNode(side_, 3, 5);

    EXPECT_EQ(walk(window_, side_), positionsAt({3, 2, 1}));
}

TEST_P(WindowTest, Best_Removed_NextOneTakesOver) {
    OrderNode* best = addNode(side_, 6, 5);
    addNode(side_, 0, 5);
    OrderNode* wrapped = addNode(side_, 2, 5);
    EXPECT_EQ(walk(window_, side_), positionsAt({6, 2, 0}));

    window_.remove(best);
    EXPECT_EQ(walk(window_, side_), positionsAt({2, 0}));

    window_.remove(wrapped);
    EXPECT_EQ(walk(window_, side_), positionsAt({0}));
}

TEST_P(WindowTest, Best_BothSides_AreIndependent) {
    OrderNode* node = addNode(side_, 1, 5);
    OrderNode* otherNode = addNode(other_, 5, 5);
    addNode(other_, 7, 5);

    EXPECT_EQ(walk(window_, side_), positionsAt({1}));
    EXPECT_EQ(walk(window_, other_), positionsAt({5, 7}));

    window_.remove(node);
    EXPECT_EQ(bestAt(window_, side_), NONE);
    EXPECT_EQ(bestAt(window_, other_), at(5));

    window_.remove(otherNode);
    EXPECT_EQ(bestAt(window_, other_), at(7));
}

TEST_P(WindowTest, After_PositionWithoutOrders_IsTheNextOne) {
    addNode(side_, 6, 5);
    addNode(side_, 1, 5);

    EXPECT_EQ(afterAt(window_, side_, at(7)), at(6));
    EXPECT_EQ(afterAt(window_, side_, at(3)), at(1));
    EXPECT_EQ(afterAt(window_, side_, at(0)), NONE);
}

TEST_P(WindowTest, After_StepsOverTheWrapPoint) {
    addNode(side_, 1, 5);
    addNode(side_, 2, 5);

    EXPECT_EQ(afterAt(window_, side_, at(2)), at(1));
    EXPECT_EQ(afterAt(window_, side_, at(1)), NONE);
}

TEST_P(WindowTest, SizeOfOne_TheOnlyLevelWorks) {
    Window tiny(5, 100, 1);
    OrderNode node(Order(1, OWNER, OrderType::GTC, side_, 100, 10));
    place(tiny, &node);

    EXPECT_EQ(node.level, &tiny.level(0).queue);
    EXPECT_EQ(walk(tiny, side_), (Positions {0}));
    EXPECT_EQ(bestAt(tiny, other_), NONE);

    tiny.remove(&node);
    EXPECT_EQ(bestAt(tiny, side_), NONE);
}

TEST_P(WindowTest, WideWindow_FarGap_IsCrossed) {
    Window wide(1, 5'000, 16'384);
    OrderNode low(Order(1, OWNER, OrderType::GTC, side_, 5'005, 10));
    OrderNode high(Order(2, OWNER, OrderType::GTC, side_, 21'380, 10));
    place(wide, &low);
    place(wide, &high);

    Positions expected {5, 16'380};
    if (side_ == Side::buy) std::reverse(expected.begin(), expected.end());
    EXPECT_EQ(walk(wide, side_), expected);
    EXPECT_EQ(low.level, &wide.level(5).queue);
    EXPECT_EQ(high.level, &wide.level(16'380).queue);
}

// --------------------------
// Copy and move
// --------------------------
TEST(WindowTraitsTest, CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<Window>);
    EXPECT_FALSE(std::is_copy_assignable_v<Window>);
    EXPECT_TRUE(std::is_move_constructible_v<Window>);
    EXPECT_TRUE(std::is_move_assignable_v<Window>);
}

// Two orders at position 1, one on the other side at 3, then moved away from window_
class MovedWindowTest : public WindowTest {
protected:
    Window target_ {1, 1, 16};
    OrderNode old_ {Order(900, OWNER, OrderType::GTC, otherSide(GetParam()), 10, 5)};
    OrderNode* first_ {nullptr};
    OrderNode* second_ {nullptr};
    OrderNode* third_ {nullptr};

    void SetUp() override {
        // To be replaced by the move
        place(target_, &old_);

        first_ = addNode(side_, 1, 100);
        second_ = addNode(side_, 1, 30);
        third_ = addNode(other_, 3, 70);
    }

    // The orders point to their level, so it must not have moved
    void expectHoldsEverything(const Window& window) const {
        expectLevel(window, 1, {1, 2}, 130);
        expectLevel(window, 3, {3}, 70);
        EXPECT_EQ(walk(window, side_), positionsAt({1}));
        EXPECT_EQ(walk(window, other_), positionsAt({3}));
        EXPECT_FALSE(containsAt(window, other_, at(1)));
        EXPECT_EQ(window.size(), SIZE);
        EXPECT_EQ(window.basePrice(), BASE_PRICE);
        EXPECT_EQ(window.positionOf(BASE_PRICE), 0u);
        EXPECT_EQ(window.positionOf(BASE_PRICE + 7 * TICK), 7u);
        EXPECT_EQ(window.positionOf(BASE_PRICE + 8 * TICK), NONE);
        EXPECT_EQ(window.priceOf(7), BASE_PRICE + 7 * TICK);
    }

    void expectMovedFrom(const Window& window) const {
        EXPECT_EQ(window.size(), 0u);
        EXPECT_EQ(bestAt(window, side_), NONE);
        EXPECT_EQ(bestAt(window, other_), NONE);
        expectNone(window, {BASE_PRICE, BASE_PRICE + 5 * TICK, BASE_PRICE + 7 * TICK, 0, -1, MAX, MIN});
    }

    void expectUsable(Window& window) {
        OrderNode* fresh = makeNode(side_, 1, 10);
        place(window, fresh);
        window.fill(first_, 60);
        expectLevel(window, 1, {1, 2, 4}, 80);

        window.remove(first_);
        window.remove(fresh);
        window.remove(second_);
        expectLevel(window, 1, {}, 0);
        EXPECT_EQ(bestAt(window, side_), NONE);

        window.remove(third_);
        EXPECT_EQ(bestAt(window, other_), NONE);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(MovedWindowTest);

TEST_P(MovedWindowTest, MoveConstruct_TargetHoldsEverythingAndStaysUsable) {
    Window target(std::move(window_));

    expectHoldsEverything(target);
    expectMovedFrom(window_);
    expectUsable(target);
}

TEST_P(MovedWindowTest, MoveAssign_TargetLosesItsOldContentAndStaysUsable) {
    target_ = std::move(window_);

    expectHoldsEverything(target_);
    expectMovedFrom(window_);
    expectUsable(target_);
}

TEST_P(MovedWindowTest, MoveAssign_TargetTakesTheGeometryOfTheSource) {
    target_ = Window(10, 1'000, 4);

    expectRoundTrip(target_, 1'000, 10, 4);
    expectSlots(target_, 100);
    expectNone(target_, {10, 1'005, 1'040, 1'400});
}

TEST_P(MovedWindowTest, MovedFrom_AssignedANewWindow_IsUsableAndTargetIsUntouched) {
    target_ = std::move(window_);

    window_ = Window(5, 510, 8);
    OrderNode node(Order(50, OWNER, OrderType::GTC, side_, 525, 40));
    place(window_, &node);

    EXPECT_EQ(window_.level(3).queue.head, &node);
    EXPECT_EQ(node.level, &window_.level(3).queue);
    EXPECT_EQ(walk(window_, side_), (Positions {3}));
    EXPECT_EQ(bestAt(window_, other_), NONE);
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

// Base tick 37 in 64 slots, so the wrap is in the middle of the window
TEST(WindowRandomTest, RandomOperations_MatchAModel) {
    constexpr Price TICK = 5;
    constexpr Price BASE_PRICE = 37 * TICK;
    constexpr std::size_t SIZE = 64;
    constexpr std::size_t STEPS = 30'000;

    Window window(TICK, BASE_PRICE, SIZE);
    std::deque<OrderNode> nodes;
    std::vector<OrderNode*> live;
    Model model;
    std::mt19937 generator(2024);
    std::uniform_int_distribution<int> percent(0, 99);
    std::uniform_int_distribution<Price> anyPrice(0, SIZE - 1);
    OrderID nextID {1};
    std::size_t sideChanges {0};
    std::map<Price, Side> lastSide;

    auto positionOfPrice = [&](Price price) {
        return static_cast<std::size_t>((price - BASE_PRICE) / TICK);
    };

    auto bestOf = [&](Side side) {
        const ModelSide& modelSide = model.of(side);
        if (modelSide.empty()) return NONE;
        Price price = (side == Side::buy) ? modelSide.rbegin()->first : modelSide.begin()->first;
        return positionOfPrice(price);
    };

    auto checkPrice = [&](Price price, std::size_t step) {
        const Level& level = window.level(positionOfPrice(price));
        IDs expectedIDs;
        Quantity expectedQuantity {0};
        Quantity expectedCount {0};
        for (Side side : {Side::buy, Side::sell}) {
            auto it = model.of(side).find(price);
            if (it == model.of(side).end()) continue;
            for (const Resting& resting : it->second) {
                expectedIDs.push_back(resting.node->order.getID());
                expectedQuantity += resting.remaining;
                ++expectedCount;
            }
        }
        EXPECT_EQ(forwardIDs(level.queue), expectedIDs) << "step " << step << " price " << price;
        EXPECT_EQ(level.data.qty_, expectedQuantity) << "step " << step << " price " << price;
        EXPECT_EQ(level.data.count_, expectedCount) << "step " << step << " price " << price;
        EXPECT_EQ(window.contains<Side::buy>(positionOfPrice(price)), model.buys.contains(price)) << "step " << step;
        EXPECT_EQ(window.contains<Side::sell>(positionOfPrice(price)), model.sells.contains(price)) << "step " << step;
        EXPECT_EQ(window.best<Side::buy>(), bestOf(Side::buy)) << "step " << step;
        EXPECT_EQ(window.best<Side::sell>(), bestOf(Side::sell)) << "step " << step;
    };

    for (std::size_t step {0}; step < STEPS; ++step) {
        int choice = percent(generator);
        Price touched {0};

        if (choice < 40 || live.empty()) {
            Side side = (percent(generator) < 50) ? Side::buy : Side::sell;
            Price price = BASE_PRICE + anyPrice(generator) * TICK;
            bool crosses = (side == Side::buy)
                ? (!model.sells.empty() && model.sells.begin()->first <= price)
                : (!model.buys.empty() && model.buys.rbegin()->first >= price);
            if (crosses) continue;

            Quantity quantity = static_cast<Quantity>(percent(generator)) + 1;
            nodes.emplace_back(Order(nextID++, OWNER, OrderType::GTC, side, price, quantity));
            OrderNode* node = &nodes.back();
            place(window, node);
            model.of(side)[price].push_back({node, quantity});
            live.push_back(node);
            touched = price;

            auto last = lastSide.find(price);
            if (last != lastSide.end() && last->second != side) ++sideChanges;
            lastSide[price] = side;

            EXPECT_EQ(node->level, &window.level(positionOfPrice(price)).queue) << "step " << step;
            EXPECT_EQ(node->levelData, &window.level(positionOfPrice(price)).data) << "step " << step;
        } else if (choice < 70) {
            OrderNode* node = live[std::uniform_int_distribution<std::size_t>(0, live.size() - 1)(generator)];
            Quantity remaining = node->order.getRemainingQuantity();
            if (remaining == 0) continue;

            Quantity quantity = std::uniform_int_distribution<Quantity>(1, remaining)(generator);
            window.fill(node, quantity);
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

            window.remove(node);
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
    EXPECT_EQ(walk(window, Side::buy), expectedBuys);
    EXPECT_EQ(walk(window, Side::sell), expectedSells);
}
