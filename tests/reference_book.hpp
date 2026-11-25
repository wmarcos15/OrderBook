#pragma once
#include "test_utils.hpp"
#include "trade.hpp"
#include "types.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orderbook::test {

struct ReferenceOrder {
    OrderID id;
    OwnerID owner;
    Side side;
    Price price;
    Quantity quantity;
};

// The rules of the book written the plain way, to compare the engine with: a
// list of orders by arrival, searched from the start every time.
class ReferenceBook {
private:
    std::vector<ReferenceOrder> orders_;
    std::unordered_map<OwnerID, SelfTradePolicy> policies_;
    OrderID nextID_ {1};

    static bool isBetter(Side side, Price a, Price b) {
        return (side == Side::buy) ? a > b : a < b;
    }

    std::vector<ReferenceOrder>::iterator locate(OrderID id) {
        return std::find_if(orders_.begin(), orders_.end(),
                            [&](const ReferenceOrder& order) { return order.id == id; });
    }

    // Trades what it can and returns what is left
    Quantity match(const ReferenceOrder& incoming, OrderResult& result) {
        Quantity remaining = incoming.quantity;
        Side opposite = otherSide(incoming.side);

        while (remaining > 0) {
            const ReferenceOrder* resting = best(opposite);
            if (resting == nullptr || isBetter(incoming.side, resting->price, incoming.price)) break;

            auto it = locate(resting->id);
            if (it->owner == incoming.owner) {
                if (getPolicy(incoming.owner) == SelfTradePolicy::cancelNewest) {
                    result.status = OrderStatus::selfTradeCancelledNewest;
                    break;
                }
                result.status = OrderStatus::selfTradeCancelledOldest;
                result.cancelledOrders.push_back(it->id);
                orders_.erase(it);
                continue;
            }

            Quantity quantity = std::min(remaining, it->quantity);
            const ReferenceOrder& bid = (incoming.side == Side::buy) ? incoming : *it;
            const ReferenceOrder& ask = (incoming.side == Side::buy) ? *it : incoming;
            result.trades.push_back({bid.id, ask.id, it->price, quantity, bid.owner, ask.owner});

            remaining -= quantity;
            it->quantity -= quantity;
            if (it->quantity == 0) orders_.erase(it);
        }

        return remaining;
    }

public:
    OrderResult add(OwnerID owner, OrderType type, Side side, Price price, Quantity quantity) {
        ReferenceOrder incoming {nextID_++, owner, side, price, quantity};
        OrderResult result {incoming.id, {}};

        if (type == OrderType::FOK) {
            ReferenceBook trial = *this;
            OrderResult trialResult {incoming.id, {}};
            if (trial.match(incoming, trialResult) > 0) {
                if (trialResult.status == OrderStatus::selfTradeCancelledNewest)
                    result.status = trialResult.status;
                return result;
            }
        }

        Quantity remaining = match(incoming, result);
        bool rests = type == OrderType::GTC && remaining > 0
            && result.status != OrderStatus::selfTradeCancelledNewest;
        if (rests) {
            incoming.quantity = remaining;
            orders_.push_back(incoming);
        }
        return result;
    }

    OrderResult addMarket(OwnerID owner, Side side, Quantity quantity) {
        Price worst = (side == Side::buy) ? std::numeric_limits<Price>::max() : 0;
        return add(owner, OrderType::IOC, side, worst, quantity);
    }

    void cancel(OrderID id) {
        auto it = locate(id);
        if (it != orders_.end()) orders_.erase(it);
    }

    std::optional<OrderResult> modify(OrderID id, Price price, Quantity quantity) {
        auto it = locate(id);
        if (it == orders_.end()) return std::nullopt;

        if (price == it->price && quantity <= it->quantity) {
            it->quantity = quantity;
            return OrderResult {id, {}};
        }

        ReferenceOrder old = *it;
        orders_.erase(it);
        return add(old.owner, OrderType::GTC, old.side, price, quantity);
    }

    void setPolicy(OwnerID owner, SelfTradePolicy policy) {
        policies_[owner] = policy;
    }

    SelfTradePolicy getPolicy(OwnerID owner) const {
        auto it = policies_.find(owner);
        return (it == policies_.end()) ? SelfTradePolicy::cancelNewest : it->second;
    }

    // The oldest order at the best price
    const ReferenceOrder* best(Side side) const {
        const ReferenceOrder* found {nullptr};
        for (const ReferenceOrder& order : orders_) {
            if (order.side != side) continue;
            if (found == nullptr || isBetter(side, order.price, found->price)) found = &order;
        }
        return found;
    }

    const std::vector<ReferenceOrder>& orders() const {
        return orders_;
    }

    OrderID nextID() const {
        return nextID_;
    }

    Levels levels(Side side) const {
        std::map<Price, std::pair<Quantity, std::size_t>> totals;
        for (const ReferenceOrder& order : orders_) {
            if (order.side != side) continue;
            auto& [quantity, count] = totals[order.price];
            quantity += order.quantity;
            ++count;
        }

        Levels levels;
        for (const auto& [price, total] : totals)
            levels.push_back({price, total.first, total.second});
        if (side == Side::buy) std::reverse(levels.begin(), levels.end());
        return levels;
    }
};

} // namespace orderbook::test
