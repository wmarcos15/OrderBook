#include "orderbook.hpp"

#include <iostream>

int main() {
    OrderBook book;

    OrderResult ask = book.addOrder(OrderType::GTC, Side::sell, 10'001, 50);
    std::cout << "Rested SELL #" << ask.orderID << " | 50 @ €10001\n\n";

    OrderResult bid = book.addOrder(OrderType::GTC, Side::buy, 10'002, 80);
    std::cout << "Incoming BUY #" << bid.orderID << " | 80 @ €10002\n";
    printTrades(bid.trades);

    std::cout << "\nResting at €10002: "
        << book.getLevelQuantity(Side::buy, 10'002) << " units across "
        << book.getLevelCount(Side::buy, 10'002) << " order(s)\n"
        << "Best bid: €" << book.getBestBid()->getPrice()
        << " for " << book.getBestBid()->getRemainingQuantity() << " units\n\n";

    OrderResult trimmed = book.modifyOrder(bid.orderID, 10'002, 10).value();
    std::cout << "Trimmed #" << bid.orderID << " to 10 units | ID after modify: #"
        << trimmed.orderID << " (unchanged, queue position kept)\n";

    OrderResult moved = book.modifyOrder(trimmed.orderID, 10'003, 10).value();
    std::cout << "Repriced to €10003 | ID after modify: #"
        << moved.orderID << " (new order, time priority lost)\n\n";

    book.cancelOrder(moved.orderID);
    std::cout << "Cancelled #" << moved.orderID
        << " | book empty: " << std::boolalpha << book.empty() << '\n';
}
