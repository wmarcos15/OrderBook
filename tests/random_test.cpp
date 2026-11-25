#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include "orderbook.hpp"
#include "reference_book.hpp"
#include "test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::test;

// What the run reached, to check that it hit the interesting cases
struct RunCounts {
    std::size_t trades {0};
    std::size_t emptyBooks {0};
    std::size_t killedOrders {0};
    std::size_t movedOrders {0};
    std::size_t cancelledNewest {0};
    std::size_t cancelledOldest {0};
    std::size_t rejected {0};
};

// Owners 1 to OWNERS, so that some trades would be with themselves
constexpr OwnerID OWNERS = 8;

// Prices are in a cluster mid-band, far away on their own side, or anywhere
struct RunConfig {
    BookConfig book;
    Price clusterTicks;
    Price gapTicks;
    OwnerID owners;
    unsigned seed;
};

class RandomRun {
private:
    RunConfig config_;
    OrderBook book_;
    ReferenceBook reference_;
    std::mt19937 generator_;
    OwnerID nextOwner_ {1};
    RunCounts counts_;

    template<typename Number>
    Number pick(Number from, Number to) {
        return std::uniform_int_distribution<Number>(from, to)(generator_);
    }

    bool chance(int percent) {
        return pick(0, 99) < percent;
    }

    Side pickSide() {
        return chance(50) ? Side::buy : Side::sell;
    }

    // With no owners set, every order has its own and nothing self trades
    OwnerID pickOwner() {
        return (config_.owners == 0) ? nextOwner_++ : pick<OwnerID>(1, config_.owners);
    }

    Price pickPrice(Side side, bool anywhere) {
        Price tick = config_.book.tickSize;
        Price low = config_.book.minPrice / tick;
        Price high = config_.book.maxPrice / tick;
        Price middle = (low + high) / 2;

        int roll = anywhere ? 99 : pick(0, 99);
        if (roll < 70) {
            low = middle - config_.clusterTicks;
            high = middle + config_.clusterTicks;
        } else if (roll < 95) {
            if (side == Side::buy) high = middle - config_.gapTicks;
            else low = middle + config_.gapTicks;
        }
        return pick(low, high) * tick;
    }

    OrderID pickID() {
        const auto& orders = reference_.orders();
        if (!orders.empty() && chance(85))
            return orders[pick<std::size_t>(0, orders.size() - 1)].id;
        return pick<OrderID>(0, reference_.nextID() + 5);
    }

    void compare(const OrderResult& actual, const OrderResult& expected) {
        ASSERT_EQ(actual.orderID, expected.orderID);
        ASSERT_EQ(actual.status, expected.status);
        ASSERT_EQ(actual.cancelledOrders, expected.cancelledOrders);
        ASSERT_EQ(actual.trades.size(), expected.trades.size());
        for (std::size_t i {0}; i < expected.trades.size(); ++i) {
            const Trade& a = actual.trades[i];
            const Trade& e = expected.trades[i];
            ASSERT_EQ(a.bidID, e.bidID);
            ASSERT_EQ(a.askID, e.askID);
            ASSERT_EQ(a.price, e.price);
            ASSERT_EQ(a.quantity, e.quantity);
            ASSERT_EQ(a.bidOwner, e.bidOwner);
            ASSERT_EQ(a.askOwner, e.askOwner);
        }

        counts_.trades += expected.trades.size();
        if (expected.status == OrderStatus::selfTradeCancelledNewest) ++counts_.cancelledNewest;
        if (expected.status == OrderStatus::selfTradeCancelledOldest) ++counts_.cancelledOldest;
    }

    void addOrder() {
        int roll = pick(0, 99);
        OrderType type = (roll < 64) ? OrderType::GTC : (roll < 85) ? OrderType::IOC : OrderType::FOK;
        Side side = pickSide();
        OwnerID owner = pickOwner();
        Price price = pickPrice(side, false);
        Quantity quantity = pick<Quantity>(1, 30);

        OrderResult expected = reference_.add(owner, type, side, price, quantity);
        compare(book_.addOrder(owner, type, side, price, quantity), expected);
        if (type == OrderType::FOK && expected.trades.empty()) ++counts_.killedOrders;
    }

    void addMarketOrder() {
        Side side = pickSide();
        OwnerID owner = pickOwner();
        Quantity quantity = pick<Quantity>(1, 60);

        OrderResult expected = reference_.addMarket(owner, side, quantity);
        compare(book_.addMarketOrder(owner, side, quantity), expected);
    }

    void cancelOrder() {
        OrderID id = pickID();

        reference_.cancel(id);
        book_.cancelOrder(id);
        ASSERT_EQ(book_.getOrder(id), nullptr);
    }

    void modifyOrder() {
        OrderID id = pickID();
        const Order* order = book_.getOrder(id);
        Price price = (order != nullptr && chance(30)) ? order->getPrice() : pickPrice(pickSide(), true);
        Quantity quantity = pick<Quantity>(1, 30);

        std::optional<OrderResult> expected = reference_.modify(id, price, quantity);
        std::optional<OrderResult> actual = book_.modifyOrder(id, price, quantity);
        ASSERT_EQ(actual.has_value(), expected.has_value());
        if (!expected) return;

        compare(*actual, *expected);
        if (expected->orderID != id) ++counts_.movedOrders;
    }

    // The checks of the state after it see that nothing changed
    void sendSomethingIllegal() {
        Price price = chance(50) ? config_.book.maxPrice + config_.book.tickSize : 0;
        ASSERT_THROW(book_.addOrder(1, OrderType::GTC, pickSide(), price, 1), std::invalid_argument);
        ASSERT_THROW(book_.addOrder(1, OrderType::GTC, pickSide(), config_.book.minPrice, 0), std::invalid_argument);
        ASSERT_THROW(book_.addMarketOrder(1, pickSide(), 0), std::invalid_argument);

        if (!reference_.orders().empty()) {
            OrderID id = reference_.orders().front().id;
            ASSERT_THROW(book_.modifyOrder(id, price, 1), std::invalid_argument);
            ASSERT_THROW(book_.modifyOrder(id, config_.book.minPrice, 0), std::invalid_argument);
        }
        ++counts_.rejected;
    }

    void changePolicy() {
        OwnerID owner = pickOwner();
        SelfTradePolicy policy = chance(50) ? SelfTradePolicy::cancelOldest : SelfTradePolicy::cancelNewest;

        reference_.setPolicy(owner, policy);
        book_.setSelfTradePolicy(owner, policy);
    }

    void moveBook() {
        OrderBook moved(std::move(book_));
        book_ = OrderBook(config_.book);
        book_ = std::move(moved);
    }

    void step() {
        int roll = pick(0, 999);
        if (roll < 550) addOrder();
        else if (roll < 650) addMarketOrder();
        else if (roll < 820) cancelOrder();
        else if (roll < 980) modifyOrder();
        else if (roll < 988) sendSomethingIllegal();
        else if (roll < 998) changePolicy();
        else moveBook();
    }

    void checkState() {
        ASSERT_EQ(book_.size(), reference_.orders().size());
        ASSERT_EQ(book_.empty(), reference_.orders().empty());

        for (Side side : {Side::buy, Side::sell}) {
            const Order* actual = bestOf(book_, side);
            const ReferenceOrder* expected = reference_.best(side);
            ASSERT_EQ(actual == nullptr, expected == nullptr);
            if (expected != nullptr) {
                ASSERT_EQ(actual->getID(), expected->id);
                ASSERT_EQ(actual->getOwnerID(), expected->owner);
                ASSERT_EQ(actual->getRemainingQuantity(), expected->quantity);
            }

            Levels levels = reference_.levels(side);
            ASSERT_EQ(levelsOf(book_, side), levels);

            Levels few(levels.begin(), levels.begin() + std::min<std::ptrdiff_t>(3, std::ssize(levels)));
            ASSERT_EQ(levelsOf(book_, side, 3), few);

            for (const auto& [price, quantity, count] : few) {
                ASSERT_EQ(book_.getLevelQuantity(side, price), quantity);
                ASSERT_EQ(book_.getLevelCount(side, price), count);
                ASSERT_EQ(book_.getLevelCount(otherSide(side), price), 0u);
            }
        }

        if (book_.empty()) ++counts_.emptyBooks;
    }

    void checkEveryOrder() {
        for (const ReferenceOrder& expected : reference_.orders()) {
            const Order* actual = book_.getOrder(expected.id);
            ASSERT_NE(actual, nullptr);
            ASSERT_EQ(actual->getOwnerID(), expected.owner);
            ASSERT_EQ(actual->getSide(), expected.side);
            ASSERT_EQ(actual->getPrice(), expected.price);
            ASSERT_EQ(actual->getRemainingQuantity(), expected.quantity);
        }
    }

public:
    explicit RandomRun(const RunConfig& config)
        : config_ {config}, book_(config.book), generator_(config.seed)
    {}

    void run() {
        constexpr std::size_t STEPS = 20'000;

        for (std::size_t i {0}; i < STEPS; ++i) {
            SCOPED_TRACE(i);
            step();
            checkState();
            if (i % 100 == 0) checkEveryOrder();
            if (::testing::Test::HasFatalFailure()) return;
        }
    }

    const RunCounts& counts() const {
        return counts_;
    }
};

TEST(RandomTest, DefaultBandWithAClusterAndFarOutliers_MatchesTheReference) {
    RandomRun run({BookConfig {}, 15, 400, 0, 12345});

    run.run();

    EXPECT_GT(run.counts().trades, 1'000u);
    EXPECT_GT(run.counts().killedOrders, 50u);
    EXPECT_GT(run.counts().movedOrders, 100u);
    EXPECT_GT(run.counts().rejected, 100u);
    EXPECT_EQ(run.counts().cancelledNewest + run.counts().cancelledOldest, 0u);
}

TEST(RandomTest, SmallBandWithATickOfFive_MatchesTheReference) {
    RandomRun run({BookConfig {5, 5, 500}, 4, 10, 0, 777});

    run.run();

    EXPECT_GT(run.counts().trades, 1'000u);
    EXPECT_GT(run.counts().emptyBooks, 10u);
    EXPECT_GT(run.counts().movedOrders, 100u);
}

TEST(RandomTest, BandOfAMillionPrices_MatchesTheReference) {
    RandomRun run({BookConfig {1, 1, 1'000'000}, 10, 100'000, 0, 2024});

    run.run();

    EXPECT_GT(run.counts().trades, 500u);
    EXPECT_GT(run.counts().killedOrders, 20u);
}

TEST(RandomTest, FewOwnersThatSelfTrade_MatchesTheReference) {
    RandomRun run({BookConfig {}, 15, 400, OWNERS, 99});

    run.run();

    EXPECT_GT(run.counts().trades, 1'000u);
    EXPECT_GT(run.counts().killedOrders, 50u);
    EXPECT_GT(run.counts().cancelledNewest, 200u);
    EXPECT_GT(run.counts().cancelledOldest, 200u);
}
