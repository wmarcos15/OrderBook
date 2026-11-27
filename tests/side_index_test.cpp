#include <gtest/gtest.h>
#include <cstddef>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "bitmap.hpp"
#include "side_index.hpp"
#include "types.hpp"

using orderbook::Side;
using orderbook::detail::Bitmap;
using orderbook::detail::SideIndex;

using Positions = std::vector<std::size_t>;

constexpr std::size_t NONE = Bitmap::NONE;

constexpr std::size_t SMALL = 10;

// Two words of summary. Mirroring a position keeps it at a word edge.
constexpr std::size_t LARGE = 8'192;

// Caps the walk in case after never ends
constexpr std::size_t WALK_LIMIT = 10'000;

template<Side side>
using SideTag = std::integral_constant<Side, side>;

struct SideTagName {
    template<typename Tag>
    static std::string GetName(int) {
        return (Tag::value == Side::buy) ? "buy" : "sell";
    }
};

// Every test runs for both sides. The positions are written as the buys see them,
// and the sells get them mirrored: the higher the better for both.
template<typename Tag>
class SideIndexTest : public ::testing::Test {
public:
    static constexpr Side SIDE = Tag::value;
    using Index = SideIndex<SIDE>;

    static std::size_t at(std::size_t position, std::size_t positions = SMALL) {
        if (position == NONE || SIDE == Side::buy) return position;
        return positions - 1 - position;
    }

    static Positions at(Positions all, std::size_t positions = SMALL) {
        for (std::size_t& position : all)
            position = at(position, positions);
        return all;
    }

    static Index makeIndex(const Positions& contained, std::size_t positions = SMALL) {
        Index index(positions);
        for (std::size_t position : contained)
            index.add(at(position, positions));
        return index;
    }

    static Positions walk(const Index& index) {
        Positions found;
        for (std::size_t position = index.best(); position != NONE && found.size() < WALK_LIMIT; position = index.after(position))
            found.push_back(position);
        return found;
    }

    // For the sells this drops the back of the bitmap
    static void shiftDown(Index& index, std::size_t n) {
        if constexpr (SIDE == Side::buy) index.dropFront(n);
        else index.dropBack(n);
    }

    static void shiftUp(Index& index, std::size_t n) {
        if constexpr (SIDE == Side::buy) index.dropBack(n);
        else index.dropFront(n);
    }

    // Best first
    static void expectHolds(const Index& index, const Positions& expected, std::size_t positions = SMALL) {
        EXPECT_EQ(walk(index), at(expected, positions));
        std::set<std::size_t> contained(expected.begin(), expected.end());
        for (std::size_t position {0}; position < positions; ++position)
            ASSERT_EQ(index.contains(at(position, positions)), contained.contains(position)) << "position " << position;
    }
};

using Sides = ::testing::Types<SideTag<Side::buy>, SideTag<Side::sell>>;
TYPED_TEST_SUITE(SideIndexTest, Sides, SideTagName);

// --------------------------
// Empty and one position
// --------------------------
TYPED_TEST(SideIndexTest, NewIndex_HoldsNothing) {
    auto index = this->makeIndex({});

    this->expectHolds(index, {});
    EXPECT_EQ(index.best(), NONE);
    for (std::size_t position {0}; position < SMALL; ++position)
        EXPECT_EQ(index.after(position), NONE) << "position " << position;
}

TYPED_TEST(SideIndexTest, OnePosition_IsBestWithNothingAfter) {
    for (std::size_t position : {std::size_t {0}, std::size_t {5}, SMALL - 1}) {
        auto index = this->makeIndex({position});

        this->expectHolds(index, {position});
        EXPECT_EQ(index.after(this->at(position)), NONE);
    }
}

// --------------------------
// Add
// --------------------------
TYPED_TEST(SideIndexTest, Add_BetterPosition_BecomesBest) {
    auto index = this->makeIndex({5, 8});

    EXPECT_EQ(index.best(), this->at(8));
}

TYPED_TEST(SideIndexTest, Add_WorsePosition_BestStays) {
    auto index = this->makeIndex({5, 2});

    EXPECT_EQ(index.best(), this->at(5));
}

TYPED_TEST(SideIndexTest, Add_InAnyOrder_WalkedBestFirst) {
    this->expectHolds(this->makeIndex({2, 8, 5}), {8, 5, 2});
    this->expectHolds(this->makeIndex({7, 3, 9, 0, 5}), {9, 7, 5, 3, 0});
}

// --------------------------
// Remove
// --------------------------
TYPED_TEST(SideIndexTest, Remove_EachOfThree_TheOthersStayInOrder) {
    for (std::size_t removed : {2u, 5u, 8u}) {
        auto index = this->makeIndex({2, 5, 8});

        index.remove(this->at(removed));

        Positions left {8, 5, 2};
        std::erase(left, removed);
        this->expectHolds(index, left);
        EXPECT_EQ(index.best(), this->at(left[0]));
    }
}

TYPED_TEST(SideIndexTest, Remove_OnlyPosition_IndexIsEmpty) {
    auto index = this->makeIndex({5});

    index.remove(this->at(5));

    this->expectHolds(index, {});
    for (std::size_t position {0}; position < SMALL; ++position)
        EXPECT_EQ(index.after(position), NONE);
}

TYPED_TEST(SideIndexTest, Remove_UntilEmptyThenAdd_StartsFromScratch) {
    auto index = this->makeIndex({5});
    index.remove(this->at(5));

    index.add(this->at(3));

    this->expectHolds(index, {3});
}

TYPED_TEST(SideIndexTest, Remove_ThenAddItAgain_IsBestAgain) {
    auto index = this->makeIndex({4, 6});
    index.remove(this->at(6));

    index.add(this->at(6));

    this->expectHolds(index, {6, 4});
}

TYPED_TEST(SideIndexTest, Remove_BestRepeatedly_DrainsBestFirst) {
    auto index = this->makeIndex({7, 3, 9, 0, 5});
    Positions drained;

    while (index.best() != NONE && drained.size() < SMALL) {
        drained.push_back(index.best());
        index.remove(index.best());
    }

    EXPECT_EQ(drained, this->at(Positions {9, 7, 5, 3, 0}));
}

// --------------------------
// After
// --------------------------
TYPED_TEST(SideIndexTest, After_ContainedPosition_IsTheNextOne) {
    auto index = this->makeIndex({0, 4, 5, 6, SMALL - 1});

    EXPECT_EQ(index.after(this->at(SMALL - 1)), this->at(6));
    EXPECT_EQ(index.after(this->at(6)), this->at(5));
    EXPECT_EQ(index.after(this->at(5)), this->at(4));
    EXPECT_EQ(index.after(this->at(4)), this->at(0));
    EXPECT_EQ(index.after(this->at(0)), NONE);
}

TYPED_TEST(SideIndexTest, After_PositionInAGap_IsTheNextOne) {
    auto index = this->makeIndex({2, 8});

    EXPECT_EQ(index.after(this->at(7)), this->at(2));
    EXPECT_EQ(index.after(this->at(3)), this->at(2));
}

TYPED_TEST(SideIndexTest, After_PositionBetterThanTheBest_IsTheBest) {
    auto index = this->makeIndex({2, 5});

    EXPECT_EQ(index.after(this->at(SMALL - 1)), this->at(5));
    EXPECT_EQ(index.after(this->at(6)), this->at(5));
}

TYPED_TEST(SideIndexTest, After_PositionWorseThanTheWorst_IsNone) {
    auto index = this->makeIndex({5, 8});

    EXPECT_EQ(index.after(this->at(4)), NONE);
    EXPECT_EQ(index.after(this->at(0)), NONE);
}

// --------------------------
// Word and summary edges
// --------------------------
TYPED_TEST(SideIndexTest, Edges_TwoPositions_FoundAndRemovedBestFirst) {
    const std::vector<std::pair<std::size_t, std::size_t>> pairs {
        {63, 62}, {64, 63}, {64, 0}, {127, 0},
        {4'095, 63}, {4'096, 4'095}, {4'096, 0},
        {LARGE - 1, 4'096}, {LARGE - 1, 4'095}, {LARGE - 1, 0}
    };

    for (auto [best, worst] : pairs) {
        SCOPED_TRACE(::testing::Message() << best << " and " << worst);
        auto index = this->makeIndex({worst, best}, LARGE);
        this->expectHolds(index, {best, worst}, LARGE);
        EXPECT_EQ(index.after(this->at(worst, LARGE)), NONE);
        EXPECT_EQ(index.after(this->at(worst + 1, LARGE)), this->at(worst, LARGE));

        index.remove(this->at(best, LARGE));
        this->expectHolds(index, {worst}, LARGE);

        index.remove(this->at(worst, LARGE));
        this->expectHolds(index, {}, LARGE);
    }
}

TYPED_TEST(SideIndexTest, Edges_OneOfTwoInAWordRemoved_OtherIsStillFound) {
    auto index = this->makeIndex({10, 5'000, 5'001, 8'000}, LARGE);

    index.remove(this->at(5'001, LARGE));
    this->expectHolds(index, {8'000, 5'000, 10}, LARGE);

    index.remove(this->at(5'000, LARGE));
    this->expectHolds(index, {8'000, 10}, LARGE);
}

TYPED_TEST(SideIndexTest, EveryPosition_WalkedInOrder) {
    Positions all;
    for (std::size_t position {LARGE}; position > 0; --position)
        all.push_back(position - 1);

    this->expectHolds(this->makeIndex(all, LARGE), all, LARGE);
}

TYPED_TEST(SideIndexTest, HundredThousandPositions_FirstAndLast_CrossTheGap) {
    constexpr std::size_t POSITIONS = 100'000;
    auto index = this->makeIndex({0, POSITIONS - 1}, POSITIONS);

    EXPECT_EQ(this->walk(index), this->at(Positions {POSITIONS - 1, 0}, POSITIONS));
    EXPECT_EQ(index.after(this->at(50'000, POSITIONS)), this->at(0, POSITIONS));

    index.remove(this->at(POSITIONS - 1, POSITIONS));

    EXPECT_EQ(index.best(), this->at(0, POSITIONS));
}

// --------------------------
// Dropping positions
// --------------------------
TYPED_TEST(SideIndexTest, ShiftDown_BestKept_BestMovesDown) {
    auto index = this->makeIndex({2, 5, 8});

    this->shiftDown(index, 2);

    this->expectHolds(index, {6, 3, 0});
    EXPECT_EQ(index.best(), this->at(6));
}

TYPED_TEST(SideIndexTest, ShiftUp_BestKept_BestMovesUp) {
    auto index = this->makeIndex({2, 5, 6});

    this->shiftUp(index, 3);

    this->expectHolds(index, {9, 8, 5});
    EXPECT_EQ(index.best(), this->at(9));
}

TYPED_TEST(SideIndexTest, ShiftDown_WorsePositionsDropped_BestMoves) {
    auto index = this->makeIndex({1, 5, 8});

    this->shiftDown(index, 3);

    this->expectHolds(index, {5, 2});
    EXPECT_EQ(index.best(), this->at(5));
}

TYPED_TEST(SideIndexTest, ShiftUp_BestDropped_NextOneTakesOver) {
    auto index = this->makeIndex({2, 5, 8});

    this->shiftUp(index, 3);

    this->expectHolds(index, {8, 5});
    EXPECT_EQ(index.best(), this->at(8));
}

TYPED_TEST(SideIndexTest, ShiftUp_SeveralBestDropped_OnlyTheWorstIsLeft) {
    auto index = this->makeIndex({2, 5, 8});

    this->shiftUp(index, 5);

    this->expectHolds(index, {7});
    EXPECT_EQ(index.best(), this->at(7));
}

TYPED_TEST(SideIndexTest, Shift_EverythingDropped_BestIsNone) {
    for (std::size_t n : {SMALL, SMALL + 1, std::numeric_limits<std::size_t>::max()}) {
        SCOPED_TRACE(n);
        auto down = this->makeIndex({2, 5, 8});
        auto up = this->makeIndex({2, 5, 8});

        this->shiftDown(down, n);
        this->shiftUp(up, n);

        this->expectHolds(down, {});
        this->expectHolds(up, {});
        EXPECT_EQ(down.best(), NONE);
        EXPECT_EQ(up.best(), NONE);
    }
}

TYPED_TEST(SideIndexTest, Shift_OnlyTheBestDropped_IsNone) {
    auto down = this->makeIndex({0});
    auto up = this->makeIndex({SMALL - 1});

    this->shiftDown(down, 1);
    this->shiftUp(up, 1);

    EXPECT_EQ(down.best(), NONE);
    EXPECT_EQ(up.best(), NONE);
}

TYPED_TEST(SideIndexTest, Shift_Zero_ChangesNothing) {
    auto index = this->makeIndex({2, 5, 8});

    this->shiftDown(index, 0);
    this->shiftUp(index, 0);

    this->expectHolds(index, {8, 5, 2});
    EXPECT_EQ(index.best(), this->at(8));
}

TYPED_TEST(SideIndexTest, Shift_ThenAfter_WalksTheShiftedPositions) {
    auto down = this->makeIndex({1, 4, 7});
    auto up = this->makeIndex({1, 4, 7});

    this->shiftDown(down, 1);
    this->shiftUp(up, 1);

    EXPECT_EQ(down.best(), this->at(6));
    EXPECT_EQ(down.after(this->at(6)), this->at(3));
    EXPECT_EQ(down.after(this->at(3)), this->at(0));
    EXPECT_EQ(down.after(this->at(0)), NONE);
    EXPECT_EQ(up.best(), this->at(8));
    EXPECT_EQ(up.after(this->at(8)), this->at(5));
    EXPECT_EQ(up.after(this->at(5)), this->at(2));
    EXPECT_EQ(up.after(this->at(2)), NONE);
}

TYPED_TEST(SideIndexTest, Shift_ThenAddAndRemove_KeepsTheBestRight) {
    auto index = this->makeIndex({2, 5, 8});
    this->shiftDown(index, 2);

    index.add(this->at(9));
    index.remove(this->at(9));
    index.remove(this->at(6));

    this->expectHolds(index, {3, 0});
}

TYPED_TEST(SideIndexTest, Shift_AcrossWords_BestAndWalkFollow) {
    auto down = this->makeIndex({10, 5'000, 8'000}, LARGE);
    auto up = this->makeIndex({10, 5'000, 8'000}, LARGE);

    this->shiftDown(down, 4'090);
    this->shiftUp(up, 3'000);

    this->expectHolds(down, {3'910, 910}, LARGE);
    this->expectHolds(up, {8'000, 3'010}, LARGE);
}

// --------------------------
// Copy and move
// --------------------------
TYPED_TEST(SideIndexTest, Traits_CopyIsDeletedAndMoveIsAllowed) {
    using Index = SideIndex<TypeParam::value>;

    EXPECT_FALSE(std::is_copy_constructible_v<Index>);
    EXPECT_FALSE(std::is_copy_assignable_v<Index>);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<Index>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<Index>);
}

TYPED_TEST(SideIndexTest, MoveConstruct_TargetHoldsEverythingAndStaysUsable) {
    auto source = this->makeIndex({2, 5, 8});

    auto target(std::move(source));

    this->expectHolds(target, {8, 5, 2});
    EXPECT_EQ(source.best(), NONE);

    target.add(this->at(9));
    target.remove(this->at(5));
    this->expectHolds(target, {9, 8, 2});
}

TYPED_TEST(SideIndexTest, MoveAssign_TargetLosesItsOldContent) {
    auto source = this->makeIndex({2, 5});
    auto target = this->makeIndex({0, 7, 8});

    target = std::move(source);

    this->expectHolds(target, {5, 2});
    EXPECT_EQ(source.best(), NONE);
}

TYPED_TEST(SideIndexTest, MoveAssign_EmptySource_TargetBecomesEmpty) {
    auto source = this->makeIndex({});
    auto target = this->makeIndex({1, 2});

    target = std::move(source);

    this->expectHolds(target, {});
}

TYPED_TEST(SideIndexTest, MoveAssign_LargerSource_TargetTakesItsSize) {
    auto source = this->makeIndex({10, LARGE - 1}, LARGE);
    auto target = this->makeIndex({1});

    target = std::move(source);
    target.add(this->at(5'000, LARGE));

    this->expectHolds(target, {LARGE - 1, 5'000, 10}, LARGE);
}

TYPED_TEST(SideIndexTest, MoveAssign_ToItself_KeepsEverything) {
    auto index = this->makeIndex({2, 5, 8});
    auto& same = index;

    index = std::move(same);

    this->expectHolds(index, {8, 5, 2});
}

TYPED_TEST(SideIndexTest, MovedFrom_AssignedANewIndex_IsUsableAndTargetIsUntouched) {
    auto source = this->makeIndex({2, 5, 8});
    auto target(std::move(source));

    source = this->makeIndex({4, 6});
    source.add(this->at(7));

    this->expectHolds(source, {7, 6, 4});
    this->expectHolds(target, {8, 5, 2});
}

TYPED_TEST(SideIndexTest, MovedFrom_DropFrontAndBack_BestStaysNone) {
    auto source = this->makeIndex({2, 5, 8});
    auto target(std::move(source));

    source.dropFront(3);
    EXPECT_EQ(source.best(), NONE);
    source.dropBack(3);
    EXPECT_EQ(source.best(), NONE);
    this->expectHolds(target, {8, 5, 2});
}

// --------------------------
// Random operations
// --------------------------
// Compared with a set of the positions as the buys see them
template<typename Test>
static void runRandomOperations(std::size_t positions, std::size_t maxInUse, unsigned seed) {
    constexpr std::size_t STEPS = 20'000;

    std::mt19937 generator(seed);
    std::uniform_int_distribution<std::size_t> anyPosition(0, positions - 1);
    std::uniform_int_distribution<int> percent(0, 99);
    auto index = Test::makeIndex({}, positions);
    std::set<std::size_t> model;
    std::size_t emptySteps {0};

    for (std::size_t step {0}; step < STEPS; ++step) {
        bool shouldAdd = model.size() < maxInUse && (model.empty() || percent(generator) < 45);
        if (shouldAdd) {
            std::size_t position = anyPosition(generator);
            if (model.insert(position).second) index.add(Test::at(position, positions));
        } else {
            std::uniform_int_distribution<std::size_t> anyIndex(0, model.size() - 1);
            auto it = std::next(model.begin(), static_cast<std::ptrdiff_t>(anyIndex(generator)));
            index.remove(Test::at(*it, positions));
            model.erase(it);
        }

        std::size_t probe = anyPosition(generator);
        auto it = model.lower_bound(probe);
        std::size_t best = model.empty() ? NONE : *model.rbegin();
        std::size_t after = (it == model.begin()) ? NONE : *std::prev(it);
        ASSERT_EQ(index.best(), Test::at(best, positions)) << "step " << step;
        ASSERT_EQ(index.contains(Test::at(probe, positions)), model.contains(probe)) << "step " << step;
        ASSERT_EQ(index.after(Test::at(probe, positions)), Test::at(after, positions)) << "step " << step;
        if (step % 250 == 0)
            ASSERT_EQ(Test::walk(index), Test::at(Positions(model.rbegin(), model.rend()), positions)) << "step " << step;
        if (model.empty()) ++emptySteps;
    }

    // The empty state must be reached
    EXPECT_GT(emptySteps, 100u);
}

TYPED_TEST(SideIndexTest, RandomOperationsSparse_MatchASet) {
    runRandomOperations<TestFixture>(20'000, 12, 12345);
}

TYPED_TEST(SideIndexTest, RandomOperationsInFewWords_MatchASet) {
    runRandomOperations<TestFixture>(130, 6, 777);
}
