#pragma once
#include "trade.hpp"
#include <stdlib.h>
#include <iostream>

void printTrade(const Trade& trade) {
    std::cout << "[✓] MATCH | BUY #" << trade.bidID 
        << " vs SELL #" << trade.askID 
        << " @ €" << trade.price 
        << " | " << trade.quantity
        << " units traded";
}

void const printTrades(const Trades& trades) {
    for (const auto& t : trades) {
        printTrade(t);
        std::cout << '\n';
    }
}

