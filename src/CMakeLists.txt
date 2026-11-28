add_library(orderbook_lib
    lib/trade.cpp
    lib/order.cpp
    lib/orderbook.cpp
)

target_include_directories(orderbook_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# PUBLIC so that whoever links the library compiles its headers as C++20 too
target_compile_features(orderbook_lib PUBLIC cxx_std_20)

# Warnings for our own code only: a project that links the library does not get them
option(ORDERBOOK_WARNINGS_AS_ERRORS "Treat the warnings of OrderBook's own code as errors" OFF)

if(PROJECT_IS_TOP_LEVEL)
    target_compile_options(orderbook_lib PRIVATE -Wall -Wextra -Wpedantic)

    if(ORDERBOOK_WARNINGS_AS_ERRORS)
        target_compile_options(orderbook_lib PRIVATE -Werror)
    endif()
endif()

# The usage example, only built when OrderBook is the top-level project
if(PROJECT_IS_TOP_LEVEL)
    add_executable(orderbook main.cpp)

    target_link_libraries(orderbook PRIVATE orderbook_lib)
endif()
