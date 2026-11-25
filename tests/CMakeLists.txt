add_executable(orderbook_tests
    orderbook_test.cpp
    matching_test.cpp
    pool_test.cpp
    order_queue_test.cpp
    order_table_test.cpp
    bitmap_test.cpp
    side_index_test.cpp
    ladder_test.cpp
    price_level_test.cpp
    moved_book_test.cpp
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
