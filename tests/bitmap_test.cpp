#include <gtest/gtest.h>
#include <cstddef>
#include <iterator>
#include <limits>
#include <random>
#include <set>
#include <utility>
#include <vector>
#include "bitmap.hpp"

using orderbook::detail::Bitmap;

using Positions = std::vector<std::size_t>;

constexpr std::size_t NONE = Bitmap::NONE;

static Positions walkUp(const Bitmap& bitmap) {
    Positions found;
    for (std::size_t position = bitmap.next(0); position != NONE; position = bitmap.next(position + 1))
        found.push_back(position);
    return found;
}

static Positions walkDown(const Bitmap& bitmap, std::size_t last) {
    Positions found;
    std::size_t position = bitmap.previous(last);
    while (position != NONE) {
        found.push_back(position);
        if (position == 0) break;
        position = bitmap.previous(position - 1);
    }
    return found;
}

// --------------------------
// New bitmap
// --------------------------
TEST(BitmapTest, NewBitmap_Test_EveryBitIsOff) {
    Bitmap bitmap(200);

    for (std::size_t position {0}; position < 200; ++position)
        EXPECT_FALSE(bitmap.test(position));
}

TEST(BitmapTest, NewBitmap_Next_ReturnsNone) {
    Bitmap bitmap(200);

    EXPECT_EQ(bitmap.next(0), NONE);
}

TEST(BitmapTest, NewBitmap_Previous_ReturnsNone) {
    Bitmap bitmap(200);

    EXPECT_EQ(bitmap.previous(199), NONE);
}

TEST(BitmapTest, ZeroPositions_Next_ReturnsNone) {
    Bitmap bitmap(0);

    EXPECT_EQ(bitmap.next(0), NONE);
}

TEST(BitmapTest, OnePosition_SetThenFind_Works) {
    Bitmap bitmap(1);

    bitmap.set(0);

    EXPECT_TRUE(bitmap.test(0));
    EXPECT_EQ(bitmap.next(0), 0u);
    EXPECT_EQ(bitmap.previous(0), 0u);
}

// --------------------------
// Set, test and clear
// --------------------------
TEST(BitmapTest, Set_OneBit_OnlyThatBitIsOn) {
    for (std::size_t position : {0u, 1u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u}) {
        Bitmap bitmap(192);

        bitmap.set(position);

        for (std::size_t other {0}; other < 192; ++other)
            EXPECT_EQ(bitmap.test(other), other == position) << "set " << position << ", test " << other;
    }
}

TEST(BitmapTest, Set_Twice_BitStaysOn) {
    Bitmap bitmap(128);

    bitmap.set(70);
    bitmap.set(70);

    EXPECT_TRUE(bitmap.test(70));
}

TEST(BitmapTest, Clear_OneBit_OnlyThatBitIsOff) {
    for (std::size_t position : {0u, 1u, 31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u}) {
        Bitmap bitmap(192);
        for (std::size_t other {0}; other < 192; ++other)
            bitmap.set(other);

        bitmap.clear(position);

        for (std::size_t other {0}; other < 192; ++other)
            EXPECT_EQ(bitmap.test(other), other != position) << "clear " << position << ", test " << other;
    }
}

TEST(BitmapTest, Clear_Twice_BitStaysOff) {
    Bitmap bitmap(128);
    bitmap.set(70);

    bitmap.clear(70);
    bitmap.clear(70);

    EXPECT_FALSE(bitmap.test(70));
}

TEST(BitmapTest, Clear_BitThatIsOff_ChangesNothing) {
    Bitmap bitmap(128);
    bitmap.set(69);
    bitmap.set(71);

    bitmap.clear(70);

    EXPECT_FALSE(bitmap.test(70));
    EXPECT_TRUE(bitmap.test(69));
    EXPECT_TRUE(bitmap.test(71));
}

TEST(BitmapTest, Clear_OneBitOfAFullWord_OthersStayOn) {
    Bitmap bitmap(64);
    for (std::size_t position {0}; position < 64; ++position)
        bitmap.set(position);

    bitmap.clear(40);

    EXPECT_EQ(walkUp(bitmap).size(), 63u);
    EXPECT_FALSE(bitmap.test(40));
}

// --------------------------
// Odd sizes
// --------------------------
TEST(BitmapTest, OddSizes_LastPosition_CanBeSetTestedAndFound) {
    for (std::size_t positions : {63u, 65u, 130u}) {
        Bitmap bitmap(positions);
        std::size_t last = positions - 1;

        bitmap.set(last);

        EXPECT_TRUE(bitmap.test(last)) << positions << " positions";
        EXPECT_EQ(bitmap.next(0), last) << positions << " positions";
        EXPECT_EQ(bitmap.previous(last), last) << positions << " positions";
    }
}

// --------------------------
// Next
// --------------------------
TEST(BitmapTest, Next_StartOnASetBit_ReturnsItself) {
    Bitmap bitmap(128);
    bitmap.set(10);

    EXPECT_EQ(bitmap.next(10), 10u);
}

TEST(BitmapTest, Next_BitBelowTheStartInTheSameWord_IsIgnored) {
    Bitmap bitmap(128);
    bitmap.set(10);

    EXPECT_EQ(bitmap.next(11), NONE);
}

TEST(BitmapTest, Next_BitLaterInTheSameWord_IsFound) {
    Bitmap bitmap(128);
    bitmap.set(50);

    EXPECT_EQ(bitmap.next(10), 50u);
}

TEST(BitmapTest, Next_BitInTheNextWord_IsFound) {
    Bitmap bitmap(192);
    bitmap.set(100);

    EXPECT_EQ(bitmap.next(10), 100u);
}

TEST(BitmapTest, Next_SeveralEmptyWordsInBetween_BitIsFound) {
    Bitmap bitmap(1'000);
    bitmap.set(900);

    EXPECT_EQ(bitmap.next(10), 900u);
}

TEST(BitmapTest, Next_AfterTheLastOne_ReturnsNone) {
    Bitmap bitmap(1'000);
    bitmap.set(500);

    EXPECT_EQ(bitmap.next(501), NONE);
    EXPECT_EQ(bitmap.next(999), NONE);
}

TEST(BitmapTest, Next_FirstPositionPastTheLastWord_ReturnsNone) {
    Bitmap bitmap(128);
    bitmap.set(127);

    EXPECT_EQ(bitmap.next(128), NONE);
}

TEST(BitmapTest, Next_HugePosition_ReturnsNone) {
    Bitmap bitmap(128);
    bitmap.set(127);

    EXPECT_EQ(bitmap.next(std::numeric_limits<std::size_t>::max()), NONE);
}

TEST(BitmapTest, Next_WordEdges_FindsTheRightBit) {
    Bitmap bitmap(256);
    bitmap.set(63);
    bitmap.set(64);
    bitmap.set(127);
    bitmap.set(128);

    EXPECT_EQ(bitmap.next(0), 63u);
    EXPECT_EQ(bitmap.next(63), 63u);
    EXPECT_EQ(bitmap.next(64), 64u);
    EXPECT_EQ(bitmap.next(65), 127u);
    EXPECT_EQ(bitmap.next(127), 127u);
    EXPECT_EQ(bitmap.next(128), 128u);
    EXPECT_EQ(bitmap.next(129), NONE);
}

// --------------------------
// Previous
// --------------------------
TEST(BitmapTest, Previous_StartOnASetBit_ReturnsItself) {
    Bitmap bitmap(128);
    bitmap.set(10);

    EXPECT_EQ(bitmap.previous(10), 10u);
}

TEST(BitmapTest, Previous_BitAboveTheStartInTheSameWord_IsIgnored) {
    Bitmap bitmap(128);
    bitmap.set(10);

    EXPECT_EQ(bitmap.previous(9), NONE);
}

TEST(BitmapTest, Previous_BitEarlierInTheSameWord_IsFound) {
    Bitmap bitmap(128);
    bitmap.set(10);

    EXPECT_EQ(bitmap.previous(50), 10u);
}

TEST(BitmapTest, Previous_BitInThePreviousWord_IsFound) {
    Bitmap bitmap(192);
    bitmap.set(10);

    EXPECT_EQ(bitmap.previous(100), 10u);
}

TEST(BitmapTest, Previous_SeveralEmptyWordsInBetween_BitIsFound) {
    Bitmap bitmap(1'000);
    bitmap.set(10);

    EXPECT_EQ(bitmap.previous(900), 10u);
}

TEST(BitmapTest, Previous_BeforeTheFirstOne_ReturnsNone) {
    Bitmap bitmap(1'000);
    bitmap.set(500);

    EXPECT_EQ(bitmap.previous(499), NONE);
    EXPECT_EQ(bitmap.previous(0), NONE);
}

TEST(BitmapTest, Previous_PositionZeroOnASetBit_ReturnsZero) {
    Bitmap bitmap(128);
    bitmap.set(0);

    EXPECT_EQ(bitmap.previous(0), 0u);
}

TEST(BitmapTest, Previous_PositionZeroOnAnOffBit_ReturnsNone) {
    Bitmap bitmap(128);
    bitmap.set(1);

    EXPECT_EQ(bitmap.previous(0), NONE);
}

TEST(BitmapTest, Previous_WordEdges_FindsTheRightBit) {
    Bitmap bitmap(256);
    bitmap.set(63);
    bitmap.set(64);
    bitmap.set(127);
    bitmap.set(128);

    EXPECT_EQ(bitmap.previous(255), 128u);
    EXPECT_EQ(bitmap.previous(128), 128u);
    EXPECT_EQ(bitmap.previous(127), 127u);
    EXPECT_EQ(bitmap.previous(126), 64u);
    EXPECT_EQ(bitmap.previous(64), 64u);
    EXPECT_EQ(bitmap.previous(63), 63u);
    EXPECT_EQ(bitmap.previous(62), NONE);
}

// --------------------------
// Far gaps
// --------------------------
TEST(BitmapTest, Next_FirstAndLastOfHundredThousand_JumpsTheGap) {
    Bitmap bitmap(100'000);
    bitmap.set(0);
    bitmap.set(99'999);

    EXPECT_EQ(bitmap.next(1), 99'999u);
}

TEST(BitmapTest, Previous_FirstAndLastOfHundredThousand_JumpsTheGap) {
    Bitmap bitmap(100'000);
    bitmap.set(0);
    bitmap.set(99'999);

    EXPECT_EQ(bitmap.previous(99'998), 0u);
}

TEST(BitmapTest, Next_FirstAndLastOfOneMillion_JumpsTheGap) {
    Bitmap bitmap(1'000'000);
    bitmap.set(0);
    bitmap.set(999'999);

    EXPECT_EQ(bitmap.next(1), 999'999u);
}

TEST(BitmapTest, Previous_FirstAndLastOfOneMillion_JumpsTheGap) {
    Bitmap bitmap(1'000'000);
    bitmap.set(0);
    bitmap.set(999'999);

    EXPECT_EQ(bitmap.previous(999'998), 0u);
}

TEST(BitmapTest, Next_BitsAroundPosition4096_FindsTheRightBit) {
    Bitmap bitmap(8'192);
    bitmap.set(4'095);
    bitmap.set(4'096);

    EXPECT_EQ(bitmap.next(0), 4'095u);
    EXPECT_EQ(bitmap.next(4'096), 4'096u);
    EXPECT_EQ(bitmap.next(4'097), NONE);
}

TEST(BitmapTest, Previous_BitsAroundPosition4096_FindsTheRightBit) {
    Bitmap bitmap(8'192);
    bitmap.set(4'095);
    bitmap.set(4'096);

    EXPECT_EQ(bitmap.previous(8'191), 4'096u);
    EXPECT_EQ(bitmap.previous(4'095), 4'095u);
    EXPECT_EQ(bitmap.previous(4'094), NONE);
}

TEST(BitmapTest, Next_BitsAroundPosition262144_FindsTheRightBit) {
    Bitmap bitmap(524'288);
    bitmap.set(262'143);
    bitmap.set(262'144);

    EXPECT_EQ(bitmap.next(0), 262'143u);
    EXPECT_EQ(bitmap.next(262'144), 262'144u);
    EXPECT_EQ(bitmap.next(262'145), NONE);
}

TEST(BitmapTest, Previous_BitsAroundPosition262144_FindsTheRightBit) {
    Bitmap bitmap(524'288);
    bitmap.set(262'143);
    bitmap.set(262'144);

    EXPECT_EQ(bitmap.previous(524'287), 262'144u);
    EXPECT_EQ(bitmap.previous(262'143), 262'143u);
    EXPECT_EQ(bitmap.previous(262'142), NONE);
}

// 4096 positions are 64 words, so the word after the last is past the summary
TEST(BitmapTest, Next_EmptyLastWordOf4096Positions_ReturnsNone) {
    Bitmap bitmap(4'096);
    bitmap.set(0);

    EXPECT_EQ(bitmap.next(1), NONE);
    EXPECT_EQ(bitmap.next(4'095), NONE);
}

TEST(BitmapTest, Next_LastBitCleared_ReturnsNone) {
    Bitmap bitmap(1'000'000);
    bitmap.set(999'999);
    bitmap.clear(999'999);

    EXPECT_EQ(bitmap.next(0), NONE);
}

TEST(BitmapTest, Previous_LastBitCleared_ReturnsNone) {
    Bitmap bitmap(1'000'000);
    bitmap.set(999'999);
    bitmap.clear(999'999);

    EXPECT_EQ(bitmap.previous(999'999), NONE);
}

// --------------------------
// Clear and set again
// --------------------------
TEST(BitmapTest, Clear_ThenNext_NoLongerFindsTheBit) {
    Bitmap bitmap(200);
    bitmap.set(100);

    bitmap.clear(100);

    EXPECT_EQ(bitmap.next(0), NONE);
}

TEST(BitmapTest, Clear_ThenPrevious_NoLongerFindsTheBit) {
    Bitmap bitmap(200);
    bitmap.set(100);

    bitmap.clear(100);

    EXPECT_EQ(bitmap.previous(199), NONE);
}

TEST(BitmapTest, Clear_OneOfTwoInAWord_NextFindsTheOther) {
    Bitmap bitmap(128);
    bitmap.set(10);
    bitmap.set(20);

    bitmap.clear(10);

    EXPECT_EQ(bitmap.next(0), 20u);
}

TEST(BitmapTest, Clear_OneOfTwoInAWord_PreviousFindsTheOther) {
    Bitmap bitmap(128);
    bitmap.set(10);
    bitmap.set(20);

    bitmap.clear(20);

    EXPECT_EQ(bitmap.previous(127), 10u);
}

TEST(BitmapTest, Set_AfterTheWordWasEmptied_IsFoundAgain) {
    Bitmap bitmap(200);
    bitmap.set(100);
    bitmap.clear(100);

    bitmap.set(100);

    EXPECT_EQ(bitmap.next(0), 100u);
    EXPECT_EQ(bitmap.previous(199), 100u);
}

// --------------------------
// Searching
// --------------------------
TEST(BitmapTest, NextAndPrevious_Called_DoNotChangeTheBitmap) {
    Bitmap bitmap(200);
    bitmap.set(5);
    bitmap.set(150);

    bitmap.next(0);
    bitmap.next(6);
    bitmap.previous(199);
    bitmap.previous(149);

    EXPECT_EQ(walkUp(bitmap), (Positions {5, 150}));
}

// --------------------------
// Walking
// --------------------------
TEST(BitmapTest, Next_Repeated_VisitsEveryBitAscending) {
    Bitmap bitmap(1'000);
    for (std::size_t position : {0u, 63u, 64u, 300u, 301u, 999u})
        bitmap.set(position);

    EXPECT_EQ(walkUp(bitmap), (Positions {0, 63, 64, 300, 301, 999}));
}

TEST(BitmapTest, Previous_Repeated_VisitsEveryBitDescending) {
    Bitmap bitmap(1'000);
    for (std::size_t position : {0u, 63u, 64u, 300u, 301u, 999u})
        bitmap.set(position);

    EXPECT_EQ(walkDown(bitmap, 999), (Positions {999, 301, 300, 64, 63, 0}));
}

// --------------------------
// Random operations
// --------------------------
TEST(BitmapTest, RandomOperations_AgreeWithStdSet) {
    constexpr std::size_t POSITIONS = 100'000;
    constexpr std::size_t POSITIONS_IN_USE = 40; // few, so words often empty and gaps are long
    constexpr int OPERATIONS = 20'000;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<std::size_t> anyPosition(0, POSITIONS - 1);
    std::uniform_int_distribution<std::size_t> anyInUse(0, POSITIONS_IN_USE - 1);
    std::bernoulli_distribution isSet(0.5);

    Positions inUse;
    for (std::size_t i {0}; i < POSITIONS_IN_USE; ++i)
        inUse.push_back(anyPosition(rng));

    Bitmap bitmap(POSITIONS);
    std::set<std::size_t> model;

    for (int i {0}; i < OPERATIONS; ++i) {
        std::size_t position = inUse[anyInUse(rng)];
        if (isSet(rng)) {
            bitmap.set(position);
            model.insert(position);
        } else {
            bitmap.clear(position);
            model.erase(position);
        }

        std::size_t probe = anyPosition(rng);
        auto above = model.lower_bound(probe);
        auto after = model.upper_bound(probe);
        std::size_t expectedNext = (above == model.end()) ? NONE : *above;
        std::size_t expectedPrevious = (after == model.begin()) ? NONE : *std::prev(after);

        ASSERT_EQ(bitmap.test(position), model.count(position) == 1) << "operation " << i;
        ASSERT_EQ(bitmap.next(probe), expectedNext) << "operation " << i << ", probe " << probe;
        ASSERT_EQ(bitmap.previous(probe), expectedPrevious) << "operation " << i << ", probe " << probe;
    }
}

// --------------------------
// Copy and move
// --------------------------
TEST(BitmapTest, Copy_ChangingTheCopy_LeavesTheOriginalAlone) {
    Bitmap original(200);
    original.set(10);

    Bitmap copy = original;
    copy.set(20);
    copy.clear(10);

    EXPECT_TRUE(original.test(10));
    EXPECT_FALSE(original.test(20));
    EXPECT_FALSE(copy.test(10));
    EXPECT_TRUE(copy.test(20));
}

TEST(BitmapTest, Copy_ChangingTheOriginal_LeavesTheCopyAlone) {
    Bitmap original(200);
    original.set(10);

    Bitmap copy = original;
    original.clear(10);

    EXPECT_TRUE(copy.test(10));
}

TEST(BitmapTest, Move_NewOwner_KeepsTheBits) {
    Bitmap original(200);
    original.set(10);
    original.set(150);

    Bitmap moved = std::move(original);

    EXPECT_EQ(walkUp(moved), (Positions {10, 150}));
}
