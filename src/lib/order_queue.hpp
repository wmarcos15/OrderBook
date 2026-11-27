#pragma once
#include "order.hpp"
#include "types.hpp"
#include <cassert>

namespace orderbook::detail {

struct LevelData {
    Quantity qty_ {0};
    Quantity count_ {0};
};

struct OrderQueue;

struct OrderNode {
    Order order;
    OrderNode* prev {nullptr};
    OrderNode* next {nullptr};
    OrderQueue* level {nullptr};
    LevelData* levelData {nullptr};

    explicit OrderNode(const Order& o) : order(o) {}
};

static_assert(sizeof(OrderNode) <= 64);

struct OrderQueue {
    OrderNode* head {nullptr};
    OrderNode* tail {nullptr};

    bool empty() const {
        return head == nullptr && tail == nullptr;
    }

    void pushBack(OrderNode* node) {
        assert(node != nullptr);
        assert(node->prev == nullptr && node->next == nullptr);

        if (tail == nullptr) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            node->prev = tail;
            tail = node;
        }
    }

    void popFront() {
        assert(head != nullptr);
        unlink(head);
    }

    void unlink(OrderNode* node) {
        assert(node != nullptr);
        if (node->prev != nullptr)
            node->prev->next = node->next;
        if (node->next != nullptr)
            node->next->prev = node->prev;
        if (node == head)
            head = node->next;
        if (node == tail)
            tail = node->prev;
        node->prev = nullptr;
        node->next = nullptr;
    }
};

struct Level {
    OrderQueue queue;
    LevelData data;
};

} // namespace orderbook::detail
