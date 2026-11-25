#include <gtest/gtest.h>
#include <cstddef>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

class DepthTest : public SidedBookTest {
protected:
    // Levels as far apart as the band allows
    void restFarLevels() {
        rest(side_, 1, 5);
        rest(side_, 50'000, 7);
        rest(side_, 100'000, 20);
        rest(side_, 50'000, 5);
    }

    Levels farLevels() const {
        return at(Levels {{100'000, 20, 1}, {50'000, 12, 2}, {1, 5, 1}});
    }
};

INSTANTIATE_FOR_BOTH_SIDES(DepthTest);

// --------------------------
// How many levels
// --------------------------
TEST_P(DepthTest, EmptySide_NoLevelsForAnyCount) {
    rest(other_, 100, 50);

    for (std::size_t levels : {std::size_t {0}, std::size_t {1}, std::size_t {100}, ALL_LEVELS})
        EXPECT_TRUE(book_.getDepth(side_, levels).empty()) << levels << " levels";
}

TEST_P(DepthTest, FewerThanThereAre_OnlyTheBestOnes) {
    restFarLevels();

    Levels expected = farLevels();
    for (std::size_t levels {0}; levels < 3; ++levels) {
        Levels best(expected.begin(), expected.begin() + static_cast<std::ptrdiff_t>(levels));
        EXPECT_EQ(levelsOf(book_, side_, levels), best) << levels << " levels";
    }
}

TEST_P(DepthTest, AsManyAsThereAreOrMore_AllOfThem) {
    restFarLevels();

    for (std::size_t levels : {std::size_t {3}, std::size_t {4}, std::size_t {1'000'000}, ALL_LEVELS})
        EXPECT_EQ(levelsOf(book_, side_, levels), farLevels()) << levels << " levels";
}

// getDepth reserves room for a few levels only
TEST_P(DepthTest, HundredLevels_AllOfThemBestFirst) {
    Levels expected;
    for (Price price {100}; price >= 1; --price) {
        rest(side_, price, static_cast<Quantity>(price));
        expected.push_back({price, static_cast<Quantity>(price), 1});
    }

    EXPECT_EQ(levelsOf(book_, side_), at(expected));
}

// --------------------------
// What each level holds
// --------------------------
TEST_P(DepthTest, AddedInMixedOrder_BestFirst) {
    for (Price price : {98, 100, 97, 99})
        rest(side_, price, 10);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 10, 1}, {99, 10, 1}, {98, 10, 1}, {97, 10, 1}}));
}

TEST_P(DepthTest, SeveralOrdersAtOnePrice_OneLevelWithTheTotals) {
    for (Quantity quantity : {50, 30, 20})
        rest(side_, 100, quantity);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 100, 3}}));
}

TEST_P(DepthTest, BothSidesPopulated_EachShowsItsOwnLevels) {
    rest(side_, 99, 50);
    rest(side_, 98, 40);
    rest(other_, 101, 30);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{99, 50, 1}, {98, 40, 1}}));
    EXPECT_EQ(levelsOf(book_, other_), at(Levels {{101, 30, 1}}));
}

TEST_P(DepthTest, PopulatedBook_AgreesWithTheOtherQueries) {
    restFarLevels();

    auto depth = book_.getDepth(side_, ALL_LEVELS);

    ASSERT_EQ(depth.size(), 3u);
    EXPECT_EQ(depth[0].price, best(side_)->getPrice());
    for (const LevelView& level : depth) {
        EXPECT_EQ(level.quantity, book_.getLevelQuantity(side_, level.price));
        EXPECT_EQ(level.count, book_.getLevelCount(side_, level.price));
    }
}

// --------------------------
// After a change
// --------------------------
TEST_P(DepthTest, RestingOrderPartiallyFilled_QuantityDropsAndCountStays) {
    rest(side_, 100, 50);
    rest(side_, 100, 30);

    add(OrderType::GTC, other_, 100, 20);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 60, 2}}));
    EXPECT_TRUE(levelsOf(book_, other_).empty());
}

TEST_P(DepthTest, BestLevelFilled_NextOneIsFirst) {
    rest(side_, 100, 50);
    rest(side_, 99, 30);

    add(OrderType::GTC, other_, 100, 50);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{99, 30, 1}}));
}

TEST_P(DepthTest, OneOrderOfALevelCancelled_QuantityAndCountDrop) {
    rest(side_, 100, 50);
    OrderID id = rest(side_, 100, 30);
    rest(side_, 100, 20);

    book_.cancelOrder(id);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 70, 2}}));
}

TEST_P(DepthTest, MiddleLevelCancelled_ItDisappears) {
    rest(side_, 100'000, 9);
    OrderID id = rest(side_, 50'000, 7);
    rest(side_, 1, 5);

    book_.cancelOrder(id);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100'000, 9, 1}, {1, 5, 1}}));
}

TEST_P(DepthTest, QuantityReducedInPlace_QuantityDropsAndCountStays) {
    OrderID id = rest(side_, 100, 50);
    rest(side_, 100, 30);

    book_.modifyOrder(id, at(100), 10);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 40, 2}}));
}

TEST_P(DepthTest, OrderMovedToANewPrice_QuantityMovesWithIt) {
    OrderID id = rest(side_, 100, 50);
    rest(side_, 100, 30);

    book_.modifyOrder(id, at(98), 50);

    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{100, 30, 1}, {98, 50, 1}}));
}

// The result is a copy, so later changes do not show up in it
TEST_P(DepthTest, BookChangedAfterwards_EarlierResultUnchanged) {
    OrderID id = rest(side_, 100, 50);
    auto before = book_.getDepth(side_, 5);

    book_.cancelOrder(id);
    rest(side_, 99, 30);

    ASSERT_EQ(before.size(), 1u);
    EXPECT_EQ(before[0].price, at(100));
    EXPECT_EQ(before[0].quantity, 50u);
    EXPECT_EQ(before[0].count, 1u);
    EXPECT_EQ(levelsOf(book_, side_), at(Levels {{99, 30, 1}}));
}
