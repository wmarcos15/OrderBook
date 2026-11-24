#include <gtest/gtest.h>
#include <vector>
#include "order_queue.hpp"

using namespace orderbook;
using namespace orderbook::detail;

using Ids = std::vector<OrderID>;

static OrderNode makeNode(OrderID id) {
    return OrderNode(Order(id, 1, OrderType::GTC, Side::buy, 100, 10));
}

static Ids forwardIds(const OrderQueue& q) {
    Ids ids;
    for (const OrderNode* n = q.head; n != nullptr; n = n->next)
        ids.push_back(n->order.getID());
    return ids;
}

static Ids backwardIds(const OrderQueue& q) {
    Ids ids;
    for (const OrderNode* n = q.tail; n != nullptr; n = n->prev)
        ids.push_back(n->order.getID());
    return ids;
}

// --------------------------
// Initial state
// --------------------------
TEST(OrderQueueTest, NewQueue_IsEmpty) {
    OrderQueue q;

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.head, nullptr);
    EXPECT_EQ(q.tail, nullptr);
}

TEST(OrderQueueTest, OrderNode_StartsUnlinked) {
    OrderNode a = makeNode(7);

    EXPECT_EQ(a.prev, nullptr);
    EXPECT_EQ(a.next, nullptr);
    EXPECT_EQ(a.level, nullptr);
    EXPECT_EQ(a.levelData, nullptr);
    EXPECT_EQ(a.order.getID(), 7u);
}

TEST(OrderQueueTest, LevelData_StartsAtZero) {
    LevelData data;

    EXPECT_EQ(data.qty_, 0u);
    EXPECT_EQ(data.count_, 0u);
}

// --------------------------
// pushBack
// --------------------------
TEST(OrderQueueTest, PushBack_OneNode_HeadAndTailAreTheNode) {
    OrderQueue q;
    OrderNode a = makeNode(1);

    q.pushBack(&a);

    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.head, &a);
    EXPECT_EQ(q.tail, &a);
    EXPECT_EQ(a.prev, nullptr);
    EXPECT_EQ(a.next, nullptr);
}

TEST(OrderQueueTest, PushBack_ThreeNodes_ForwardAndBackwardFollowInsertionOrder) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);

    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    EXPECT_EQ(forwardIds(q), (Ids {1, 2, 3}));
    EXPECT_EQ(backwardIds(q), (Ids {3, 2, 1}));
    EXPECT_EQ(q.head, &a);
    EXPECT_EQ(q.tail, &c);
}

// --------------------------
// popFront
// --------------------------
TEST(OrderQueueTest, PopFront_ThreeNodes_HeadAdvances) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    q.popFront();

    EXPECT_EQ(q.head, &b);
    EXPECT_EQ(b.prev, nullptr);
    EXPECT_EQ(q.tail, &c);
    EXPECT_EQ(forwardIds(q), (Ids {2, 3}));
    EXPECT_EQ(backwardIds(q), (Ids {3, 2}));
    EXPECT_EQ(a.prev, nullptr);
    EXPECT_EQ(a.next, nullptr);
}

TEST(OrderQueueTest, PopFront_OnlyNode_QueueBecomesEmpty) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    q.pushBack(&a);

    q.popFront();

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.head, nullptr);
    EXPECT_EQ(q.tail, nullptr);
    EXPECT_EQ(a.prev, nullptr);
    EXPECT_EQ(a.next, nullptr);
}

TEST(OrderQueueTest, PopFront_UntilEmpty_ReturnsNodesInInsertionOrder) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    OrderNode d = makeNode(4);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);
    q.pushBack(&d);

    Ids popped;
    while (!q.empty()) {
        popped.push_back(q.head->order.getID());
        q.popFront();
    }

    EXPECT_EQ(popped, (Ids {1, 2, 3, 4}));
}

// --------------------------
// unlink
// --------------------------
TEST(OrderQueueTest, Unlink_Head) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    q.unlink(&a);

    EXPECT_EQ(forwardIds(q), (Ids {2, 3}));
    EXPECT_EQ(backwardIds(q), (Ids {3, 2}));
    EXPECT_EQ(q.head, &b);
    EXPECT_EQ(b.prev, nullptr);
}

TEST(OrderQueueTest, Unlink_Tail) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    q.unlink(&c);

    EXPECT_EQ(forwardIds(q), (Ids {1, 2}));
    EXPECT_EQ(backwardIds(q), (Ids {2, 1}));
    EXPECT_EQ(q.tail, &b);
    EXPECT_EQ(b.next, nullptr);
}

TEST(OrderQueueTest, Unlink_Middle) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    q.unlink(&b);

    EXPECT_EQ(forwardIds(q), (Ids {1, 3}));
    EXPECT_EQ(backwardIds(q), (Ids {3, 1}));
    EXPECT_EQ(q.head, &a);
    EXPECT_EQ(q.tail, &c);
}

TEST(OrderQueueTest, Unlink_OnlyNode) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    q.pushBack(&a);

    q.unlink(&a);

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.head, nullptr);
    EXPECT_EQ(q.tail, nullptr);
}

TEST(OrderQueueTest, Unlink_ClearsTheNodesOwnLinks) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    q.unlink(&b);
    EXPECT_EQ(b.prev, nullptr);
    EXPECT_EQ(b.next, nullptr);

    q.unlink(&a);
    EXPECT_EQ(a.prev, nullptr);
    EXPECT_EQ(a.next, nullptr);

    q.unlink(&c);
    EXPECT_EQ(c.prev, nullptr);
    EXPECT_EQ(c.next, nullptr);
}

TEST(OrderQueueTest, Unlink_AllNodesInArbitraryOrder_LeavesQueueEmpty) {
    OrderQueue q;
    OrderNode n1 = makeNode(1);
    OrderNode n2 = makeNode(2);
    OrderNode n3 = makeNode(3);
    OrderNode n4 = makeNode(4);
    OrderNode n5 = makeNode(5);
    q.pushBack(&n1);
    q.pushBack(&n2);
    q.pushBack(&n3);
    q.pushBack(&n4);
    q.pushBack(&n5);

    q.unlink(&n3);
    EXPECT_EQ(forwardIds(q), (Ids {1, 2, 4, 5}));
    EXPECT_EQ(backwardIds(q), (Ids {5, 4, 2, 1}));

    q.unlink(&n1);
    EXPECT_EQ(forwardIds(q), (Ids {2, 4, 5}));
    EXPECT_EQ(backwardIds(q), (Ids {5, 4, 2}));

    q.unlink(&n5);
    EXPECT_EQ(forwardIds(q), (Ids {2, 4}));
    EXPECT_EQ(backwardIds(q), (Ids {4, 2}));

    q.unlink(&n2);
    EXPECT_EQ(forwardIds(q), (Ids {4}));
    EXPECT_EQ(backwardIds(q), (Ids {4}));

    q.unlink(&n4);
    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(forwardIds(q).empty());
    EXPECT_TRUE(backwardIds(q).empty());
}

// --------------------------
// Reuse of unlinked nodes
// --------------------------
TEST(OrderQueueTest, Unlinked_NodeCanBePushedAgain_AndGoesToTheBack) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    q.pushBack(&a);
    q.pushBack(&b);
    q.pushBack(&c);

    q.unlink(&b);
    q.pushBack(&b);

    EXPECT_EQ(forwardIds(q), (Ids {1, 3, 2}));
    EXPECT_EQ(backwardIds(q), (Ids {2, 3, 1}));
    EXPECT_EQ(q.tail, &b);
}

TEST(OrderQueueTest, Unlinked_NodeCanJoinAnotherQueue) {
    OrderQueue qa;
    OrderQueue qb;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    OrderNode d = makeNode(4);
    qa.pushBack(&a);
    qa.pushBack(&b);
    qa.pushBack(&c);
    qb.pushBack(&d);

    qa.unlink(&b);
    qb.pushBack(&b);

    EXPECT_EQ(forwardIds(qa), (Ids {1, 3}));
    EXPECT_EQ(backwardIds(qa), (Ids {3, 1}));
    EXPECT_EQ(forwardIds(qb), (Ids {4, 2}));
    EXPECT_EQ(backwardIds(qb), (Ids {2, 4}));
}

TEST(OrderQueueTest, PushBack_AfterQueueWasEmptied_Works) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    q.pushBack(&a);
    q.popFront();

    q.pushBack(&b);

    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.head, &b);
    EXPECT_EQ(q.tail, &b);
    EXPECT_EQ(b.prev, nullptr);
    EXPECT_EQ(b.next, nullptr);
}

TEST(OrderQueueTest, Interleaved_PushAndPop_KeepsFifoOrder) {
    OrderQueue q;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    OrderNode c = makeNode(3);
    OrderNode d = makeNode(4);
    Ids popped;

    q.pushBack(&a);
    q.pushBack(&b);
    popped.push_back(q.head->order.getID());
    q.popFront();
    q.pushBack(&c);
    popped.push_back(q.head->order.getID());
    q.popFront();
    q.pushBack(&d);

    EXPECT_EQ(popped, (Ids {1, 2}));
    EXPECT_EQ(forwardIds(q), (Ids {3, 4}));
    EXPECT_EQ(backwardIds(q), (Ids {4, 3}));
}
