#include "orderbook.hpp"

#include <iostream>

int main() {
    orderbook::OrderBook book;

    book.addOrder(1, orderbook::OrderType::GTC, orderbook::Side::sell, 100, 10);
    orderbook::OrderResult result = book.addOrder(2, orderbook::OrderType::GTC, orderbook::Side::buy, 100, 10);

    if (result.trades.size() != 1) {
        std::cerr << "ERROR: expected one trade, got " << result.trades.size() << "\n";
        return 1;
    }

    return 0;
}
