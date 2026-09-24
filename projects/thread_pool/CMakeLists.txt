find_package(Threads REQUIRED)

add_library(
    thread_pool_lib
    src/thread_pool.cpp
    src/worker_queue.cpp
)

target_include_directories(
    thread_pool_lib
    PUBLIC
    include/
)

target_link_libraries(
    thread_pool_lib
    PUBLIC
    Threads::Threads
)

add_executable(
    thread_pool_demo
    src/main.cpp
)

target_link_libraries(
    thread_pool_demo
    PRIVATE
    thread_pool_lib
)

add_executable(
    thread_pool_stress
    tests/stress_test.cpp
)

target_link_libraries(
    thread_pool_stress
    PRIVATE
    thread_pool_lib
)

add_executable(
    thread_pool_benchmark
    benchmarks/benchmark.cpp
)

target_link_libraries(
    thread_pool_benchmark
    PRIVATE
    thread_pool_lib
)