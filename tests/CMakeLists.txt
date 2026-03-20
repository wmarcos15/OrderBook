add_executable(orderbook_tests
    orderbook_test.cpp
)

target_link_libraries(orderbook_tests
    PRIVATE
    orderbook_lib
    GTest::gtest_main
)

target_include_directories(orderbook_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
)

include(GoogleTest)
gtest_discover_tests(orderbook_tests)
