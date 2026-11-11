#pragma once
#include "types.hpp"

#include <vector>

struct Trade {
    OrderID bidID;
    OrderID askID;
    Price price;
    Quantity quantity;
    OwnerID bidOwner;
    OwnerID askOwner;
};

using Trades = std::vector<Trade>;

struct OrderResult {
    OrderID orderID;
    Trades  trades;
};

void printTrade(const Trade& trade);
void const printTrades(const Trades& trades);

