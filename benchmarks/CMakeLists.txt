add_executable(orderbook_benchmark
    workload.cpp
    scenarios/addorder.cpp
    scenarios/cancelorder.cpp
    scenarios/modifyorder.cpp
)

target_link_libraries(orderbook_benchmark
    PRIVATE
    orderbook_lib
    benchmark::benchmark_main
)

target_include_directories(orderbook_benchmark
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
