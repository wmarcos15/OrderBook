#include <gtest/gtest.h>
#include <cstddef>
#include <deque>
#include <vector>
#include "order_queue.hpp"
#include "queue_test_utils.hpp"
#include "types.hpp"

using namespace orderbook;
using namespace orderbook::detail;
using namespace orderbook::test;

using IDs = std::vector<OrderID>;

class OrderQueueTest : public ::testing::Test {
protected:
    OrderQueue queue_;

    // A deque, so that the nodes never move
    std::deque<OrderNode> nodes_;

    // Nodes with IDs from 1, so the one at index i has the ID i + 1
    void push(std::size_t count) {
        for (std::size_t i {0}; i < count; ++i) {
            nodes_.push_back(makeNode(nodes_.size() + 1));
            queue_.pushBack(&nodes_.back());
        }
    }

    static void expectUnlinked(const OrderNode& node) {
        EXPECT_EQ(node.prev, nullptr);
        EXPECT_EQ(node.next, nullptr);
    }

    static void expectHolds(const OrderQueue& queue, const IDs& ids) {
        EXPECT_EQ(forwardIDs(queue), ids);
        EXPECT_EQ(backwardIDs(queue), IDs(ids.rbegin(), ids.rend()));
        EXPECT_EQ(queue.empty(), ids.empty());
        if (!ids.empty()) return;
        EXPECT_EQ(queue.head, nullptr);
        EXPECT_EQ(queue.tail, nullptr);
    }
};

// --------------------------
// Initial state
// --------------------------
TEST_F(OrderQueueTest, NewQueue_IsEmpty) {
    expectHolds(queue_, {});
}

TEST_F(OrderQueueTest, NewNode_IsUnlinked) {
    OrderNode node = makeNode(7);

    expectUnlinked(node);
    EXPECT_EQ(node.level, nullptr);
    EXPECT_EQ(node.levelData, nullptr);
    EXPECT_EQ(node.order.getID(), 7u);
}

TEST_F(OrderQueueTest, NewLevelData_IsZero) {
    LevelData data;

    EXPECT_EQ(data.qty_, 0u);
    EXPECT_EQ(data.count_, 0u);
}

// --------------------------
// pushBack
// --------------------------
TEST_F(OrderQueueTest, PushBack_OneNode_IsHeadAndTail) {
    push(1);

    EXPECT_EQ(queue_.head, &nodes_[0]);
    EXPECT_EQ(queue_.tail, &nodes_[0]);
    expectUnlinked(nodes_[0]);
    expectHolds(queue_, {1});
}

TEST_F(OrderQueueTest, PushBack_ThreeNodes_InOrderOfArrival) {
    push(3);

    EXPECT_EQ(queue_.head, &nodes_[0]);
    EXPECT_EQ(queue_.tail, &nodes_[2]);
    expectHolds(queue_, {1, 2, 3});
}

TEST_F(OrderQueueTest, PushBack_AfterTheQueueWasEmptied_Works) {
    push(1);
    queue_.unlink(&nodes_[0]);

    push(1);

    expectHolds(queue_, {2});
    expectUnlinked(nodes_[1]);
}

// --------------------------
// popFront
// --------------------------
TEST_F(OrderQueueTest, PopFront_UntilEmpty_OldestFirst) {
    push(3);

    queue_.popFront();
    EXPECT_EQ(queue_.head, &nodes_[1]);
    expectUnlinked(nodes_[0]);
    expectHolds(queue_, {2, 3});

    queue_.popFront();
    expectHolds(queue_, {3});

    queue_.popFront();
    expectUnlinked(nodes_[2]);
    expectHolds(queue_, {});
}

TEST_F(OrderQueueTest, PopFrontAndPushBack_Interleaved_KeepTheOrder) {
    push(2);
    queue_.popFront();
    push(1);
    queue_.popFront();
    push(1);

    expectHolds(queue_, {3, 4});
}

// --------------------------
// unlink
// --------------------------
TEST_F(OrderQueueTest, Unlink_EachOfThree_OthersLinkUp) {
    for (std::size_t unlinked {0}; unlinked < 3; ++unlinked) {
        queue_ = {};
        nodes_.clear();
        push(3);

        queue_.unlink(&nodes_[unlinked]);

        IDs left {1, 2, 3};
        std::erase(left, unlinked + 1);
        expectHolds(queue_, left);
        expectUnlinked(nodes_[unlinked]);
    }
}

TEST_F(OrderQueueTest, Unlink_OnlyNode_QueueIsEmpty) {
    push(1);

    queue_.unlink(&nodes_[0]);

    expectHolds(queue_, {});
    expectUnlinked(nodes_[0]);
}

TEST_F(OrderQueueTest, Unlink_AllInAnyOrder_LeavesTheQueueEmpty) {
    push(5);

    queue_.unlink(&nodes_[2]);
    expectHolds(queue_, {1, 2, 4, 5});
    queue_.unlink(&nodes_[0]);
    expectHolds(queue_, {2, 4, 5});
    queue_.unlink(&nodes_[4]);
    expectHolds(queue_, {2, 4});
    queue_.unlink(&nodes_[1]);
    expectHolds(queue_, {4});
    queue_.unlink(&nodes_[3]);
    expectHolds(queue_, {});

    for (const OrderNode& node : nodes_)
        expectUnlinked(node);
}

TEST_F(OrderQueueTest, Unlinked_PushedAgain_GoesToTheBack) {
    push(3);
    queue_.unlink(&nodes_[1]);

    queue_.pushBack(&nodes_[1]);

    expectHolds(queue_, {1, 3, 2});
}

TEST_F(OrderQueueTest, Unlinked_CanJoinAnotherQueue) {
    OrderQueue other;
    OrderNode node = makeNode(9);
    other.pushBack(&node);
    push(3);
    queue_.unlink(&nodes_[1]);

    other.pushBack(&nodes_[1]);

    expectHolds(queue_, {1, 3});
    expectHolds(other, {9, 2});
}
