add_executable(orderbook_benchmark
    benchmark_main.cpp
    workload.cpp
    harness.cpp
    scenarios/throughput.cpp
    scenarios/latency.cpp
)

target_link_libraries(orderbook_benchmark
    PRIVATE
    orderbook_lib
)
