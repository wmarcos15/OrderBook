#include "orderbook.hpp"

#include <iostream>

using namespace orderbook;

int main() {
    constexpr OwnerID ALICE = 1;
    constexpr OwnerID BOB   = 2;

    OrderBook book;

    OrderResult ask = book.addOrder(ALICE, OrderType::GTC, Side::sell, 10'001, 50);
    std::cout << "Rested SELL #" << ask.orderID << " | 50 @ €10001\n\n";

    OrderResult bid = book.addOrder(BOB, OrderType::GTC, Side::buy, 10'002, 80);
    std::cout << "Incoming BUY #" << bid.orderID << " | 80 @ €10002\n";
    printTrades(bid.trades);

    std::cout << "\nResting at €10002: "
        << book.getLevelQuantity(Side::buy, 10'002) << " units across "
        << book.getLevelCount(Side::buy, 10'002) << " order(s)\n"
        << "Best bid: €" << book.getBestBid()->getPrice()
        << " for " << book.getBestBid()->getRemainingQuantity() << " units\n";

    // Top levels of the bid side, best first
    for (const LevelView& level : book.getDepth(Side::buy, 5)) {
        std::cout << "Bid depth: €" << level.price << " | "
            << level.quantity << " units across "
            << level.count << " order(s)\n";
    }
    std::cout << '\n';

    OrderResult trimmed = book.modifyOrder(bid.orderID, 10'002, 10).value();
    std::cout << "Trimmed #" << bid.orderID << " to 10 units | ID after modify: #"
        << trimmed.orderID << " (unchanged, queue position kept)\n";

    OrderResult moved = book.modifyOrder(trimmed.orderID, 10'003, 10).value();
    std::cout << "Repriced to €10003 | ID after modify: #"
        << moved.orderID << " (new order, time priority lost)\n\n";

    book.cancelOrder(moved.orderID);
    std::cout << "Cancelled #" << moved.orderID
        << " | book empty: " << std::boolalpha << book.empty() << "\n\n";

    // Self-trade prevention
    OrderResult ownAsk = book.addOrder(ALICE, OrderType::GTC, Side::sell, 10'001, 50);
    OrderResult ownBid = book.addOrder(ALICE, OrderType::GTC, Side::buy, 10'001, 75);
    std::cout << "Alice rests SELL #" << ownAsk.orderID << " and sends BUY #"
        << ownBid.orderID << " across it (cancelNewest)\n"
        << "Trades: " << ownBid.trades.size()
        << " | BUY dropped: " << (book.getOrder(ownBid.orderID) == nullptr)
        << " | SELL still resting: " << (book.getOrder(ownAsk.orderID) != nullptr) << "\n\n";

    book.setSelfTradePolicy(ALICE, SelfTradePolicy::cancelOldest);
    ownBid = book.addOrder(ALICE, OrderType::GTC, Side::buy, 10'001, 75);
    std::cout << "Alice sends BUY #" << ownBid.orderID << " again (cancelOldest)\n"
        << "Trades: " << ownBid.trades.size()
        << " | cancelled SELL #" << ownBid.cancelledOrders.front()
        << " | BUY rests with " << book.getBestBid()->getRemainingQuantity() << " units\n";
}
