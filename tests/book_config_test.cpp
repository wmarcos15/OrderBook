#include <gtest/gtest.h>
#include <optional>
#include <stdexcept>
#include <string>
#include "orderbook.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// Tick 5, band [100, 1'000]
static BookConfig tick5Config() {
    return BookConfig {.tickSize = 5, .minPrice = 100, .maxPrice = 1'000};
}

static void expectRejected(const BookConfig& config) {
    EXPECT_THROW(OrderBook {config}, std::invalid_argument)
        << "tick " << config.tickSize << ", band " << config.minPrice << " to " << config.maxPrice;
}

static void expectRejectedWith(const BookConfig& config, const std::string& message) {
    try {
        OrderBook book(config);
        ADD_FAILURE() << "accepted, expected: " << message;
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(error.what(), message);
    }
}

// --------------------------
// Constructor
// --------------------------
TEST(BookConfigTest, Constructor_Default_UsesTheDefaultValues) {
    OrderBook book;

    EXPECT_EQ(book.getBookConfig().tickSize, 1);
    EXPECT_EQ(book.getBookConfig().minPrice, 1);
    EXPECT_EQ(book.getBookConfig().maxPrice, 100'000);
    expectEmptyBook(book);
}

TEST(BookConfigTest, Constructor_LegalConfig_StoresTheExactValues) {
    OrderBook book(tick5Config());

    EXPECT_EQ(book.getBookConfig().tickSize, 5);
    EXPECT_EQ(book.getBookConfig().minPrice, 100);
    EXPECT_EQ(book.getBookConfig().maxPrice, 1'000);
    expectEmptyBook(book);
}

TEST(BookConfigTest, Constructor_OnePriceBand_Accepted) {
    OrderBook book(BookConfig {.tickSize = 5, .minPrice = 100, .maxPrice = 100});

    OrderID id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 100, 10).orderID;

    EXPECT_NE(book.getOrder(id), nullptr);
}

// With the band on the tick, so that only the check of the tick can reject it
TEST(BookConfigTest, Constructor_TickNotPositive_ThrowsBecauseOfTheTick) {
    for (Price tick : {0, -1, -5})
        expectRejectedWith({.tickSize = tick, .minPrice = 5, .maxPrice = 1'000}, "tickSize must be greater than 0");
}

TEST(BookConfigTest, Constructor_MinPriceNotPositive_Throws) {
    expectRejected({.minPrice = 0});
    expectRejected({.minPrice = -10});
}

TEST(BookConfigTest, Constructor_MinPriceAboveMaxPrice_Throws) {
    expectRejected({.minPrice = 200, .maxPrice = 100});
    expectRejected({.minPrice = 101, .maxPrice = 100});
}

TEST(BookConfigTest, Constructor_BandNotOnTick_ThrowsAndNamesTheEdge) {
    expectRejectedWith({.tickSize = 5, .minPrice = 7, .maxPrice = 1'000},
        "minPrice (7) must be a multiple of tickSize (5); set minPrice and maxPrice to multiples of the tick");
    expectRejectedWith({.tickSize = 5, .minPrice = 5, .maxPrice = 1'003},
        "maxPrice (1003) must be a multiple of tickSize (5); set minPrice and maxPrice to multiples of the tick");
}

// The default band starts at 1, so a book with another tick has to set the band too
TEST(BookConfigTest, Constructor_OnlyTheTickChanged_Throws) {
    expectRejected({.tickSize = 5});
}

// --------------------------
// Prices of the orders
// --------------------------
TEST(BookConfigTest, AddOrder_PriceOnTick_Rests) {
    OrderBook book(tick5Config());

    for (Price price : {100, 105, 500, 995, 1'000}) {
        OrderID id = book.addOrder(BUYER, OrderType::GTC, Side::buy, price, 10).orderID;

        ASSERT_NE(book.getBestBid(), nullptr);
        EXPECT_EQ(book.getBestBid()->getID(), id);
        EXPECT_EQ(book.getLevelQuantity(Side::buy, price), 10u);
    }
}

TEST(BookConfigTest, AddOrder_IllegalPrice_ThrowsAndLeavesNoTrace) {
    OrderBook book(tick5Config());
    OrderID id = book.addOrder(BUYER, OrderType::GTC, Side::buy, 500, 10).orderID;
    Snapshot before = snapshotOf(book);

    for (Price price : {503, 99, 95, 5, 0, -5, 1'001, 1'005, 2'000}) // off tick, below, above
        for (Side side : {Side::buy, Side::sell})
            for (OrderType type : {OrderType::GTC, OrderType::IOC, OrderType::FOK})
                EXPECT_THROW(book.addOrder(ownerOf(side), type, side, price, 10), std::invalid_argument)
                    << "price " << price;

    EXPECT_EQ(snapshotOf(book), before);
    EXPECT_EQ(book.addOrder(BUYER, OrderType::GTC, Side::buy, 505, 10).orderID, id + 1);
}

TEST(BookConfigTest, ModifyOrder_IllegalPrice_ThrowsAndLeavesTheOrderAlone) {
    OrderBook book(tick5Config());
    OrderID id = book.addOrder(SELLER, OrderType::GTC, Side::sell, 500, 100).orderID;
    Snapshot before = snapshotOf(book);

    for (Price price : {503, 95, 0, 1'005})
        EXPECT_THROW(book.modifyOrder(id, price, 100), std::invalid_argument) << "price " << price;

    ASSERT_NE(book.getOrder(id), nullptr);
    EXPECT_EQ(book.getOrder(id)->getPrice(), 500);
    EXPECT_EQ(book.getOrder(id)->getRemainingQuantity(), 100u);
    EXPECT_EQ(snapshotOf(book), before);
}

TEST(BookConfigTest, ModifyOrder_UnknownIDWithIllegalPrice_ReturnsNullopt) {
    OrderBook book(tick5Config());

    EXPECT_EQ(book.modifyOrder(42, 503, 100), std::nullopt);
}
