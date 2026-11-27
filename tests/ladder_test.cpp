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
#include "queue_test_utils.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::detail;
using namespace orderbook::test;

using Positions = std::vector<std::size_t>;

constexpr std::size_t NONE = Ladder::NONE;

constexpr OwnerID OWNER = 1;

constexpr Price MAX = std::numeric_limits<Price>::max();
constexpr Price MIN = std::numeric_limits<Price>::min();

// Stops a walk that never ends, which a broken after would give
constexpr std::size_t WALK_LIMIT = 1'000;

static bool containsAt(const Ladder& ladder, Side side, std::size_t position) {
    return (side == Side::buy) ? ladder.contains<Side::buy>(position) : ladder.contains<Side::sell>(position);
}

static std::size_t bestAt(const Ladder& ladder, Side side) {
    return (side == Side::buy) ? ladder.best<Side::buy>() : ladder.best<Side::sell>();
}

static std::size_t afterAt(const Ladder& ladder, Side side, std::size_t position) {
    return (side == Side::buy) ? ladder.after<Side::buy>(position) : ladder.after<Side::sell>(position);
}

static Positions walk(const Ladder& ladder, Side side) {
    Positions found;
    for (std::size_t position = bestAt(ladder, side); position != NONE && found.size() < WALK_LIMIT; position = afterAt(ladder, side, position))
        found.push_back(position);
    return found;
}

static void place(Ladder& ladder, OrderNode* node) {
    ladder.add(ladder.positionOf(node->order.getPrice()), node);
}

// Every test runs once per side. The prices are written as the buys see them, and
// the sells get them mirrored in the band. Price p is at position p - 1.
class LadderTest : public ::testing::TestWithParam<Side> {
protected:
    Side side_ {GetParam()};
    Side other_ {otherSide(side_)};
    Ladder ladder_ {1, 100, 1};

    // A deque, so that the nodes never move
    std::deque<OrderNode> nodes_;
    OrderID nextID_ {1};

    Price at(Price price) const {
        return (side_ == Side::buy) ? price : 101 - price;
    }

    std::size_t positionAt(Price price) const {
        return static_cast<std::size_t>(at(price) - 1);
    }

    Positions positionsAt(const std::vector<Price>& prices) const {
        Positions positions;
        for (Price price : prices)
            positions.push_back(positionAt(price));
        return positions;
    }

    OrderNode* makeNode(Side side, Price price, Quantity quantity) {
        nodes_.emplace_back(Order(nextID_++, OWNER, OrderType::GTC, side, at(price), quantity));
        return &nodes_.back();
    }

    OrderNode* addNode(Side side, Price price, Quantity quantity) {
        OrderNode* node = makeNode(side, price, quantity);
        place(ladder_, node);
        return node;
    }

    const Level& levelAt(Price price) const {
        return ladder_.level(positionAt(price));
    }

    // In both directions, and every order points to its level
    void expectLevel(const Ladder& ladder, Price price, const IDs& ids, Quantity quantity) const {
        const Level& level = ladder.level(positionAt(price));
        EXPECT_EQ(forwardIDs(level.queue), ids);
        EXPECT_EQ(backwardIDs(level.queue), IDs(ids.rbegin(), ids.rend()));
        EXPECT_EQ(level.data.qty_, quantity);
        EXPECT_EQ(level.data.count_, ids.size());
        for (const OrderNode* node = level.queue.head; node != nullptr; node = node->next) {
            EXPECT_EQ(node->level, &level.queue);
            EXPECT_EQ(node->levelData, &level.data);
        }
    }

    void expectLevel(Price price, const IDs& ids, Quantity quantity) const {
        expectLevel(ladder_, price, ids, quantity);
    }

    void expectOnlySide(Side side, Price price) const {
        EXPECT_TRUE(containsAt(ladder_, side, positionAt(price)));
        EXPECT_FALSE(containsAt(ladder_, otherSide(side), positionAt(price)));
    }

    void expectNeitherSide(Price price) const {
        EXPECT_FALSE(containsAt(ladder_, side_, positionAt(price)));
        EXPECT_FALSE(containsAt(ladder_, other_, positionAt(price)));
    }
};

INSTANTIATE_FOR_BOTH_SIDES(LadderTest);

// --------------------------
// Price and position
// --------------------------
static void expectRoundTrip(const Ladder& ladder, Price minPrice, Price tick, std::size_t levels) {
    for (std::size_t position {0}; position < levels; ++position) {
        Price price = minPrice + static_cast<Price>(position) * tick;
        ASSERT_EQ(ladder.priceOf(position), price);
        ASSERT_EQ(ladder.positionOf(price), position);
    }
}

static void expectNone(const Ladder& ladder, const std::vector<Price>& prices) {
    for (Price price : prices)
        EXPECT_EQ(ladder.positionOf(price), NONE) << "price " << price;
}

TEST(LadderTranslationTest, LegalPrices_EachHasItsPosition) {
    expectRoundTrip(Ladder(1, 1'000, 1), 1, 1, 1'000);
    expectRoundTrip(Ladder(50, 60, 1), 50, 1, 11);      // band not starting at 1
    expectRoundTrip(Ladder(10, 500, 5), 10, 5, 99);
    expectRoundTrip(Ladder(5, 50, 5), 5, 5, 10);        // band starting at the tick
    expectRoundTrip(Ladder(7, 7, 1), 7, 1, 1);
    expectRoundTrip(Ladder(10, 10, 5), 10, 5, 1);
    expectRoundTrip(Ladder(1'000'000'000, 2'000'000'000, 1'000'000'000), 1'000'000'000, 1'000'000'000, 2);
    expectRoundTrip(Ladder(MAX - 47, MAX, 1), MAX - 47, 1, 48);
    expectRoundTrip(Ladder(2'147'480'000, 2'147'483'000, 1'000), 2'147'480'000, 1'000, 4);
}

TEST(LadderTranslationTest, PricesOutsideTheBand_AreNone) {
    expectNone(Ladder(1, 100, 1), {0, -1, 101});
    expectNone(Ladder(10, 50, 5), {9, 5, 1, 0, -5, -10, 51, 55, 1'000, MAX, MAX - 1, MIN, MIN + 1});
    expectNone(Ladder(7, 7, 1), {6, 8});
    expectNone(Ladder(10, 10, 5), {5, 11, 15});
    expectNone(Ladder(1'000'000'000, 2'000'000'000, 1'000'000'000), {0, 999'999'999, 2'000'000'001, MAX, MIN});
    expectNone(Ladder(MAX - 47, MAX, 1), {MAX - 48, 0, -1, MIN});
    expectNone(Ladder(2'147'480'000, 2'147'483'000, 1'000), {2'147'479'000, 1'000, 2'147'483'001, MAX, MIN});
}

TEST(LadderTranslationTest, PricesOffTheTick_AreNone) {
    Ladder ladder(10, 50, 5);
    for (Price price {11}; price < 50; ++price)
        EXPECT_EQ(ladder.positionOf(price) == NONE, price % 5 != 0) << "price " << price;

    expectNone(Ladder(1'000'000'000, 2'000'000'000, 1'000'000'000), {1'500'000'000});
}

// --------------------------
// New ladder
// --------------------------
TEST_P(LadderTest, NewLadder_EveryPriceIsEmpty) {
    EXPECT_EQ(bestAt(ladder_, side_), NONE);
    for (Price price {1}; price <= 100; ++price) {
        EXPECT_TRUE(levelAt(price).queue.empty()) << "price " << price;
        expectLevel(price, {}, 0);
        expectNeitherSide(price);
    }
}

// --------------------------
// Add
// --------------------------
TEST_P(LadderTest, Add_FirstOrder_LevelHoldsOnlyIt) {
    OrderNode* node = addNode(side_, 40, 100);

    EXPECT_FALSE(levelAt(40).queue.empty());
    EXPECT_EQ(levelAt(40).queue.head, node);
    EXPECT_EQ(levelAt(40).queue.tail, node);
    expectLevel(40, {1}, 100);
    expectOnlySide(side_, 40);
    EXPECT_EQ(bestAt(ladder_, side_), positionAt(40));
    EXPECT_EQ(bestAt(ladder_, other_), NONE);
}

TEST_P(LadderTest, Add_FirstOrder_NeighboursStayEmpty) {
    addNode(side_, 40, 100);

    for (Price price : {39, 41}) {
        expectLevel(price, {}, 0);
        expectNeitherSide(price);
    }
}

TEST_P(LadderTest, Add_SeveralAtOnePrice_QueuedByArrival) {
    for (Quantity quantity : {10, 20, 30, 40, 50})
        addNode(side_, 40, quantity);

    expectLevel(40, {1, 2, 3, 4, 5}, 150);
    EXPECT_EQ(walk(ladder_, side_), positionsAt({40}));
}

TEST_P(LadderTest, Add_PartiallyFilledOrder_TotalUsesWhatIsLeft) {
    OrderNode* node = makeNode(side_, 40, 100);
    node->order.fill(30);

    place(ladder_, node);

    expectLevel(40, {1}, 70);
}

TEST_P(LadderTest, Add_FirstAndLastPrice_LandInTheEndLevels) {
    addNode(side_, 1, 10);
    addNode(side_, 100, 20);

    expectLevel(1, {1}, 10);
    expectLevel(100, {2}, 20);
    EXPECT_EQ(walk(ladder_, side_), positionsAt({100, 1}));
}

TEST_P(LadderTest, Add_OneOrderOnEachSide_LevelsAndSidesAreIndependent) {
    addNode(side_, 40, 100);
    addNode(other_, 60, 70);

    expectLevel(40, {1}, 100);
    expectLevel(60, {2}, 70);
    expectOnlySide(side_, 40);
    expectOnlySide(other_, 60);
    EXPECT_EQ(bestAt(ladder_, side_), positionAt(40));
    EXPECT_EQ(bestAt(ladder_, other_), positionAt(60));
}

// --------------------------
// Fill
// --------------------------
TEST_P(LadderTest, Fill_SeveralTimes_EachComesOffTheLevel) {
    OrderNode* node = addNode(side_, 40, 100);

    ladder_.fill(node, 30);
    expectLevel(40, {1}, 70);
    EXPECT_EQ(node->order.getRemainingQuantity(), 70u);

    ladder_.fill(node, 55);
    expectLevel(40, {1}, 15);
    EXPECT_EQ(node->order.getRemainingQuantity(), 15u);
}

TEST_P(LadderTest, Fill_Fully_OrderStaysUntilItIsRemoved) {
    OrderNode* node = addNode(side_, 40, 100);
    addNode(side_, 30, 10);

    ladder_.fill(node, 100);

    EXPECT_TRUE(node->order.isFilled());
    expectLevel(40, {1}, 0);
    expectOnlySide(side_, 40);
    EXPECT_EQ(bestAt(ladder_, side_), positionAt(40));
}

TEST_P(LadderTest, Fill_OneOfTwo_OtherOrderAndPricesUntouched) {
    OrderNode* first = addNode(side_, 40, 100);
    OrderNode* second = addNode(side_, 40, 60);
    addNode(side_, 41, 50);

    ladder_.fill(second, 25);

    EXPECT_EQ(first->order.getRemainingQuantity(), 100u);
    EXPECT_EQ(second->order.getRemainingQuantity(), 35u);
    expectLevel(40, {1, 2}, 135);
    expectLevel(41, {3}, 50);
}

TEST_P(LadderTest, Fill_OrderPartiallyFilledBeforeItWasAdded_LevelReachesZero) {
    OrderNode* node = makeNode(side_, 40, 100);
    node->order.fill(30);
    place(ladder_, node);

    ladder_.fill(node, 70);

    EXPECT_TRUE(node->order.isFilled());
    expectLevel(40, {1}, 0);
}

// --------------------------
// Remove
// --------------------------
TEST_P(LadderTest, Remove_EachOfThree_OthersKeepTheirOrder) {
    for (std::size_t removed {0}; removed < 3; ++removed) {
        std::vector<OrderNode*> nodes;
        IDs left;
        for (Quantity quantity : {10, 20, 30}) {
            nodes.push_back(addNode(side_, 40, quantity));
            left.push_back(nodes.back()->order.getID());
        }

        ladder_.remove(nodes[removed]);

        left.erase(left.begin() + static_cast<std::ptrdiff_t>(removed));
        expectLevel(40, left, 60 - 10 * static_cast<Quantity>(removed + 1));
        EXPECT_EQ(nodes[removed]->prev, nullptr);
        EXPECT_EQ(nodes[removed]->next, nullptr);
        expectOnlySide(side_, 40);
        EXPECT_EQ(bestAt(ladder_, side_), positionAt(40));

        nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(removed));
        for (OrderNode* node : nodes)
            ladder_.remove(node);
    }
}

TEST_P(LadderTest, Remove_OnlyOrder_PriceIsEmpty) {
    OrderNode* node = addNode(side_, 40, 100);

    ladder_.remove(node);

    EXPECT_TRUE(levelAt(40).queue.empty());
    expectLevel(40, {}, 0);
    expectNeitherSide(40);
    EXPECT_EQ(bestAt(ladder_, side_), NONE);
}

TEST_P(LadderTest, Remove_PartiallyFilled_OnlyWhatIsLeftComesOff) {
    OrderNode* first = addNode(side_, 40, 100);
    addNode(side_, 40, 50);
    ladder_.fill(first, 30);

    ladder_.remove(first);

    expectLevel(40, {2}, 50);
}

TEST_P(LadderTest, Remove_FullyFilled_OnlyTheCountDrops) {
    OrderNode* first = addNode(side_, 40, 100);
    OrderNode* second = addNode(side_, 40, 50);
    ladder_.fill(first, 100);
    ladder_.fill(second, 50);

    ladder_.remove(first);
    expectLevel(40, {2}, 0);

    ladder_.remove(second);
    expectLevel(40, {}, 0);
    expectNeitherSide(40);
}

TEST_P(LadderTest, Remove_AtOnePrice_OtherPricesAndSideUntouched) {
    OrderNode* node = addNode(side_, 40, 100);
    addNode(side_, 41, 50);
    addNode(other_, 60, 70);

    ladder_.remove(node);

    expectLevel(41, {2}, 50);
    expectLevel(60, {3}, 70);
    expectOnlySide(side_, 41);
    expectOnlySide(other_, 60);
}

TEST_P(LadderTest, Remove_OnlyOrder_EitherSideCanUseThePriceAgain) {
    for (Side next : {side_, other_}) {
        OrderNode* old = addNode(side_, 40, 100);
        ladder_.remove(old);

        OrderNode* fresh = addNode(next, 40, 70);

        expectLevel(40, {fresh->order.getID()}, 70);
        expectOnlySide(next, 40);
        EXPECT_EQ(bestAt(ladder_, next), positionAt(40));
        EXPECT_EQ(bestAt(ladder_, otherSide(next)), NONE);
        ladder_.remove(fresh);
    }
}

// --------------------------
// Best and after
// --------------------------
TEST_P(LadderTest, Best_Removed_NextOneTakesOver) {
    OrderNode* best = addNode(side_, 30, 5);
    addNode(side_, 10, 5);
    addNode(side_, 20, 5);
    EXPECT_EQ(walk(ladder_, side_), positionsAt({30, 20, 10}));

    ladder_.remove(best);

    EXPECT_EQ(walk(ladder_, side_), positionsAt({20, 10}));
}

TEST_P(LadderTest, Best_BothSides_AreIndependent) {
    OrderNode* node = addNode(side_, 30, 5);
    OrderNode* otherNode = addNode(other_, 70, 5);
    addNode(other_, 80, 5);

    EXPECT_EQ(walk(ladder_, side_), positionsAt({30}));
    EXPECT_EQ(walk(ladder_, other_), positionsAt({70, 80}));

    ladder_.remove(node);
    EXPECT_EQ(bestAt(ladder_, side_), NONE);
    EXPECT_EQ(bestAt(ladder_, other_), positionAt(70));

    ladder_.remove(otherNode);
    EXPECT_EQ(bestAt(ladder_, other_), positionAt(80));
}

TEST_P(LadderTest, After_PositionWithoutOrders_IsTheNextOne) {
    addNode(side_, 30, 5);
    addNode(side_, 10, 5);

    EXPECT_EQ(afterAt(ladder_, side_, positionAt(100)), positionAt(30));
    EXPECT_EQ(afterAt(ladder_, side_, positionAt(25)), positionAt(10));
    EXPECT_EQ(afterAt(ladder_, side_, positionAt(5)), NONE);
}

TEST_P(LadderTest, WideLadder_FarGap_IsCrossed) {
    Ladder wide(1, 100'000, 1);
    OrderNode low(Order(1, OWNER, OrderType::GTC, side_, 5, 10));
    OrderNode high(Order(2, OWNER, OrderType::GTC, side_, 99'990, 10));
    place(wide, &low);
    place(wide, &high);

    Positions expected {4, 99'989};
    if (side_ == Side::buy) std::reverse(expected.begin(), expected.end());
    EXPECT_EQ(walk(wide, side_), expected);
}

TEST_P(LadderTest, TickOfFive_OrdersLandInTheLevelOfTheirPosition) {
    Ladder ladder(10, 500, 5);
    std::deque<OrderNode> nodes;
    for (Price price : {10, 25, 500}) {
        nodes.emplace_back(Order(nextID_++, OWNER, OrderType::GTC, side_, price, 40));
        place(ladder, &nodes.back());
    }

    Positions expected {0, 3, 98};
    for (std::size_t i {0}; i < 3; ++i) {
        EXPECT_EQ(ladder.level(expected[i]).queue.head, &nodes[i]);
        EXPECT_EQ(ladder.level(expected[i]).data.qty_, 40u);
        EXPECT_EQ(nodes[i].level, &ladder.level(expected[i]).queue);
    }
    if (side_ == Side::buy) std::reverse(expected.begin(), expected.end());
    EXPECT_EQ(walk(ladder, side_), expected);

    for (OrderNode& node : nodes)
        ladder.remove(&node);
    EXPECT_EQ(bestAt(ladder, side_), NONE);
}

// --------------------------
// Copy and move
// --------------------------
TEST(LadderTraitsTest, CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<Ladder>);
    EXPECT_FALSE(std::is_copy_assignable_v<Ladder>);
    EXPECT_TRUE(std::is_move_constructible_v<Ladder>);
    EXPECT_TRUE(std::is_move_assignable_v<Ladder>);
}

// Two orders at 40 and one on the other side at 60, then moved away from ladder_
class MovedLadderTest : public LadderTest {
protected:
    Ladder target_ {1, 200, 1};
    OrderNode old_ {Order(900, OWNER, OrderType::GTC, otherSide(GetParam()), 150, 5)};
    OrderNode* first_ {nullptr};
    OrderNode* second_ {nullptr};
    OrderNode* third_ {nullptr};

    void SetUp() override {
        // To be replaced by the move
        place(target_, &old_);

        first_ = addNode(side_, 40, 100);
        second_ = addNode(side_, 40, 30);
        third_ = addNode(other_, 60, 70);
    }

    // The orders point to their level, so it must not have moved
    void expectHoldsEverything(const Ladder& ladder) const {
        expectLevel(ladder, 40, {1, 2}, 130);
        expectLevel(ladder, 60, {3}, 70);
        EXPECT_EQ(walk(ladder, side_), positionsAt({40}));
        EXPECT_EQ(walk(ladder, other_), positionsAt({60}));
        EXPECT_FALSE(containsAt(ladder, other_, positionAt(40)));
        EXPECT_EQ(ladder.positionOf(1), 0u);
        EXPECT_EQ(ladder.positionOf(100), 99u);
        EXPECT_EQ(ladder.positionOf(101), NONE);
        EXPECT_EQ(ladder.priceOf(99), 100);
    }

    void expectMovedFrom(const Ladder& ladder) const {
        EXPECT_EQ(bestAt(ladder, side_), NONE);
        EXPECT_EQ(bestAt(ladder, other_), NONE);
        expectNone(ladder, {1, 40, 60, 100, 0, -1, 101, MAX, MIN});
    }

    void expectUsable(Ladder& ladder) {
        OrderNode* fresh = makeNode(side_, 40, 10);
        place(ladder, fresh);
        ladder.fill(first_, 60);
        expectLevel(ladder, 40, {1, 2, 4}, 80);

        ladder.remove(first_);
        ladder.remove(fresh);
        ladder.remove(second_);
        expectLevel(ladder, 40, {}, 0);
        EXPECT_EQ(bestAt(ladder, side_), NONE);

        ladder.remove(third_);
        EXPECT_EQ(bestAt(ladder, other_), NONE);
    }
};

INSTANTIATE_FOR_BOTH_SIDES(MovedLadderTest);

TEST_P(MovedLadderTest, MoveConstruct_TargetHoldsEverythingAndStaysUsable) {
    Ladder target(std::move(ladder_));

    expectHoldsEverything(target);
    expectMovedFrom(ladder_);
    expectUsable(target);
}

TEST_P(MovedLadderTest, MoveAssign_TargetLosesItsOldContentAndStaysUsable) {
    target_ = std::move(ladder_);

    expectHoldsEverything(target_);
    expectMovedFrom(ladder_);
    expectUsable(target_);
}

TEST_P(MovedLadderTest, MoveAssign_TargetTakesTheTickOfTheSource) {
    target_ = Ladder(10, 500, 5);

    expectRoundTrip(target_, 10, 5, 99);
    expectNone(target_, {1, 12, 151, 505});
}

TEST_P(MovedLadderTest, MovedFrom_AssignedANewLadder_IsUsableAndTargetIsUntouched) {
    target_ = std::move(ladder_);

    ladder_ = Ladder(10, 500, 5);
    OrderNode node(Order(50, OWNER, OrderType::GTC, side_, 25, 40));
    place(ladder_, &node);

    EXPECT_EQ(ladder_.level(3).queue.head, &node);
    EXPECT_EQ(node.level, &ladder_.level(3).queue);
    EXPECT_EQ(walk(ladder_, side_), (Positions {3}));
    EXPECT_EQ(bestAt(ladder_, other_), NONE);
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

// 60 prices with a tick of 5, so prices empty and change side often
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
    EXPECT_EQ(walk(ladder, Side::buy), expectedBuys);
    EXPECT_EQ(walk(ladder, Side::sell), expectedSells);
}
