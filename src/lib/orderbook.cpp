#include "orderbook.hpp"
#include "order.hpp"
#include "types.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>

OrderBook::OrderIterator OrderBook::findOrder(OrderID id) {
    return orders_.find(id);
}

std::size_t OrderBook::size() const {
    return orders_.size();
}

const Order& OrderBook::getBestBid() const {
    if (bids_.empty()) throw std::runtime_error("empty bid side");
    return *bids_.begin()->second.front();
}

const Order& OrderBook::getBestAsk() const {
    if (asks_.empty()) throw std::runtime_error("empty ask side");
    return *asks_.begin()->second.front();
}

bool OrderBook::empty() const {
    return orders_.empty();
}

Quantity OrderBook::getLevelQuantity(Side side, Price price) const {
    const auto& data = (side == Side::buy) ? bidsData_ : asksData_;
    auto it = data.find(price);
    return it != data.end() ? it->second.qty_ : 0;
}
std::size_t OrderBook::getLevelCount(Side side, Price price) const {
    const auto& data = (side == Side::buy) ? bidsData_ : asksData_;
    auto it = data.find(price);
    return it != data.end() ? it->second.count_ : 0;
}

const Order* OrderBook::getOrder(OrderID id) const {
    auto it = orders_.find(id);
    if (it == orders_.end()) return nullptr;
    return it->second.order_.get();
}

Trades OrderBook::match(OrderPointer incoming) {
    Trades trades;

    while (incoming->getRemainingQuantity() > 0) {
        if (incoming->getSide() == Side::buy) {
            if (asks_.empty() || incoming->getPrice() < asks_.begin()->first) break;
            matchTop(incoming, asks_, asksData_, trades);
        } else {
            if (bids_.empty() || incoming->getPrice() > bids_.begin()->first) break;
            matchTop(incoming, bids_, bidsData_, trades);
        }
    }

    return trades;
}

// O(n) so that updates can be O(1)
bool OrderBook::canFullyFill(OrderPointer order) const {
    Quantity qty = order->getRemainingQuantity();

    if (order->getSide() == Side::buy) {
        if (asks_.empty()) return false;
        for (const auto& [price, _] : asks_) {
            if (price > order->getPrice()) break;
            if (asksData_.at(price).qty_ >= qty) return true;
            qty -= asksData_.at(price).qty_;
        }
    } else {
        if (bids_.empty()) return false;
        for (const auto& [price, _] : bids_) {
            if (price < order->getPrice()) break;
            if (bidsData_.at(price).qty_ >= qty) return true;
            qty -= bidsData_.at(price).qty_;
        }
    }
    return false;
}

OrderResult OrderBook::addOrder(OrderType type, Side side, Price price, Quantity qty) {
    OrderID id = nextID_++;
    OrderPointer incoming = std::make_shared<Order>(id, type, side, price, qty);

    if (type == OrderType::FOK && !canFullyFill(incoming))
        return {id, {}};

    Trades trades = match(incoming);

    if (type == OrderType::IOC && !incoming->isFilled())
        return {incoming->getID(), trades};

    if (!incoming->isFilled()) {
        if (side == Side::buy) {
            auto& list = bids_[price];
            list.push_back(incoming);
            orders_[id] = OrderEntry{ incoming, std::prev(list.end()) };
            bidsData_[price].count_++;
            bidsData_[price].qty_ += incoming->getRemainingQuantity();
        } else {
            auto& list = asks_[price];
            list.push_back(incoming);
            orders_[id] = OrderEntry{ incoming, std::prev(list.end()) };
            asksData_[price].count_++;
            asksData_[price].qty_ += incoming->getRemainingQuantity();
        }
    }

    return {incoming->getID(), trades};
}

void OrderBook::cancelOrder(OrderID orderID) {
    auto it = findOrder(orderID);
    if (it == orders_.end()) return;

    const auto [order, location] = it->second;
    Price price = order->getPrice();
    Quantity remainingQty = order->getRemainingQuantity();
    
    if (order->getSide() == Side::buy) {
        OrderPointers& list = bids_[price];
        list.erase(location);
        if (list.empty()) bids_.erase(order->getPrice()); 
        auto& level = bidsData_[price];
        level.count_--;
        level.qty_ -= remainingQty;
        if (level.count_ == 0) bidsData_.erase(price);
    } else if (order->getSide() == Side::sell) {
        OrderPointers& list = asks_[order->getPrice()];
        list.erase(location);
        if (list.empty()) asks_.erase(order->getPrice());
        auto& level = asksData_[price];
        level.count_--;
        level.qty_ -= remainingQty;
        if (level.count_ == 0) asksData_.erase(price);
    }

    orders_.erase(orderID);
}

std::optional<OrderResult> OrderBook::modifyOrder(OrderID orderID, Price newPrice, Quantity newQty) {
    auto it = findOrder(orderID);
    if (it == orders_.end()) return std::nullopt;

    OrderPointer order = it->second.order_;
    OrderType type = order->getType();
    Side side = order->getSide();
    Price oldPrice = order->getPrice();
    Quantity oldQty = order->getRemainingQuantity();

    if (newQty == 0) {
        cancelOrder(orderID);
        return OrderResult({orderID, {}});
    }

    if (newPrice == oldPrice && newQty == oldQty)
        return OrderResult({orderID, {}});

    // Friendly
    if (newPrice == oldPrice && newQty < oldQty) {
        order->fill(oldQty - newQty);
        auto& data = (side == Side::buy) ? bidsData_ : asksData_;
        data[oldPrice].qty_ -= (oldQty - newQty);
        return OrderResult({orderID, {}});
    } else { // Unfriendly
        cancelOrder(orderID);
        return addOrder(type, side, newPrice, newQty);
    }

}

void OrderBook::printState() {
    std::cout << "========================================" << '\n';
    std::cout << "           ORDER BOOK STATE" << '\n';
    std::cout << "========================================" << '\n';
    std::cout << std::left << std::setw(30) << "BIDS" << "ASKS\n";

    // Build bid string lines
    std::vector<std::string> bidLines;
    for (const auto& [price, orders] : bids_) {
        Quantity total = 0;
        for (const auto& o : orders) total += o->getRemainingQuantity();
        bidLines.push_back("€" + std::to_string(price) + " → [" 
                + std::to_string(total) + " units]");
    }

    // Build ask string lines
    std::vector<std::string> askLines;
    for (const auto& [price, orders] : asks_) {
        Quantity total = 0;
        for (const auto& o : orders) total += o->getRemainingQuantity();
        askLines.push_back("€" + std::to_string(price) + " → [" 
                + std::to_string(total) + " units]");
    }

    // Print table-like
    int rows = std::max(bidLines.size(), askLines.size());
    for (int i = 0; i < rows; i++) {
        std::string left  = i < (int)bidLines.size() ? bidLines[i] : "";
        std::string right = i < (int)askLines.size() ? askLines[i] : "";
        std::cout << std::left << std::setw(30) << left << right << "\n";
    }

    // Spread
    std::cout << "----------------------------------------\n";
    if (!bids_.empty() && !asks_.empty()) {
        Price spread = asks_.begin()->first - bids_.begin()->first;
        std::cout << "Best Bid: €" << bids_.begin()->first
            << "  |  Best Ask: €" << asks_.begin()->first
            << "  |  Spread: €" << spread << "\n";
    } else {
        std::cout << "Spread: N/A\n";
    }
    std::cout << "========================================\n\n";
}

