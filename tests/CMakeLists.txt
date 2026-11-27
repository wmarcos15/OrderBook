add_executable(orderbook_tests
    bitmap_test.cpp
    side_index_test.cpp
    ladder_test.cpp
    window_test.cpp
    price_levels_test.cpp
    order_queue_test.cpp
    order_table_test.cpp
    pool_test.cpp
    book_config_test.cpp
    add_order_test.cpp
    cancel_order_test.cpp
    modify_order_test.cpp
    matching_test.cpp
    fill_or_kill_test.cpp
    market_order_test.cpp
    self_trade_test.cpp
    depth_test.cpp
    price_band_test.cpp
    moved_book_test.cpp
    random_test.cpp
    allocation_test.cpp
)

target_link_libraries(orderbook_tests
    PRIVATE
    orderbook_lib
    GTest::gtest_main
)

target_include_directories(orderbook_tests
    PRIVATE
)

include(GoogleTest)
gtest_discover_tests(orderbook_tests)
