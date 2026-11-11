#include "orderbook.hpp"
#include "order.hpp"
#include "types.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <string>

OrderBook::OrderBook(BookConfig config) : config_(config) {
    if (config.tickSize <= 0)
        throw std::invalid_argument("tickSize must be greater than 0");
    if (config.minPrice <= 0)
        throw std::invalid_argument("minPrice must be greater than 0");
    if (config.minPrice > config.maxPrice)
        throw std::invalid_argument("minPrice must not exceed maxPrice");
    if (config.maxPrice % config.tickSize != 0)
        throw std::invalid_argument(
            "maxPrice (" + std::to_string(config.maxPrice) +
            ") must be a multiple of tickSize (" + std::to_string(config.tickSize) +
            "); set minPrice and maxPrice to multiples of the tick");
    if (config.minPrice % config.tickSize != 0)
        throw std::invalid_argument(
            "minPrice (" + std::to_string(config.minPrice) +
            ") must be a multiple of tickSize (" + std::to_string(config.tickSize) +
            "); set minPrice and maxPrice to multiples of the tick");
}

OrderBook::OrderIterator OrderBook::findOrder(OrderID id) {
    return orders_.find(id);
}

std::size_t OrderBook::size() const {
    return orders_.size();
}

const Order* OrderBook::getBestBid() const {
    if (bids_.empty()) return nullptr;
    return bids_.begin()->second.front().get();
}

const Order* OrderBook::getBestAsk() const {
    if (asks_.empty()) return nullptr;
    return asks_.begin()->second.front().get();
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

const BookConfig& OrderBook::getBookConfig() const {
    return config_;
}

void OrderBook::validatePrice(Price price) const {
    if (price < config_.minPrice)
        throw std::invalid_argument("price below minimum");
    if (price > config_.maxPrice)
        throw std::invalid_argument("price above maximum");
    if (price % config_.tickSize != 0)
        throw std::invalid_argument("price not on tick");
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

OrderResult OrderBook::addOrder(OwnerID owner, OrderType type, Side side, Price price, Quantity qty) {
    validatePrice(price);
    if (qty == 0)
        throw std::invalid_argument("quantity must be greater than zero");

    OrderID id = nextID_++;
    OrderPointer incoming = std::make_shared<Order>(id, owner, type, side, price, qty);

    if (type == OrderType::FOK && !canFullyFill(incoming))
        return {id, {}};

    Trades trades = match(incoming);

    if (type == OrderType::IOC && !incoming->isFilled())
        return {incoming->getID(), trades};

    if (!incoming->isFilled()) {
        auto& list  = (side == Side::buy) ? bids_[price]     : asks_[price];
        auto& level = (side == Side::buy) ? bidsData_[price] : asksData_[price];
        list.push_back(incoming);
        level.count_++;
        level.qty_ += incoming->getRemainingQuantity();
        orders_[id] = OrderEntry{ incoming, std::prev(list.end()), &list, &level };
    }

    return {incoming->getID(), trades};
}

OrderResult OrderBook::addMarketOrder(OwnerID owner, Side side, Quantity qty) {
    Price price;
    if (side == Side::buy) {
        if (asks_.empty()) return {nextID_++, {}};
        price = asks_.rbegin()->first;
    } else {
        if (bids_.empty()) return {nextID_++, {}};
        price = (bids_.rbegin())->first;
    }
    return addOrder(owner, OrderType::IOC, side, price, qty);
}

void OrderBook::cancelOrder(OrderID orderID) {
    auto it = findOrder(orderID);
    if (it == orders_.end()) return;

    const auto& [order, location, list, level] = it->second;
    Side side = order->getSide();
    Price price = order->getPrice();
    Quantity remainingQty = order->getRemainingQuantity();

    // No lookup by price: the entry already points at its level.
    list->erase(location);
    level->count_--;
    level->qty_ -= remainingQty;

    // Only an emptied level is looked up by price, and that is rare.
    if (list->empty()) {
        if (side == Side::buy) {
            bids_.erase(price);
            bidsData_.erase(price);
        } else {
            asks_.erase(price);
            asksData_.erase(price);
        }
    }

    orders_.erase(orderID);
}

std::optional<OrderResult> OrderBook::modifyOrder(OrderID orderID, Price newPrice, Quantity newQty) {
    auto it = findOrder(orderID);
    if (it == orders_.end()) return std::nullopt;

    OrderPointer order = it->second.order_;
    OwnerID owner = order->getOwnerID();
    OrderType type = order->getType();
    Side side = order->getSide();
    Price oldPrice = order->getPrice();
    Quantity oldQty = order->getRemainingQuantity();

    validatePrice(newPrice);
    if (newQty == 0) 
        throw std::invalid_argument("quantity must be greater than zero");


    if (newPrice == oldPrice && newQty == oldQty)
        return OrderResult({orderID, {}});

    // Friendly
    if (newPrice == oldPrice && newQty < oldQty) {
        order->fill(oldQty - newQty);
        it->second.levelData_->qty_ -= (oldQty - newQty);
        return OrderResult({orderID, {}});
    } else { // Unfriendly
        cancelOrder(orderID);
        return addOrder(owner, type, side, newPrice, newQty);
    }

}

