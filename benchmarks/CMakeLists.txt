add_executable(orderbook_benchmark
    workload.cpp
    scenarios/addorder.cpp
    scenarios/cancelorder.cpp
    scenarios/matchorder.cpp
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

# Runs one operation in a loop so that a sampling profiler can be attached;
# profile/breakdown.py drives it.
add_executable(orderbook_profile
    workload.cpp
    profile/driver.cpp
)

target_link_libraries(orderbook_profile
    PRIVATE
    orderbook_lib
)

target_include_directories(orderbook_profile
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
