#include <gtest/gtest.h>
#include <cstddef>
#include <iterator>
#include <random>
#include <set>
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

template<Side side>
static SideIndex<side> makeIndex(std::size_t positions, const Positions& contained) {
    SideIndex<side> index(positions);
    for (std::size_t position : contained)
        index.add(position);
    return index;
}

// Caps the walk in case after never ends
constexpr std::size_t WALK_LIMIT = 1'000;

template<Side side>
static Positions walk(const SideIndex<side>& index) {
    Positions found;
    for (std::size_t position = index.best(); position != NONE && found.size() < WALK_LIMIT; position = index.after(position))
        found.push_back(position);
    return found;
}

// --------------------------
// Empty index
// --------------------------
TEST(SideIndexTest, SellIndex_NewIndex_BestReturnsNone) {
    SideIndex<Side::sell> index(SMALL);

    EXPECT_EQ(index.best(), NONE);
}

TEST(SideIndexTest, BuyIndex_NewIndex_BestReturnsNone) {
    SideIndex<Side::buy> index(SMALL);

    EXPECT_EQ(index.best(), NONE);
}

TEST(SideIndexTest, SellIndex_NewIndex_ContainsNothing) {
    SideIndex<Side::sell> index(SMALL);

    for (std::size_t position {0}; position < SMALL; ++position)
        EXPECT_FALSE(index.contains(position)) << "position " << position;
}

TEST(SideIndexTest, BuyIndex_NewIndex_ContainsNothing) {
    SideIndex<Side::buy> index(SMALL);

    for (std::size_t position {0}; position < SMALL; ++position)
        EXPECT_FALSE(index.contains(position)) << "position " << position;
}

TEST(SideIndexTest, SellIndex_NewIndex_AfterReturnsNone) {
    SideIndex<Side::sell> index(SMALL);

    for (std::size_t position {0}; position < SMALL; ++position)
        EXPECT_EQ(index.after(position), NONE) << "position " << position;
}

TEST(SideIndexTest, BuyIndex_NewIndex_AfterReturnsNone) {
    SideIndex<Side::buy> index(SMALL);

    for (std::size_t position {0}; position < SMALL; ++position)
        EXPECT_EQ(index.after(position), NONE) << "position " << position;
}

TEST(SideIndexTest, NoneConstant_EqualsBitmapNone) {
    EXPECT_EQ(SideIndex<Side::sell>::NONE, Bitmap::NONE);
    EXPECT_EQ(SideIndex<Side::buy>::NONE, Bitmap::NONE);
}

// --------------------------
// One position
// --------------------------
TEST(SideIndexTest, SellIndex_OnePosition_BestIsIt) {
    SideIndex<Side::sell> index(SMALL);

    index.add(5);

    EXPECT_EQ(index.best(), 5u);
    EXPECT_TRUE(index.contains(5));
    EXPECT_FALSE(index.contains(4));
    EXPECT_FALSE(index.contains(6));
}

TEST(SideIndexTest, BuyIndex_OnePosition_BestIsIt) {
    SideIndex<Side::buy> index(SMALL);

    index.add(5);

    EXPECT_EQ(index.best(), 5u);
    EXPECT_TRUE(index.contains(5));
    EXPECT_FALSE(index.contains(4));
    EXPECT_FALSE(index.contains(6));
}

TEST(SideIndexTest, SellIndex_OnlyFirstPosition_BestIsZero) {
    SideIndex<Side::sell> index(SMALL);

    index.add(0);

    EXPECT_EQ(index.best(), 0u);
    EXPECT_TRUE(index.contains(0));
    EXPECT_EQ(index.after(0), NONE);
}

TEST(SideIndexTest, BuyIndex_OnlyFirstPosition_BestIsZero) {
    SideIndex<Side::buy> index(SMALL);

    index.add(0);

    EXPECT_EQ(index.best(), 0u);
    EXPECT_TRUE(index.contains(0));
    EXPECT_EQ(index.after(0), NONE);
}

TEST(SideIndexTest, SellIndex_OnlyLastPosition_BestIsLast) {
    SideIndex<Side::sell> index(SMALL);

    index.add(SMALL - 1);

    EXPECT_EQ(index.best(), SMALL - 1);
    EXPECT_TRUE(index.contains(SMALL - 1));
    EXPECT_EQ(index.after(SMALL - 1), NONE);
}

TEST(SideIndexTest, BuyIndex_OnlyLastPosition_BestIsLast) {
    SideIndex<Side::buy> index(SMALL);

    index.add(SMALL - 1);

    EXPECT_EQ(index.best(), SMALL - 1);
    EXPECT_TRUE(index.contains(SMALL - 1));
    EXPECT_EQ(index.after(SMALL - 1), NONE);
}

TEST(SideIndexTest, SellIndex_OnePosition_AfterItReturnsNone) {
    SideIndex<Side::sell> index(SMALL);

    index.add(5);

    EXPECT_EQ(index.after(5), NONE);
}

TEST(SideIndexTest, BuyIndex_OnePosition_AfterItReturnsNone) {
    SideIndex<Side::buy> index(SMALL);

    index.add(5);

    EXPECT_EQ(index.after(5), NONE);
}

// --------------------------
// Add
// --------------------------
TEST(SideIndexTest, SellIndex_AddLowerPosition_BecomesBest) {
    SideIndex<Side::sell> index(SMALL);

    index.add(5);
    index.add(2);

    EXPECT_EQ(index.best(), 2u);
    EXPECT_TRUE(index.contains(5));
}

TEST(SideIndexTest, BuyIndex_AddHigherPosition_BecomesBest) {
    SideIndex<Side::buy> index(SMALL);

    index.add(5);
    index.add(8);

    EXPECT_EQ(index.best(), 8u);
    EXPECT_TRUE(index.contains(5));
}

TEST(SideIndexTest, SellIndex_AddHigherPosition_BestStaysTheSame) {
    SideIndex<Side::sell> index(SMALL);

    index.add(5);
    index.add(8);

    EXPECT_EQ(index.best(), 5u);
    EXPECT_TRUE(index.contains(8));
}

TEST(SideIndexTest, BuyIndex_AddLowerPosition_BestStaysTheSame) {
    SideIndex<Side::buy> index(SMALL);

    index.add(5);
    index.add(2);

    EXPECT_EQ(index.best(), 5u);
    EXPECT_TRUE(index.contains(2));
}

TEST(SideIndexTest, SellIndex_AddBetweenTwo_BestStaysAndOrderIsKept) {
    SideIndex<Side::sell> index(SMALL);

    index.add(2);
    index.add(8);
    index.add(5);

    EXPECT_EQ(index.best(), 2u);
    EXPECT_EQ(walk(index), (Positions {2, 5, 8}));
}

TEST(SideIndexTest, BuyIndex_AddBetweenTwo_BestStaysAndOrderIsKept) {
    SideIndex<Side::buy> index(SMALL);

    index.add(2);
    index.add(8);
    index.add(5);

    EXPECT_EQ(index.best(), 8u);
    EXPECT_EQ(walk(index), (Positions {8, 5, 2}));
}

TEST(SideIndexTest, SellIndex_AddedPosition_IsContained) {
    SideIndex<Side::sell> index(SMALL);

    index.add(3);

    EXPECT_TRUE(index.contains(3));
    EXPECT_FALSE(index.contains(2));
}

TEST(SideIndexTest, BuyIndex_AddedPosition_IsContained) {
    SideIndex<Side::buy> index(SMALL);

    index.add(3);

    EXPECT_TRUE(index.contains(3));
    EXPECT_FALSE(index.contains(2));
}

// --------------------------
// Remove
// --------------------------
TEST(SideIndexTest, SellIndex_RemoveBest_NextOneBecomesBest) {
    auto index = makeIndex<Side::sell>(SMALL, {2, 5, 8});

    index.remove(2);

    EXPECT_EQ(index.best(), 5u);
    EXPECT_FALSE(index.contains(2));
    EXPECT_EQ(walk(index), (Positions {5, 8}));
}

TEST(SideIndexTest, BuyIndex_RemoveBest_NextOneBecomesBest) {
    auto index = makeIndex<Side::buy>(SMALL, {2, 5, 8});

    index.remove(8);

    EXPECT_EQ(index.best(), 5u);
    EXPECT_FALSE(index.contains(8));
    EXPECT_EQ(walk(index), (Positions {5, 2}));
}

TEST(SideIndexTest, SellIndex_RemoveMiddle_BestStaysAndGapIsSkipped) {
    auto index = makeIndex<Side::sell>(SMALL, {2, 5, 8});

    index.remove(5);

    EXPECT_EQ(index.best(), 2u);
    EXPECT_FALSE(index.contains(5));
    EXPECT_EQ(walk(index), (Positions {2, 8}));
}

TEST(SideIndexTest, BuyIndex_RemoveMiddle_BestStaysAndGapIsSkipped) {
    auto index = makeIndex<Side::buy>(SMALL, {2, 5, 8});

    index.remove(5);

    EXPECT_EQ(index.best(), 8u);
    EXPECT_FALSE(index.contains(5));
    EXPECT_EQ(walk(index), (Positions {8, 2}));
}

TEST(SideIndexTest, SellIndex_RemoveWorst_BestStaysAndWalkEndsEarlier) {
    auto index = makeIndex<Side::sell>(SMALL, {2, 5, 8});

    index.remove(8);

    EXPECT_EQ(index.best(), 2u);
    EXPECT_FALSE(index.contains(8));
    EXPECT_EQ(index.after(5), NONE);
}

TEST(SideIndexTest, BuyIndex_RemoveWorst_BestStaysAndWalkEndsEarlier) {
    auto index = makeIndex<Side::buy>(SMALL, {2, 5, 8});

    index.remove(2);

    EXPECT_EQ(index.best(), 8u);
    EXPECT_FALSE(index.contains(2));
    EXPECT_EQ(index.after(5), NONE);
}

TEST(SideIndexTest, SellIndex_RemoveOnlyPosition_IndexIsEmpty) {
    auto index = makeIndex<Side::sell>(SMALL, {5});

    index.remove(5);

    EXPECT_EQ(index.best(), NONE);
    EXPECT_FALSE(index.contains(5));
    EXPECT_EQ(index.after(0), NONE);
}

TEST(SideIndexTest, BuyIndex_RemoveOnlyPosition_IndexIsEmpty) {
    auto index = makeIndex<Side::buy>(SMALL, {5});

    index.remove(5);

    EXPECT_EQ(index.best(), NONE);
    EXPECT_FALSE(index.contains(5));
    EXPECT_EQ(index.after(SMALL - 1), NONE);
}

TEST(SideIndexTest, SellIndex_AddAgainAfterEmpty_WorksFromScratch) {
    auto index = makeIndex<Side::sell>(SMALL, {5});
    index.remove(5);

    index.add(7);

    EXPECT_EQ(index.best(), 7u);
    EXPECT_FALSE(index.contains(5));
    EXPECT_EQ(walk(index), (Positions {7}));
}

TEST(SideIndexTest, BuyIndex_AddAgainAfterEmpty_WorksFromScratch) {
    auto index = makeIndex<Side::buy>(SMALL, {5});
    index.remove(5);

    index.add(3);

    EXPECT_EQ(index.best(), 3u);
    EXPECT_FALSE(index.contains(5));
    EXPECT_EQ(walk(index), (Positions {3}));
}

TEST(SideIndexTest, SellIndex_AddSamePositionAgainAfterRemove_IsContained) {
    auto index = makeIndex<Side::sell>(SMALL, {4, 6});
    index.remove(4);

    index.add(4);

    EXPECT_EQ(index.best(), 4u);
    EXPECT_EQ(walk(index), (Positions {4, 6}));
}

TEST(SideIndexTest, BuyIndex_AddSamePositionAgainAfterRemove_IsContained) {
    auto index = makeIndex<Side::buy>(SMALL, {4, 6});
    index.remove(6);

    index.add(6);

    EXPECT_EQ(index.best(), 6u);
    EXPECT_EQ(walk(index), (Positions {6, 4}));
}

// --------------------------
// After
// --------------------------
TEST(SideIndexTest, SellIndex_After_StepsFromBestToWorst) {
    auto index = makeIndex<Side::sell>(SMALL, {2, 5, 8});

    EXPECT_EQ(index.after(2), 5u);
    EXPECT_EQ(index.after(5), 8u);
    EXPECT_EQ(index.after(8), NONE);
}

TEST(SideIndexTest, BuyIndex_After_StepsFromBestToWorst) {
    auto index = makeIndex<Side::buy>(SMALL, {2, 5, 8});

    EXPECT_EQ(index.after(8), 5u);
    EXPECT_EQ(index.after(5), 2u);
    EXPECT_EQ(index.after(2), NONE);
}

TEST(SideIndexTest, SellIndex_After_NeighbourIsNotInclusive) {
    auto index = makeIndex<Side::sell>(SMALL, {4, 5, 6});

    EXPECT_EQ(index.after(5), 6u);
}

TEST(SideIndexTest, BuyIndex_After_NeighbourIsNotInclusive) {
    auto index = makeIndex<Side::buy>(SMALL, {4, 5, 6});

    EXPECT_EQ(index.after(5), 4u);
}

TEST(SideIndexTest, SellIndex_After_BothEndsOfTheRange) {
    auto index = makeIndex<Side::sell>(SMALL, {0, SMALL - 1});

    EXPECT_EQ(index.after(0), SMALL - 1);
    EXPECT_EQ(index.after(SMALL - 1), NONE);
}

TEST(SideIndexTest, BuyIndex_After_BothEndsOfTheRange) {
    auto index = makeIndex<Side::buy>(SMALL, {0, SMALL - 1});

    EXPECT_EQ(index.after(SMALL - 1), 0u);
    EXPECT_EQ(index.after(0), NONE);
}

TEST(SideIndexTest, SellIndex_After_PositionNotContainedInAGap) {
    auto index = makeIndex<Side::sell>(SMALL, {2, 8});

    EXPECT_EQ(index.after(3), 8u);
    EXPECT_EQ(index.after(7), 8u);
}

TEST(SideIndexTest, BuyIndex_After_PositionNotContainedInAGap) {
    auto index = makeIndex<Side::buy>(SMALL, {2, 8});

    EXPECT_EQ(index.after(7), 2u);
    EXPECT_EQ(index.after(3), 2u);
}

TEST(SideIndexTest, SellIndex_After_PositionBetterThanBest_ReturnsBest) {
    auto index = makeIndex<Side::sell>(SMALL, {5, 8});

    EXPECT_EQ(index.after(0), 5u);
    EXPECT_EQ(index.after(4), 5u);
}

TEST(SideIndexTest, BuyIndex_After_PositionBetterThanBest_ReturnsBest) {
    auto index = makeIndex<Side::buy>(SMALL, {2, 5});

    EXPECT_EQ(index.after(SMALL - 1), 5u);
    EXPECT_EQ(index.after(6), 5u);
}

TEST(SideIndexTest, SellIndex_After_PositionWorseThanWorst_ReturnsNone) {
    auto index = makeIndex<Side::sell>(SMALL, {2, 5});

    EXPECT_EQ(index.after(6), NONE);
    EXPECT_EQ(index.after(SMALL - 1), NONE);
}

TEST(SideIndexTest, BuyIndex_After_PositionWorseThanWorst_ReturnsNone) {
    auto index = makeIndex<Side::buy>(SMALL, {5, 8});

    EXPECT_EQ(index.after(4), NONE);
    EXPECT_EQ(index.after(0), NONE);
}

// --------------------------
// Sparse
// --------------------------
TEST(SideIndexTest, SellIndex_HugeRangeFirstAndLast_BestAndAfterCrossTheGap) {
    auto index = makeIndex<Side::sell>(100'000, {0, 99'999});

    EXPECT_EQ(index.best(), 0u);
    EXPECT_EQ(index.after(0), 99'999u);
    EXPECT_EQ(index.after(99'999), NONE);
}

TEST(SideIndexTest, BuyIndex_HugeRangeFirstAndLast_BestAndAfterCrossTheGap) {
    auto index = makeIndex<Side::buy>(100'000, {0, 99'999});

    EXPECT_EQ(index.best(), 99'999u);
    EXPECT_EQ(index.after(99'999), 0u);
    EXPECT_EQ(index.after(0), NONE);
}

TEST(SideIndexTest, SellIndex_HugeRangeRemoveBest_BestJumpsToTheOtherEnd) {
    auto index = makeIndex<Side::sell>(100'000, {0, 99'999});

    index.remove(0);

    EXPECT_EQ(index.best(), 99'999u);
}

TEST(SideIndexTest, BuyIndex_HugeRangeRemoveBest_BestJumpsToTheOtherEnd) {
    auto index = makeIndex<Side::buy>(100'000, {0, 99'999});

    index.remove(99'999);

    EXPECT_EQ(index.best(), 0u);
}

TEST(SideIndexTest, SellIndex_HugeRangeFromEmptyPosition_AfterCrossesTheGap) {
    auto index = makeIndex<Side::sell>(100'000, {99'999});

    EXPECT_EQ(index.after(0), 99'999u);
    EXPECT_EQ(index.after(50'000), 99'999u);
}

TEST(SideIndexTest, BuyIndex_HugeRangeFromEmptyPosition_AfterCrossesTheGap) {
    auto index = makeIndex<Side::buy>(100'000, {0});

    EXPECT_EQ(index.after(99'999), 0u);
    EXPECT_EQ(index.after(50'000), 0u);
}

TEST(SideIndexTest, SellIndex_PositionsAroundA4096Boundary_BothFound) {
    auto index = makeIndex<Side::sell>(8'192, {4'095, 4'096});

    EXPECT_EQ(index.best(), 4'095u);
    EXPECT_EQ(index.after(4'095), 4'096u);
    EXPECT_EQ(index.after(4'096), NONE);
}

TEST(SideIndexTest, BuyIndex_PositionsAroundA4096Boundary_BothFound) {
    auto index = makeIndex<Side::buy>(8'192, {4'095, 4'096});

    EXPECT_EQ(index.best(), 4'096u);
    EXPECT_EQ(index.after(4'096), 4'095u);
    EXPECT_EQ(index.after(4'095), NONE);
}

TEST(SideIndexTest, SellIndex_RemoveBestAroundA4096Boundary_OtherIsBest) {
    auto index = makeIndex<Side::sell>(8'192, {4'095, 4'096});

    index.remove(4'095);

    EXPECT_EQ(index.best(), 4'096u);
}

TEST(SideIndexTest, BuyIndex_RemoveBestAroundA4096Boundary_OtherIsBest) {
    auto index = makeIndex<Side::buy>(8'192, {4'095, 4'096});

    index.remove(4'096);

    EXPECT_EQ(index.best(), 4'095u);
}

TEST(SideIndexTest, SellIndex_PositionsAroundA64Boundary_BothFound) {
    auto index = makeIndex<Side::sell>(200, {63, 64});

    EXPECT_EQ(index.best(), 63u);
    EXPECT_EQ(index.after(63), 64u);
    EXPECT_EQ(index.after(64), NONE);
}

TEST(SideIndexTest, BuyIndex_PositionsAroundA64Boundary_BothFound) {
    auto index = makeIndex<Side::buy>(200, {63, 64});

    EXPECT_EQ(index.best(), 64u);
    EXPECT_EQ(index.after(64), 63u);
    EXPECT_EQ(index.after(63), NONE);
}

TEST(SideIndexTest, SellIndex_RemoveOneOfTwoInTheSameWord_OtherStillFound) {
    auto index = makeIndex<Side::sell>(100'000, {5'000, 5'001, 90'000});

    index.remove(5'000);

    EXPECT_EQ(index.best(), 5'001u);
    EXPECT_EQ(index.after(5'001), 90'000u);
}

TEST(SideIndexTest, BuyIndex_RemoveOneOfTwoInTheSameWord_OtherStillFound) {
    auto index = makeIndex<Side::buy>(100'000, {5'000, 5'001, 90'000});

    index.remove(5'001);

    EXPECT_EQ(index.best(), 90'000u);
    EXPECT_EQ(index.after(90'000), 5'000u);
}

TEST(SideIndexTest, SellIndex_RemoveWholeWordInTheMiddle_WalkSkipsIt) {
    auto index = makeIndex<Side::sell>(100'000, {10, 5'000, 5'001, 90'000});

    index.remove(5'000);
    index.remove(5'001);

    EXPECT_EQ(walk(index), (Positions {10, 90'000}));
}

TEST(SideIndexTest, BuyIndex_RemoveWholeWordInTheMiddle_WalkSkipsIt) {
    auto index = makeIndex<Side::buy>(100'000, {10, 5'000, 5'001, 90'000});

    index.remove(5'000);
    index.remove(5'001);

    EXPECT_EQ(walk(index), (Positions {90'000, 10}));
}

// --------------------------
// Walking
// --------------------------
TEST(SideIndexTest, SellIndex_Walk_VisitsEverythingLowestFirst) {
    auto index = makeIndex<Side::sell>(SMALL, {7, 3, 9, 0, 5});

    EXPECT_EQ(walk(index), (Positions {0, 3, 5, 7, 9}));
}

TEST(SideIndexTest, BuyIndex_Walk_VisitsEverythingHighestFirst) {
    auto index = makeIndex<Side::buy>(SMALL, {7, 3, 9, 0, 5});

    EXPECT_EQ(walk(index), (Positions {9, 7, 5, 3, 0}));
}

TEST(SideIndexTest, SellIndex_EveryPositionContained_WalkVisitsAllInOrder) {
    Positions all;
    for (std::size_t position {0}; position < 200; ++position)
        all.push_back(position);
    auto index = makeIndex<Side::sell>(200, all);

    EXPECT_EQ(walk(index), all);
}

TEST(SideIndexTest, BuyIndex_EveryPositionContained_WalkVisitsAllInOrder) {
    Positions all;
    for (std::size_t position {200}; position > 0; --position)
        all.push_back(position - 1);
    auto index = makeIndex<Side::buy>(200, all);

    EXPECT_EQ(walk(index), all);
}

TEST(SideIndexTest, SellIndex_RemoveBestRepeatedly_DrainsLowestFirst) {
    auto index = makeIndex<Side::sell>(SMALL, {7, 3, 9, 0, 5});
    Positions drained;

    while (index.best() != NONE && drained.size() < SMALL) {
        drained.push_back(index.best());
        index.remove(index.best());
    }

    EXPECT_EQ(drained, (Positions {0, 3, 5, 7, 9}));
}

TEST(SideIndexTest, BuyIndex_RemoveBestRepeatedly_DrainsHighestFirst) {
    auto index = makeIndex<Side::buy>(SMALL, {7, 3, 9, 0, 5});
    Positions drained;

    while (index.best() != NONE && drained.size() < SMALL) {
        drained.push_back(index.best());
        index.remove(index.best());
    }

    EXPECT_EQ(drained, (Positions {9, 7, 5, 3, 0}));
}

// --------------------------
// Copy and move
// --------------------------
TEST(SideIndexTest, Traits_CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<SideIndex<Side::sell>>);
    EXPECT_FALSE(std::is_copy_assignable_v<SideIndex<Side::sell>>);
    EXPECT_TRUE(std::is_move_constructible_v<SideIndex<Side::sell>>);
    EXPECT_TRUE(std::is_move_assignable_v<SideIndex<Side::sell>>);
    EXPECT_FALSE(std::is_copy_constructible_v<SideIndex<Side::buy>>);
    EXPECT_FALSE(std::is_copy_assignable_v<SideIndex<Side::buy>>);
    EXPECT_TRUE(std::is_move_constructible_v<SideIndex<Side::buy>>);
    EXPECT_TRUE(std::is_move_assignable_v<SideIndex<Side::buy>>);
}

TEST(SideIndexTest, SellIndex_MoveConstruct_TargetKeepsEverything) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});

    SideIndex<Side::sell> target(std::move(source));

    EXPECT_EQ(target.best(), 2u);
    EXPECT_EQ(walk(target), (Positions {2, 5, 8}));
    EXPECT_TRUE(target.contains(5));
    EXPECT_FALSE(target.contains(4));
}

TEST(SideIndexTest, BuyIndex_MoveConstruct_TargetKeepsEverything) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});

    SideIndex<Side::buy> target(std::move(source));

    EXPECT_EQ(target.best(), 8u);
    EXPECT_EQ(walk(target), (Positions {8, 5, 2}));
    EXPECT_TRUE(target.contains(5));
    EXPECT_FALSE(target.contains(4));
}

TEST(SideIndexTest, SellIndex_MoveConstruct_SourceBestIsNone) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});

    SideIndex<Side::sell> target(std::move(source));

    EXPECT_EQ(source.best(), NONE);
}

TEST(SideIndexTest, BuyIndex_MoveConstruct_SourceBestIsNone) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});

    SideIndex<Side::buy> target(std::move(source));

    EXPECT_EQ(source.best(), NONE);
}

TEST(SideIndexTest, SellIndex_MoveConstruct_TargetStaysUsable) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});
    SideIndex<Side::sell> target(std::move(source));

    target.add(1);
    target.remove(5);

    EXPECT_EQ(walk(target), (Positions {1, 2, 8}));
}

TEST(SideIndexTest, BuyIndex_MoveConstruct_TargetStaysUsable) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});
    SideIndex<Side::buy> target(std::move(source));

    target.add(9);
    target.remove(5);

    EXPECT_EQ(walk(target), (Positions {9, 8, 2}));
}

TEST(SideIndexTest, SellIndex_MoveConstruct_SourceAssignedNewValueIsUsable) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});
    SideIndex<Side::sell> target(std::move(source));

    source = makeIndex<Side::sell>(SMALL, {4, 6});

    EXPECT_EQ(walk(source), (Positions {4, 6}));
    EXPECT_EQ(walk(target), (Positions {2, 5, 8}));
}

TEST(SideIndexTest, BuyIndex_MoveConstruct_SourceAssignedNewValueIsUsable) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});
    SideIndex<Side::buy> target(std::move(source));

    source = makeIndex<Side::buy>(SMALL, {4, 6});

    EXPECT_EQ(walk(source), (Positions {6, 4}));
    EXPECT_EQ(walk(target), (Positions {8, 5, 2}));
}

TEST(SideIndexTest, SellIndex_MoveAssign_TargetKeepsEverything) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});
    SideIndex<Side::sell> target(SMALL);

    target = std::move(source);

    EXPECT_EQ(target.best(), 2u);
    EXPECT_EQ(walk(target), (Positions {2, 5, 8}));
}

TEST(SideIndexTest, BuyIndex_MoveAssign_TargetKeepsEverything) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});
    SideIndex<Side::buy> target(SMALL);

    target = std::move(source);

    EXPECT_EQ(target.best(), 8u);
    EXPECT_EQ(walk(target), (Positions {8, 5, 2}));
}

TEST(SideIndexTest, SellIndex_MoveAssign_TargetOldContentIsGone) {
    auto source = makeIndex<Side::sell>(SMALL, {5, 8});
    auto target = makeIndex<Side::sell>(SMALL, {1, 2, 9});

    target = std::move(source);

    EXPECT_EQ(walk(target), (Positions {5, 8}));
    EXPECT_FALSE(target.contains(1));
    EXPECT_FALSE(target.contains(2));
    EXPECT_FALSE(target.contains(9));
}

TEST(SideIndexTest, BuyIndex_MoveAssign_TargetOldContentIsGone) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5});
    auto target = makeIndex<Side::buy>(SMALL, {0, 7, 8});

    target = std::move(source);

    EXPECT_EQ(walk(target), (Positions {5, 2}));
    EXPECT_FALSE(target.contains(0));
    EXPECT_FALSE(target.contains(7));
    EXPECT_FALSE(target.contains(8));
}

TEST(SideIndexTest, SellIndex_MoveAssignEmptySource_TargetBecomesEmpty) {
    SideIndex<Side::sell> source(SMALL);
    auto target = makeIndex<Side::sell>(SMALL, {1, 2});

    target = std::move(source);

    EXPECT_EQ(target.best(), NONE);
    EXPECT_FALSE(target.contains(1));
}

TEST(SideIndexTest, BuyIndex_MoveAssignEmptySource_TargetBecomesEmpty) {
    SideIndex<Side::buy> source(SMALL);
    auto target = makeIndex<Side::buy>(SMALL, {1, 2});

    target = std::move(source);

    EXPECT_EQ(target.best(), NONE);
    EXPECT_FALSE(target.contains(1));
}

TEST(SideIndexTest, SellIndex_MoveAssignLargerSource_TargetKeepsFarPositions) {
    auto source = makeIndex<Side::sell>(100'000, {10, 99'999});
    SideIndex<Side::sell> target(SMALL);

    target = std::move(source);
    target.add(50'000);

    EXPECT_EQ(walk(target), (Positions {10, 50'000, 99'999}));
}

TEST(SideIndexTest, BuyIndex_MoveAssignLargerSource_TargetKeepsFarPositions) {
    auto source = makeIndex<Side::buy>(100'000, {10, 99'999});
    SideIndex<Side::buy> target(SMALL);

    target = std::move(source);
    target.add(50'000);

    EXPECT_EQ(walk(target), (Positions {99'999, 50'000, 10}));
}

TEST(SideIndexTest, SellIndex_MoveAssign_SourceBestIsNone) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});
    SideIndex<Side::sell> target(SMALL);

    target = std::move(source);

    EXPECT_EQ(source.best(), NONE);
}

TEST(SideIndexTest, BuyIndex_MoveAssign_SourceBestIsNone) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});
    SideIndex<Side::buy> target(SMALL);

    target = std::move(source);

    EXPECT_EQ(source.best(), NONE);
}

TEST(SideIndexTest, SellIndex_MoveAssign_SourceAssignedNewValueIsUsable) {
    auto source = makeIndex<Side::sell>(SMALL, {2, 5, 8});
    SideIndex<Side::sell> target(SMALL);
    target = std::move(source);

    source = makeIndex<Side::sell>(SMALL, {4, 6});
    source.add(3);

    EXPECT_EQ(walk(source), (Positions {3, 4, 6}));
    EXPECT_EQ(walk(target), (Positions {2, 5, 8}));
}

TEST(SideIndexTest, BuyIndex_MoveAssign_SourceAssignedNewValueIsUsable) {
    auto source = makeIndex<Side::buy>(SMALL, {2, 5, 8});
    SideIndex<Side::buy> target(SMALL);
    target = std::move(source);

    source = makeIndex<Side::buy>(SMALL, {4, 6});
    source.add(7);

    EXPECT_EQ(walk(source), (Positions {7, 6, 4}));
    EXPECT_EQ(walk(target), (Positions {8, 5, 2}));
}

// --------------------------
// Random operations
// --------------------------
template<Side side>
static std::size_t modelBest(const std::set<std::size_t>& model) {
    if (model.empty()) return NONE;
    return (side == Side::sell) ? *model.begin() : *model.rbegin();
}

template<Side side>
static std::size_t modelAfter(const std::set<std::size_t>& model, std::size_t position) {
    if constexpr (side == Side::sell) {
        auto it = model.upper_bound(position);
        return (it == model.end()) ? NONE : *it;
    } else {
        auto it = model.lower_bound(position);
        return (it == model.begin()) ? NONE : *std::prev(it);
    }
}

template<Side side>
static Positions modelWalk(const std::set<std::size_t>& model) {
    if constexpr (side == Side::sell)
        return Positions(model.begin(), model.end());
    else
        return Positions(model.rbegin(), model.rend());
}

template<Side side>
static void runRandomOperations(std::size_t positions, std::size_t maxInUse, unsigned seed) {
    constexpr std::size_t STEPS = 20'000;

    std::mt19937 generator(seed);
    std::uniform_int_distribution<std::size_t> anyPosition(0, positions - 1);
    std::uniform_int_distribution<int> percent(0, 99);
    SideIndex<side> index(positions);
    std::set<std::size_t> model;
    std::size_t emptySteps {0};

    for (std::size_t step {0}; step < STEPS; ++step) {
        bool shouldAdd = model.size() < maxInUse && (model.empty() || percent(generator) < 45);
        if (shouldAdd) {
            std::size_t position = anyPosition(generator);
            if (!model.contains(position)) {
                index.add(position);
                model.insert(position);
            }
        } else {
            std::uniform_int_distribution<std::size_t> anyIndex(0, model.size() - 1);
            auto it = model.begin();
            std::advance(it, anyIndex(generator));
            index.remove(*it);
            model.erase(it);
        }

        std::size_t probe = anyPosition(generator);
        ASSERT_EQ(index.best(), modelBest<side>(model)) << "step " << step;
        ASSERT_EQ(index.contains(probe), model.contains(probe)) << "step " << step;
        ASSERT_EQ(index.after(probe), modelAfter<side>(model, probe)) << "step " << step;
        if (step % 250 == 0)
            ASSERT_EQ(walk(index), modelWalk<side>(model)) << "step " << step;
        if (model.empty()) ++emptySteps;
    }

    // The empty state must be reached
    EXPECT_GT(emptySteps, 100u);
}

TEST(SideIndexTest, SellIndex_RandomOperationsSparse_MatchStdSet) {
    runRandomOperations<Side::sell>(20'000, 12, 12345);
}

TEST(SideIndexTest, BuyIndex_RandomOperationsSparse_MatchStdSet) {
    runRandomOperations<Side::buy>(20'000, 12, 12345);
}

TEST(SideIndexTest, SellIndex_RandomOperationsFewWords_MatchStdSet) {
    runRandomOperations<Side::sell>(130, 6, 777);
}

TEST(SideIndexTest, BuyIndex_RandomOperationsFewWords_MatchStdSet) {
    runRandomOperations<Side::buy>(130, 6, 777);
}
