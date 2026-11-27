#pragma once
#include "orderbook.hpp"
#include "types.hpp"

#include <gtest/gtest.h>
#include <cstddef>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace orderbook::test {

// Buys belong to BUYER and sells to SELLER, so that nothing trades with itself
constexpr OwnerID BUYER  = 1;
constexpr OwnerID SELLER = 2;

constexpr std::size_t ALL_LEVELS = std::numeric_limits<std::size_t>::max();

using IDs = std::vector<OrderID>;
using Prices = std::vector<Price>;
using Quantities = std::vector<Quantity>;
using Levels = std::vector<std::tuple<Price, Quantity, std::size_t>>;

constexpr Side otherSide(Side side) {
    return (side == Side::buy) ? Side::sell : Side::buy;
}

constexpr OwnerID ownerOf(Side side) {
    return (side == Side::buy) ? BUYER : SELLER;
}

inline std::string sideName(const ::testing::TestParamInfo<Side>& info) {
    return (info.param == Side::buy) ? "buy" : "sell";
}

inline const Order* bestOf(const OrderBook& book, Side side) {
    return (side == Side::buy) ? book.getBestBid() : book.getBestAsk();
}

inline Levels levelsOf(const OrderBook& book, Side side, std::size_t levels = ALL_LEVELS) {
    Levels found;
    for (const LevelView& level : book.getDepth(side, levels))
        found.push_back({level.price, level.quantity, level.count});
    return found;
}

inline Prices tradePrices(const Trades& trades) {
    Prices prices;
    for (const Trade& trade : trades)
        prices.push_back(trade.price);
    return prices;
}

inline Quantities tradeQuantities(const Trades& trades) {
    Quantities quantities;
    for (const Trade& trade : trades)
        quantities.push_back(trade.quantity);
    return quantities;
}

inline void expectEmptyBook(const OrderBook& book) {
    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.size(), 0u);
    for (Side side : {Side::buy, Side::sell}) {
        EXPECT_EQ(bestOf(book, side), nullptr);
        EXPECT_TRUE(levelsOf(book, side).empty());
    }
}

// Everything the book shows, to check that a call changed nothing
struct Snapshot {
    Levels bids;
    Levels asks;
    std::size_t size;

    bool operator==(const Snapshot&) const = default;
};

inline Snapshot snapshotOf(const OrderBook& book) {
    return {levelsOf(book, Side::buy), levelsOf(book, Side::sell), book.size()};
}

// Every test runs once per side. The prices are written as a buy sees them, and
// a sell gets them mirrored in the band: the higher the better for both.
class SidedBookTest : public ::testing::TestWithParam<Side> {
protected:
    Side side_ {GetParam()};
    Side other_ {otherSide(side_)};
    OrderBook book_;

    SidedBookTest() : book_(BookConfig {1, 1, 100'000}) {}
    explicit SidedBookTest(const BookConfig& config) : book_(config) {}

    Price at(Price price) const {
        if (side_ == Side::buy) return price;
        const BookConfig& config = book_.getBookConfig();
        return config.minPrice + config.maxPrice - price;
    }

    Prices at(Prices prices) const {
        for (Price& price : prices)
            price = at(price);
        return prices;
    }

    Levels at(Levels levels) const {
        for (auto& [price, quantity, count] : levels)
            price = at(price);
        return levels;
    }

    OrderResult add(OrderType type, Side side, Price price, Quantity quantity) {
        return book_.addOrder(ownerOf(side), type, side, at(price), quantity);
    }

    OrderID rest(Side side, Price price, Quantity quantity) {
        return add(OrderType::GTC, side, price, quantity).orderID;
    }

    const Order* best(Side side) const {
        return bestOf(book_, side);
    }

    void expectBest(Side side, OrderID id) const {
        ASSERT_NE(best(side), nullptr);
        EXPECT_EQ(best(side)->getID(), id);
    }

    void expectLevel(Side side, Price price, Quantity quantity, std::size_t count) const {
        EXPECT_EQ(book_.getLevelQuantity(side, at(price)), quantity);
        EXPECT_EQ(book_.getLevelCount(side, at(price)), count);
    }

    // The incoming order is on side_ and the resting one on the other side
    void expectTrade(const Trade& trade, OrderID incoming, OrderID resting, Price price, Quantity quantity) const {
        bool isBuy = side_ == Side::buy;
        EXPECT_EQ(trade.bidID, isBuy ? incoming : resting);
        EXPECT_EQ(trade.askID, isBuy ? resting : incoming);
        EXPECT_EQ(trade.price, at(price));
        EXPECT_EQ(trade.quantity, quantity);
        EXPECT_EQ(trade.bidOwner, BUYER);
        EXPECT_EQ(trade.askOwner, SELLER);
    }
};

} // namespace orderbook::test

// For the fixtures whose tests run once per side
#define INSTANTIATE_FOR_BOTH_SIDES(Fixture) \
    INSTANTIATE_TEST_SUITE_P(BothSides, Fixture, \
        ::testing::Values(orderbook::Side::buy, orderbook::Side::sell), orderbook::test::sideName)
