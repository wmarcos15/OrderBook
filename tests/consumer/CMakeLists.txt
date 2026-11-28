cmake_minimum_required(VERSION 3.25)
project(OrderBookConsumer LANGUAGES CXX)

include(FetchContent)

# The library comes from this checkout, so nothing is downloaded
FetchContent_Declare(
    orderbook
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

FetchContent_MakeAvailable(orderbook)

add_executable(consumer main.cpp)

target_link_libraries(consumer PRIVATE orderbook_lib)

# Its own warnings: the library sets none for it
target_compile_options(consumer PRIVATE -Wall -Wextra -Wpedantic -Werror)
