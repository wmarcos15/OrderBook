#include <gtest/gtest.h>
#include <string>
#include "trade.hpp"
#include "types.hpp"

using namespace orderbook;

constexpr Trade FIRST_TRADE  {.bidID = 1, .askID = 2, .price = 100, .quantity = 30, .bidOwner = 1, .askOwner = 2};
constexpr Trade SECOND_TRADE {.bidID = 7, .askID = 5, .price = 99, .quantity = 4, .bidOwner = 1, .askOwner = 2};

// --------------------------
// printTrade
// --------------------------
TEST(TradePrintTest, PrintTrade_OneTrade_PrintsTheExactLineWithoutNewline) {
    testing::internal::CaptureStdout();
    printTrade(FIRST_TRADE);

    EXPECT_EQ(testing::internal::GetCapturedStdout(), "[✓] MATCH | BUY #1 vs SELL #2 @ €100 | 30 units traded");
}

// --------------------------
// printTrades
// --------------------------
TEST(TradePrintTest, PrintTrades_Empty_PrintsNothing) {
    testing::internal::CaptureStdout();
    printTrades({});

    EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
}

TEST(TradePrintTest, PrintTrades_OneTrade_PrintsOneLine) {
    testing::internal::CaptureStdout();
    printTrades({FIRST_TRADE});

    EXPECT_EQ(testing::internal::GetCapturedStdout(), "[✓] MATCH | BUY #1 vs SELL #2 @ €100 | 30 units traded\n");
}

TEST(TradePrintTest, PrintTrades_TwoTrades_PrintsOneLineEach) {
    testing::internal::CaptureStdout();
    printTrades({FIRST_TRADE, SECOND_TRADE});

    EXPECT_EQ(testing::internal::GetCapturedStdout(),
        "[✓] MATCH | BUY #1 vs SELL #2 @ €100 | 30 units traded\n"
        "[✓] MATCH | BUY #7 vs SELL #5 @ €99 | 4 units traded\n");
}
