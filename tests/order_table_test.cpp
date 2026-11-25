#include <gtest/gtest.h>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "order_table.hpp"

using namespace orderbook;
using namespace orderbook::detail;

using SmallTable = OrderTable<4>;
using Ids = std::vector<OrderID>;
using Nodes = std::vector<OrderNode>;

static OrderNode makeNode(OrderID id) {
    return OrderNode(Order(id, 1, OrderType::GTC, Side::buy, 100, 10));
}

// Fully built before any address is taken, so the nodes never move
static Nodes makeNodes(const Ids& ids) {
    Nodes nodes;
    nodes.reserve(ids.size());
    for (OrderID id : ids)
        nodes.push_back(makeNode(id));
    return nodes;
}

// --------------------------
// Empty table
// --------------------------
TEST(OrderTableTest, NewTable_Find_ReturnsNull) {
    SmallTable table;

    EXPECT_EQ(table.find(0), nullptr);
    EXPECT_EQ(table.find(1), nullptr);
    EXPECT_EQ(table.find(std::numeric_limits<OrderID>::max()), nullptr);
}

TEST(OrderTableTest, NewTable_Find_AllocatesNothing) {
    SmallTable table;

    table.find(0);
    table.find(1);
    table.find(std::numeric_limits<OrderID>::max());

    EXPECT_EQ(table.pageCount(), 0u);
}

// --------------------------
// Insert and find
// --------------------------
TEST(OrderTableTest, Insert_OneOrder_FindReturnsSamePointer) {
    SmallTable table;
    OrderNode a = makeNode(5);

    table.insert(5, &a);

    EXPECT_EQ(table.find(5), &a);
}

TEST(OrderTableTest, Insert_OneOrder_OtherIdsOfThePageAreNull) {
    SmallTable table;
    OrderNode a = makeNode(5);

    table.insert(5, &a);

    EXPECT_EQ(table.find(4), nullptr);
    EXPECT_EQ(table.find(6), nullptr);
    EXPECT_EQ(table.find(7), nullptr);
}

TEST(OrderTableTest, Insert_FourOrdersOfOnePage_EachIdFindsItsOwnNode) {
    SmallTable table;
    Nodes nodes = makeNodes({4, 5, 6, 7});

    for (OrderNode& node : nodes)
        table.insert(node.order.getID(), &node);

    for (OrderNode& node : nodes)
        EXPECT_EQ(table.find(node.order.getID()), &node);
    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, Insert_IdsAtAPageEdge_BothFoundInTwoPages) {
    SmallTable table;
    OrderNode a = makeNode(3);
    OrderNode b = makeNode(4);

    table.insert(3, &a);
    table.insert(4, &b);

    EXPECT_EQ(table.find(3), &a);
    EXPECT_EQ(table.find(4), &b);
    EXPECT_EQ(table.pageCount(), 2u);
}

TEST(OrderTableTest, Find_IdsPastTheDirectory_ReturnNullAndAllocateNothing) {
    SmallTable table;
    OrderNode a = makeNode(1);
    table.insert(1, &a);

    EXPECT_EQ(table.find(4), nullptr);
    EXPECT_EQ(table.find(100), nullptr);
    EXPECT_EQ(table.find(std::numeric_limits<OrderID>::max()), nullptr);
    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, Insert_WithAGap_SkippedIdsAreNull) {
    SmallTable table;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(41);

    table.insert(1, &a);
    table.insert(41, &b);

    EXPECT_EQ(table.find(1), &a);
    EXPECT_EQ(table.find(41), &b);
    for (OrderID id = 2; id <= 40; ++id)
        EXPECT_EQ(table.find(id), nullptr) << "id " << id;
    EXPECT_EQ(table.pageCount(), 2u);
}

TEST(OrderTableTest, Insert_LowerSkippedEntryAfterAHigherOne_BothFound) {
    SmallTable table;
    OrderNode a = makeNode(41);
    OrderNode b = makeNode(1);

    table.insert(41, &a);
    table.insert(1, &b);

    EXPECT_EQ(table.find(41), &a);
    EXPECT_EQ(table.find(1), &b);
}

TEST(OrderTableTest, Insert_OneOrderInEachOfAHundredPages_AllStillFound) {
    SmallTable table;
    Ids ids;
    for (OrderID i = 0; i < 100; ++i)
        ids.push_back(i * 4 + 1);
    Nodes nodes = makeNodes(ids);

    for (OrderNode& node : nodes)
        table.insert(node.order.getID(), &node);

    for (OrderNode& node : nodes)
        EXPECT_EQ(table.find(node.order.getID()), &node);
    EXPECT_EQ(table.pageCount(), 100u);
}

// --------------------------
// Erase
// --------------------------
TEST(OrderTableTest, Erase_OneOrder_IdIsNullAndNeighboursRemain) {
    SmallTable table;
    Nodes nodes = makeNodes({4, 5, 6});
    for (OrderNode& node : nodes)
        table.insert(node.order.getID(), &node);

    table.erase(5);

    EXPECT_EQ(table.find(5), nullptr);
    EXPECT_EQ(table.find(4), &nodes[0]);
    EXPECT_EQ(table.find(6), &nodes[2]);
}

TEST(OrderTableTest, Erase_OneOfTwoInAPage_KeepsThePageForTheOther) {
    SmallTable table;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    table.insert(1, &a);
    table.insert(2, &b);

    table.erase(1);

    EXPECT_EQ(table.find(2), &b);
    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, Insert_AfterEraseInALivePage_BothFoundInOnePage) {
    SmallTable table;
    Nodes nodes = makeNodes({1, 2, 3});
    table.insert(1, &nodes[0]);
    table.insert(2, &nodes[1]);
    table.erase(1);

    table.insert(3, &nodes[2]);

    EXPECT_EQ(table.find(2), &nodes[1]);
    EXPECT_EQ(table.find(3), &nodes[2]);
    EXPECT_EQ(table.pageCount(), 1u);
}

// --------------------------
// Page reuse
// --------------------------
TEST(OrderTableTest, Insert_AfterAPageEmptied_ReusesThePageForAnotherEntry) {
    SmallTable table;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(5);
    table.insert(1, &a);
    table.erase(1);

    table.insert(5, &b);

    EXPECT_EQ(table.find(5), &b);
    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, Insert_IntoAReusedPage_PageIsClean) {
    SmallTable table;
    Nodes nodes = makeNodes({4, 5, 6, 7, 9});
    for (int i = 0; i < 4; ++i)
        table.insert(nodes[i].order.getID(), &nodes[i]);
    for (OrderID id = 4; id <= 7; ++id)
        table.erase(id);

    table.insert(9, &nodes[4]);

    EXPECT_EQ(table.find(9), &nodes[4]);
    EXPECT_EQ(table.find(8), nullptr);
    EXPECT_EQ(table.find(10), nullptr);
    EXPECT_EQ(table.find(11), nullptr);
    for (OrderID id = 4; id <= 7; ++id)
        EXPECT_EQ(table.find(id), nullptr) << "id " << id;
    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, Insert_IntoAnEntryThatLostItsPage_GetsAPageAgain) {
    SmallTable table;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(2);
    table.insert(1, &a);
    table.erase(1);

    table.insert(2, &b);

    EXPECT_EQ(table.find(2), &b);
    EXPECT_EQ(table.find(1), nullptr);
    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, Find_StaleIdAfterPageRecycled_ReturnsNull) {
    SmallTable table;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(5); // same slot as 1, in the recycled page
    table.insert(1, &a);
    table.erase(1);
    table.insert(5, &b);

    EXPECT_EQ(table.find(1), nullptr);
}

TEST(OrderTableTest, Insert_AfterAPageThatDidNotEmpty_AllocatesAnotherPage) {
    SmallTable table;
    Nodes nodes = makeNodes({1, 2, 5});
    table.insert(1, &nodes[0]);
    table.insert(2, &nodes[1]);
    table.erase(1);

    table.insert(5, &nodes[2]);

    EXPECT_EQ(table.pageCount(), 2u);
    EXPECT_EQ(table.find(2), &nodes[1]);
    EXPECT_EQ(table.find(5), &nodes[2]);
}

TEST(OrderTableTest, Insert_WithSeveralSparePages_ReusesThemBeforeAllocating) {
    SmallTable table;
    Nodes filled = makeNodes({4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
    for (OrderNode& node : filled)
        table.insert(node.order.getID(), &node);
    for (OrderNode& node : filled)
        table.erase(node.order.getID());
    ASSERT_EQ(table.pageCount(), 3u);
    Nodes later = makeNodes({40, 44, 48, 52});

    for (int i = 0; i < 3; ++i)
        table.insert(later[i].order.getID(), &later[i]);

    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(table.find(later[i].order.getID()), &later[i]);
    EXPECT_EQ(table.pageCount(), 3u);

    table.insert(52, &later[3]);

    EXPECT_EQ(table.find(52), &later[3]);
    EXPECT_EQ(table.pageCount(), 4u);
}

TEST(OrderTableTest, InsertAndErase_NewestPageEmptiesAThousandTimes_PageCountStaysOne) {
    SmallTable table;
    Ids ids;
    for (OrderID id = 1; id <= 1000; ++id)
        ids.push_back(id);
    Nodes nodes = makeNodes(ids);

    for (OrderNode& node : nodes) {
        table.insert(node.order.getID(), &node);
        table.erase(node.order.getID());
    }

    EXPECT_EQ(table.pageCount(), 1u);
}

TEST(OrderTableTest, InsertAndErase_AroundAnOldOrder_OnlyTheOldPageIsKept) {
    SmallTable table;
    Ids ids {1};
    for (OrderID id = 4; id <= 400; ++id)
        ids.push_back(id);
    Nodes nodes = makeNodes(ids);
    table.insert(1, &nodes[0]);

    for (std::size_t i = 1; i < nodes.size(); ++i) {
        table.insert(nodes[i].order.getID(), &nodes[i]);
        table.erase(nodes[i].order.getID());
    }

    EXPECT_EQ(table.pageCount(), 2u);
    EXPECT_EQ(table.find(1), &nodes[0]);
}

TEST(OrderTableTest, SlidingWindow_EightLiveOrders_PageCountStaysBounded) {
    constexpr OrderID WINDOW = 8;
    constexpr OrderID LAST = 1000;
    // Insert happens before erase, so up to 9 consecutive IDs are live.
    // 9 consecutive IDs span at most 3 pages of 4 (1 + 4 + 4), and a page is only
    // allocated when no spare is left, so the total never exceeds 3.
    constexpr std::size_t MAX_PAGES = 3;
    SmallTable table;
    Ids ids;
    for (OrderID id = 1; id <= LAST; ++id)
        ids.push_back(id);
    Nodes nodes = makeNodes(ids);
    std::size_t highest = 0;

    for (OrderID id = 1; id <= LAST; ++id) {
        table.insert(id, &nodes[id - 1]);
        if (id > WINDOW) table.erase(id - WINDOW);
        highest = std::max(highest, table.pageCount());
    }

    EXPECT_LE(highest, MAX_PAGES);
    for (OrderID id = 1; id <= LAST - WINDOW; ++id)
        EXPECT_EQ(table.find(id), nullptr) << "id " << id;
    for (OrderID id = LAST - WINDOW + 1; id <= LAST; ++id)
        EXPECT_EQ(table.find(id), &nodes[id - 1]) << "id " << id;
}

// --------------------------
// Randomised
// --------------------------
TEST(OrderTableTest, RandomInsertsAndErases_AgreeWithAReferenceMap) {
    constexpr std::size_t COUNT = 5000;
    std::mt19937 rng(12345);
    Ids ids;
    OrderID next = 1;
    for (std::size_t i = 0; i < COUNT; ++i) {
        ids.push_back(next);
        next += 1 + rng() % 3; // skipped IDs, as IOCs leave
    }
    Nodes nodes = makeNodes(ids);
    SmallTable table;
    std::unordered_map<OrderID, OrderNode*> reference;
    Ids live;

    for (std::size_t i = 0; i < COUNT; ++i) {
        table.insert(ids[i], &nodes[i]);
        reference[ids[i]] = &nodes[i];
        live.push_back(ids[i]);

        if (rng() % 2 == 0) {
            std::size_t pick = rng() % live.size();
            OrderID victim = live[pick];
            live[pick] = live.back();
            live.pop_back();
            table.erase(victim);
            reference.erase(victim);
        }
    }

    for (OrderID id = 0; id <= ids.back() + 10; ++id) {
        auto it = reference.find(id);
        OrderNode* expected = it == reference.end() ? nullptr : it->second;
        EXPECT_EQ(table.find(id), expected) << "id " << id;
    }

    for (OrderID id : live)
        table.erase(id);

    for (OrderID id = 0; id <= ids.back() + 10; ++id)
        EXPECT_EQ(table.find(id), nullptr) << "id " << id;
}

// --------------------------
// Copy and move
// --------------------------
TEST(OrderTableTest, Traits_CopyIsDeletedAndMoveIsAllowed) {
    EXPECT_FALSE(std::is_copy_constructible_v<SmallTable>);
    EXPECT_FALSE(std::is_copy_assignable_v<SmallTable>);
    EXPECT_TRUE(std::is_move_constructible_v<SmallTable>);
    EXPECT_TRUE(std::is_move_assignable_v<SmallTable>);
}

TEST(OrderTableTest, MoveConstruct_NewTableFindsTheSamePointers) {
    SmallTable table;
    OrderNode a = makeNode(1);
    OrderNode b = makeNode(9);
    table.insert(1, &a);
    table.insert(9, &b);

    SmallTable moved(std::move(table));

    EXPECT_EQ(moved.find(1), &a);
    EXPECT_EQ(moved.find(9), &b);
    EXPECT_EQ(moved.pageCount(), 2u);
}

TEST(OrderTableTest, MoveConstruct_NewTableCanEraseAndReuseASparePage) {
    SmallTable table;
    Nodes nodes = makeNodes({1, 5, 9});
    table.insert(1, &nodes[0]);
    table.insert(5, &nodes[1]);
    table.erase(5); // leaves a spare page before the move
    SmallTable moved(std::move(table));

    moved.insert(9, &nodes[2]);

    EXPECT_EQ(moved.pageCount(), 2u);
    EXPECT_EQ(moved.find(9), &nodes[2]);
    moved.erase(1);
    EXPECT_EQ(moved.find(1), nullptr);
    EXPECT_EQ(moved.find(9), &nodes[2]);
}

TEST(OrderTableTest, MovedFrom_IsEmptyAndUsableAgain) {
    SmallTable table;
    Nodes nodes = makeNodes({1, 2});
    table.insert(1, &nodes[0]);
    SmallTable moved(std::move(table));

    EXPECT_EQ(table.find(1), nullptr);
    EXPECT_EQ(table.pageCount(), 0u);

    table.insert(2, &nodes[1]);

    EXPECT_EQ(table.find(2), &nodes[1]);
    EXPECT_EQ(table.pageCount(), 1u);
    EXPECT_EQ(moved.find(1), &nodes[0]);
    EXPECT_EQ(moved.find(2), nullptr);
}

TEST(OrderTableTest, MoveAssign_OntoANonEmptyTable_OldEntriesAreGone) {
    SmallTable target;
    SmallTable source;
    Nodes nodes = makeNodes({1, 5, 9});
    target.insert(1, &nodes[0]);
    target.insert(5, &nodes[1]);
    source.insert(9, &nodes[2]);

    target = std::move(source);

    EXPECT_EQ(target.find(1), nullptr);
    EXPECT_EQ(target.find(5), nullptr);
    EXPECT_EQ(target.find(9), &nodes[2]);
    EXPECT_EQ(target.pageCount(), 1u);
}

// --------------------------
// Default page size
// --------------------------
TEST(OrderTableTest, DefaultPageSize_IdsAroundTheBoundaryLandInDifferentPages) {
    OrderTable<> table;
    OrderNode a = makeNode(4095);
    OrderNode b = makeNode(4096);

    table.insert(4095, &a);
    table.insert(4096, &b);

    EXPECT_EQ(table.find(4095), &a);
    EXPECT_EQ(table.find(4096), &b);
    EXPECT_EQ(table.pageCount(), 2u);
}
