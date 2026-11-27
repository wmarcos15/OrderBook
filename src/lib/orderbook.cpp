#include "orderbook.hpp"
#include "order.hpp"
#include "types.hpp"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace orderbook {

static std::string nearestPowersOfTwo(std::size_t size) {
    std::size_t below = std::bit_floor(size);

    // Doubling the largest power of two would wrap around
    if (below > std::numeric_limits<std::size_t>::max() / 2)
        return "the nearest is " + std::to_string(below);
    return "the nearest are " + std::to_string(below) + " and " + std::to_string(below * 2);
}

// Runs before the ladder is built
BookConfig OrderBook::validated(const BookConfig& config) {
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
    if (config.windowSize == 0)
        throw std::invalid_argument("windowSize must be greater than 0");
    if (!std::has_single_bit(config.windowSize))
        throw std::invalid_argument(
            "windowSize (" + std::to_string(config.windowSize) +
            ") must be a power of two; " + nearestPowersOfTwo(config.windowSize));

    return config;
}

OrderBook::OrderBook(BookConfig config)
    : config_(validated(config)),
    ladder_(config_.minPrice, config_.maxPrice, config_.tickSize) {}

// Moving a vector onto itself may empty it, so it is checked here for every member
OrderBook& OrderBook::operator=(OrderBook&& other) noexcept {
    if (this != &other) {
        config_ = other.config_;
        nextID_ = other.nextID_;
        pool_ = std::move(other.pool_);
        ladder_ = std::move(other.ladder_);
        orders_ = std::move(other.orders_);
        policies_ = std::move(other.policies_);
    }
    return *this;
}

// The live slots of the pool are exactly the resting orders
std::size_t OrderBook::size() const {
    return pool_.size();
}

const Order* OrderBook::getBestBid() const {
    std::size_t best = ladder_.best<Side::buy>();
    if (best == detail::Ladder::NONE) return nullptr;
    return &ladder_.level(best).queue.head->order;
}

const Order* OrderBook::getBestAsk() const {
    std::size_t best = ladder_.best<Side::sell>();
    if (best == detail::Ladder::NONE) return nullptr;
    return &ladder_.level(best).queue.head->order;
}

bool OrderBook::empty() const {
    return pool_.size() == 0;
}

// The level is shared, so check the side's index first
std::size_t OrderBook::positionWithOrders(Side side, Price price) const {
    std::size_t position = ladder_.positionOf(price);
    if (position == detail::Ladder::NONE) return detail::Ladder::NONE;

    bool hasOrders = (side == Side::buy) ? ladder_.contains<Side::buy>(position)
                                         : ladder_.contains<Side::sell>(position);
    return hasOrders ? position : detail::Ladder::NONE;
}

Quantity OrderBook::getLevelQuantity(Side side, Price price) const {
    std::size_t position = positionWithOrders(side, price);
    if (position == detail::Ladder::NONE) return 0;
    return ladder_.level(position).data.qty_;
}

std::size_t OrderBook::getLevelCount(Side side, Price price) const {
    std::size_t position = positionWithOrders(side, price);
    if (position == detail::Ladder::NONE) return 0;
    return ladder_.level(position).data.count_;
}

// O(levels returned)
std::vector<LevelView> OrderBook::getDepth(Side side, std::size_t levels) const {
    if (side == Side::buy) return depthOf<Side::buy>(levels);
    return depthOf<Side::sell>(levels);
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

void OrderBook::rejectPrice(Price price) const {
    if (price < config_.minPrice)
        throw std::invalid_argument("price below minimum");
    if (price > config_.maxPrice)
        throw std::invalid_argument("price above maximum");
    if (price % config_.tickSize != 0)
        throw std::invalid_argument("price not on tick");

    // A moved-from book has no levels
    throw std::logic_error("the book has been moved from");
}

bool OrderBook::match(Order& incoming, std::size_t limit, OrderResult& result) {
    if (incoming.getSide() == Side::buy) return matchAgainst<Side::sell>(incoming, limit, result);
    return matchAgainst<Side::buy>(incoming, limit, result);
}

bool OrderBook::matchTop(Order& incoming, std::size_t best, OrderResult& result) {
    detail::OrderNode* resting = ladder_.level(best).queue.head;

    if (resting->order.getOwnerID() == incoming.getOwnerID()) {
        if (getSelfTradePolicy(incoming.getOwnerID()) == SelfTradePolicy::cancelNewest) {
            result.status = OrderStatus::selfTradeCancelledNewest;
            return false;
        }

        result.cancelledOrders.push_back(resting->order.getID());   // first: it can throw
        result.status = OrderStatus::selfTradeCancelledOldest;
        removeNode(resting);
        return true;
    }

    Quantity qty = std::min(incoming.getRemainingQuantity(), resting->order.getRemainingQuantity());

    bool incomingIsBuy = incoming.getSide() == Side::buy;
    OrderID bidID = incomingIsBuy ? incoming.getID() : resting->order.getID();
    OrderID askID = incomingIsBuy ? resting->order.getID() : incoming.getID();
    OwnerID bidOwner = incomingIsBuy ? incoming.getOwnerID() : resting->order.getOwnerID();
    OwnerID askOwner = incomingIsBuy ? resting->order.getOwnerID() : incoming.getOwnerID();
    result.trades.push_back({ bidID, askID, ladder_.priceOf(best), qty, bidOwner, askOwner });

    // After the push, which can throw
    incoming.fill(qty);
    ladder_.fill(resting, qty);

    if (resting->order.isFilled()) removeNode(resting);
    return true;
}

// O(n) so that updates can be O(1)
bool OrderBook::canFullyFill(const Order& order, std::size_t limit, OrderResult& result) const {
    if (order.getSide() == Side::buy) return canFullyFillAgainst<Side::sell>(order, limit, result);
    return canFullyFillAgainst<Side::buy>(order, limit, result);
}

OrderResult OrderBook::addOrder(OwnerID owner, OrderType type, Side side, Price price, Quantity qty) {
    std::size_t position = ladder_.positionOf(price);
    if (position == detail::Ladder::NONE) rejectPrice(price);
    if (qty == 0)
        throw std::invalid_argument("quantity must be greater than zero");

    return addAt(position, owner, type, side, price, qty);
}

OrderResult OrderBook::addAt(std::size_t position, OwnerID owner, OrderType type, Side side, Price price, Quantity qty) {
    OrderID id = nextID_++;
    Order incoming(id, owner, type, side, price, qty);

    OrderResult result {id, {}};

    if (type == OrderType::FOK && !canFullyFill(incoming, position, result))
        return result;

    if (!match(incoming, position, result))
        return result;

    if (type == OrderType::IOC && !incoming.isFilled())
        return result;

    if (!incoming.isFilled()) {
        // Only now: an order that never rests allocates nothing
        ladder_.add(position, store(incoming));
    }

    return result;
}

OrderResult OrderBook::addMarketOrder(OwnerID owner, Side side, Quantity qty) {
    if (qty == 0)
        throw std::invalid_argument("quantity must be greater than zero");

    // An IOC never rests, so the band edge works as the worst price
    Price price = (side == Side::buy) ? config_.maxPrice : config_.minPrice;
    std::size_t position = ladder_.positionOf(price);
    if (position == detail::Ladder::NONE) rejectPrice(price);
    return addAt(position, owner, OrderType::IOC, side, price, qty);
}

detail::OrderNode* OrderBook::store(const Order& order) {
    detail::OrderNode* node = pool_.allocate(order);
    try {
        orders_.insert(order.getID(), node);
    } catch (...) {
        pool_.release(node);
        throw;
    }
    return node;
}

void OrderBook::removeNode(detail::OrderNode* node) {
    OrderID orderID = node->order.getID();

    ladder_.remove(node);
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

    std::size_t position = ladder_.positionOf(newPrice);
    if (position == detail::Ladder::NONE) rejectPrice(newPrice);
    if (newQty == 0) 
        throw std::invalid_argument("quantity must be greater than zero");


    if (newPrice == oldPrice && newQty == oldQty)
        return OrderResult({orderID, {}});

    // Friendly
    if (newPrice == oldPrice && newQty < oldQty) {
        ladder_.fill(node, oldQty - newQty);
        return OrderResult({orderID, {}});
    }

    // Unfriendly
    Order incoming(nextID_++, owner, type, side, newPrice, newQty);
    OrderResult result {incoming.getID(), {}};
    detail::OrderNode* moved {nullptr};

    // Matching only touches the other side, so the original can stay until nothing can throw
    try {
        if (match(incoming, position, result) && !incoming.isFilled())
            moved = store(incoming);
    } catch (...) {
        // It can only stay if it has not been replaced in part
        if (!result.trades.empty() || !result.cancelledOrders.empty()) removeNode(node);
        throw;
    }

    removeNode(node);
    if (moved != nullptr) ladder_.add(position, moved);
    return result;
}

} // namespace orderbook
