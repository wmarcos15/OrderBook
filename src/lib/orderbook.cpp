#include "orderbook.hpp"
#include "order.hpp"
#include "types.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace orderbook {

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

// The live slots of the pool are exactly the resting orders
std::size_t OrderBook::size() const {
    return pool_.size();
}

const Order* OrderBook::getBestBid() const {
    if (bids_.empty()) return nullptr;
    return &bids_.begin()->second.head->order;
}

const Order* OrderBook::getBestAsk() const {
    if (asks_.empty()) return nullptr;
    return &asks_.begin()->second.head->order;
}

bool OrderBook::empty() const {
    return pool_.size() == 0;
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

// O(levels returned)
std::vector<LevelView> OrderBook::getDepth(Side side, std::size_t levels) const {
    if (side == Side::buy) return depthOf(bids_, bidsData_, levels);
    return depthOf(asks_, asksData_, levels);
}

const Order* OrderBook::getOrder(OrderID id) const {
    const detail::OrderNode* node = orders_.find(id);
    if (node == nullptr) return nullptr;
    return &node->order;
}

const BookConfig& OrderBook::getBookConfig() const {
    return config_;
}

SelfTradePolicy OrderBook::getSelfTradePolicy(OwnerID ownerID) const {
    auto it = policies_.find(ownerID);
    if (it != policies_.end()) return it->second;
    return config_.defaultSelfTradePolicy;
}

void OrderBook::setSelfTradePolicy(OwnerID ownerID, SelfTradePolicy policy) {
    policies_[ownerID] = policy;
}

void OrderBook::validatePrice(Price price) const {
    if (price < config_.minPrice)
        throw std::invalid_argument("price below minimum");
    if (price > config_.maxPrice)
        throw std::invalid_argument("price above maximum");
    if (price % config_.tickSize != 0)
        throw std::invalid_argument("price not on tick");
}

bool OrderBook::match(Order& incoming, OrderResult& result) {
    while (incoming.getRemainingQuantity() > 0) {
        if (incoming.getSide() == Side::buy) {
            if (asks_.empty() || incoming.getPrice() < asks_.begin()->first) break;
            if (!matchTop(incoming, asks_, asksData_, result)) return false;
        } else {
            if (bids_.empty() || incoming.getPrice() > bids_.begin()->first) break;
            if (!matchTop(incoming, bids_, bidsData_, result)) return false;
        }
    }

    return true;
}

// O(n) so that updates can be O(1)
bool OrderBook::canFullyFill(const Order& order, OrderResult& result) const {
    if (order.getSide() == Side::buy) return canFullyFillAgainst(order, asks_, result);
    return canFullyFillAgainst(order, bids_, result);
}

OrderResult OrderBook::addOrder(OwnerID owner, OrderType type, Side side, Price price, Quantity qty) {
    validatePrice(price);
    if (qty == 0)
        throw std::invalid_argument("quantity must be greater than zero");

    OrderID id = nextID_++;
    Order incoming(id, owner, type, side, price, qty);

    OrderResult result {id, {}};

    if (type == OrderType::FOK && !canFullyFill(incoming, result))
        return result;

    if (!match(incoming, result))
        return result;

    if (type == OrderType::IOC && !incoming.isFilled())
        return result;

    if (!incoming.isFilled()) {
        // The slot is taken only now: an order that never rests allocates nothing
        detail::OrderNode* node = pool_.allocate(incoming);
        auto& queue = (side == Side::buy) ? bids_[price] : asks_[price];
        auto& level = (side == Side::buy) ? bidsData_[price] : asksData_[price];

        node->level = &queue;
        node->levelData = &level;
        queue.pushBack(node);
        level.count_++;
        level.qty_ += incoming.getRemainingQuantity();
        orders_.insert(id, node);
    }

    return result;
}

OrderResult OrderBook::addMarketOrder(OwnerID owner, Side side, Quantity qty) {
    // Checked here as well: against an empty opposite side addOrder is never reached
    if (qty == 0)
        throw std::invalid_argument("quantity must be greater than zero");

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

void OrderBook::removeNode(detail::OrderNode* node) {
    OrderID orderID = node->order.getID();
    Side side = node->order.getSide();
    Price price = node->order.getPrice();
    Quantity remainingQty = node->order.getRemainingQuantity();
    detail::OrderQueue* queue = node->level;
    detail::LevelData* level = node->levelData;

    // No lookup by price: the node already points at its level.
    queue->unlink(node);
    level->count_--;
    level->qty_ -= remainingQty;

    // Only an emptied level is looked up by price, and that is rare.
    // Erasing it destroys the queue and the totals, so `queue` and `level` are dead after this.
    if (queue->empty()) {
        if (side == Side::buy) {
            bids_.erase(price);
            bidsData_.erase(price);
        } else {
            asks_.erase(price);
            asksData_.erase(price);
        }
    }

    orders_.erase(orderID);
    pool_.release(node);   // last: nothing reads the node after this
}

void OrderBook::cancelOrder(OrderID orderID) {
    detail::OrderNode* node = orders_.find(orderID);
    if (node == nullptr) return;

    removeNode(node);
}

std::optional<OrderResult> OrderBook::modifyOrder(OrderID orderID, Price newPrice, Quantity newQty) {
    detail::OrderNode* node = orders_.find(orderID);
    if (node == nullptr) return std::nullopt;

    OwnerID owner = node->order.getOwnerID();
    OrderType type = node->order.getType();
    Side side = node->order.getSide();
    Price oldPrice = node->order.getPrice();
    Quantity oldQty = node->order.getRemainingQuantity();

    validatePrice(newPrice);
    if (newQty == 0) 
        throw std::invalid_argument("quantity must be greater than zero");


    if (newPrice == oldPrice && newQty == oldQty)
        return OrderResult({orderID, {}});

    // Friendly
    if (newPrice == oldPrice && newQty < oldQty) {
        node->order.fill(oldQty - newQty);
        node->levelData->qty_ -= (oldQty - newQty);
        return OrderResult({orderID, {}});
    } else { // Unfriendly
        removeNode(node);
        return addOrder(owner, type, side, newPrice, newQty);
    }

}

} // namespace orderbook
