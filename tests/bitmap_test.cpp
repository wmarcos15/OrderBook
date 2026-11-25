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

// Where the words of the bitmap and of its summary begin and end: a word is 64
// positions and a word of the summary is 4096
static const Positions EDGES {0, 1, 31, 32, 33, 62, 63, 64, 65, 127, 128, 4'094, 4'095, 4'096, 4'097, 8'191, 8'192, 12'287};

constexpr std::size_t LARGE = 3 * 4'096;

// Stops a walk that never ends, which a broken search would give
constexpr std::size_t WALK_LIMIT = 1'000;

static Positions walkUp(const Bitmap& bitmap) {
    Positions found;
    for (std::size_t position = bitmap.next(0); position != NONE && found.size() < WALK_LIMIT; position = bitmap.next(position + 1))
        found.push_back(position);
    return found;
}

static Positions walkDown(const Bitmap& bitmap, std::size_t last) {
    Positions found;
    std::size_t position = bitmap.previous(last);
    while (position != NONE && found.size() < WALK_LIMIT) {
        found.push_back(position);
        if (position == 0) break;
        position = bitmap.previous(position - 1);
    }
    return found;
}

// Asks every position for its bit, the next one and the previous one
static void expectOnlyThese(const Bitmap& bitmap, std::size_t positions, const std::set<std::size_t>& on) {
    for (std::size_t position {0}; position < positions; ++position) {
        auto next = on.lower_bound(position);
        auto above = on.upper_bound(position);
        ASSERT_EQ(bitmap.test(position), on.contains(position)) << "position " << position;
        ASSERT_EQ(bitmap.next(position), (next == on.end()) ? NONE : *next) << "position " << position;
        ASSERT_EQ(bitmap.previous(position), (above == on.begin()) ? NONE : *std::prev(above)) << "position " << position;
    }
}

// --------------------------
// Sizes
// --------------------------
TEST(BitmapTest, NewBitmap_EveryBitIsOff) {
    for (std::size_t positions : {1u, 63u, 64u, 65u, 130u, 4'095u, 4'096u, 4'097u})
        expectOnlyThese(Bitmap(positions), positions, {});
}

TEST(BitmapTest, ZeroPositions_NextIsNone) {
    Bitmap bitmap(0);

    EXPECT_EQ(bitmap.next(0), NONE);
}

TEST(BitmapTest, AnySize_FirstAndLastPositionCanBeSetAndFound) {
    for (std::size_t positions : {1u, 63u, 64u, 65u, 130u, 4'095u, 4'096u, 4'097u, 8'192u}) {
        SCOPED_TRACE(positions);
        Bitmap bitmap(positions);

        bitmap.set(positions - 1);
        expectOnlyThese(bitmap, positions, {positions - 1});

        bitmap.set(0);
        expectOnlyThese(bitmap, positions, {0, positions - 1});
    }
}

TEST(BitmapTest, Next_PositionPastTheLastWord_IsNone) {
    Bitmap bitmap(128);
    bitmap.set(127);

    EXPECT_EQ(bitmap.next(128), NONE);
    EXPECT_EQ(bitmap.next(std::numeric_limits<std::size_t>::max()), NONE);
}

// --------------------------
// Set and clear
// --------------------------
TEST(BitmapTest, Set_OneBit_OnlyItIsOn) {
    for (std::size_t position : EDGES) {
        SCOPED_TRACE(position);
        Bitmap bitmap(LARGE);

        bitmap.set(position);

        expectOnlyThese(bitmap, LARGE, {position});
    }
}

TEST(BitmapTest, SetAndClear_OneBit_BitmapIsEmptyAgain) {
    for (std::size_t position : EDGES) {
        SCOPED_TRACE(position);
        Bitmap bitmap(LARGE);
        bitmap.set(position);

        bitmap.clear(position);
        expectOnlyThese(bitmap, LARGE, {});

        bitmap.set(position);
        expectOnlyThese(bitmap, LARGE, {position});
    }
}

TEST(BitmapTest, Clear_OneBitOfAFullBitmap_OnlyItIsOff) {
    for (std::size_t position : EDGES) {
        SCOPED_TRACE(position);
        Bitmap bitmap(LARGE);
        std::set<std::size_t> on;
        for (std::size_t other {0}; other < LARGE; ++other) {
            bitmap.set(other);
            on.insert(on.end(), other);
        }

        bitmap.clear(position);
        on.erase(position);

        expectOnlyThese(bitmap, LARGE, on);
    }
}

TEST(BitmapTest, SetAndClear_Twice_SameAsOnce) {
    Bitmap bitmap(128);
    bitmap.set(69);

    bitmap.set(70);
    bitmap.set(70);
    expectOnlyThese(bitmap, 128, {69, 70});

    bitmap.clear(70);
    bitmap.clear(70);
    expectOnlyThese(bitmap, 128, {69});
}

// --------------------------
// Two bits
// --------------------------
TEST(BitmapTest, TwoBits_EveryPairOfEdges_FoundAndClearedOneByOne) {
    for (std::size_t low : EDGES) {
        for (std::size_t high : EDGES) {
            if (low >= high) continue;
            SCOPED_TRACE(::testing::Message() << low << " and " << high);
            Bitmap bitmap(LARGE);
            bitmap.set(low);
            bitmap.set(high);

            EXPECT_EQ(walkUp(bitmap), (Positions {low, high}));
            EXPECT_EQ(walkDown(bitmap, LARGE - 1), (Positions {high, low}));
            EXPECT_EQ(bitmap.next(low + 1), high);
            EXPECT_EQ(bitmap.previous(high - 1), low);

            // The other bit of the word, or of the word of the summary, must stay
            Bitmap copy = bitmap;
            copy.clear(low);
            EXPECT_EQ(walkUp(copy), (Positions {high}));
            EXPECT_EQ(walkDown(copy, LARGE - 1), (Positions {high}));

            bitmap.clear(high);
            EXPECT_EQ(walkUp(bitmap), (Positions {low}));
            EXPECT_EQ(walkDown(bitmap, LARGE - 1), (Positions {low}));
        }
    }
}

TEST(BitmapTest, AMillionPositions_FirstAndLast_JumpTheGap) {
    Bitmap bitmap(1'000'000);
    bitmap.set(0);
    bitmap.set(999'999);

    EXPECT_EQ(bitmap.next(1), 999'999u);
    EXPECT_EQ(bitmap.previous(999'998), 0u);

    bitmap.clear(999'999);

    EXPECT_EQ(bitmap.next(1), NONE);
    EXPECT_EQ(bitmap.previous(999'999), 0u);
}

TEST(BitmapTest, EveryEdge_WalkedInBothDirections) {
    Bitmap bitmap(LARGE);
    for (std::size_t position : EDGES)
        bitmap.set(position);

    EXPECT_EQ(walkUp(bitmap), EDGES);
    EXPECT_EQ(walkDown(bitmap, LARGE - 1), Positions(EDGES.rbegin(), EDGES.rend()));
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
