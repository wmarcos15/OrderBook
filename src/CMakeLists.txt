add_library(orderbook_lib
    lib/trade.cpp
    lib/order.cpp
    lib/orderbook.cpp
)

target_include_directories(orderbook_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_executable(orderbook main.cpp)

target_link_libraries(orderbook PRIVATE orderbook_lib)
