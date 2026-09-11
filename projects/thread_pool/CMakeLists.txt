cmake_minimum_required(VERSION 3.20)

project(thread_pool
    VERSION 0.1
    DESCRIPTION "A thread_pool project"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(thread_pool_demo src/main.cpp src/thread_pool.cpp)

target_include_directories(thread_pool_demo
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
