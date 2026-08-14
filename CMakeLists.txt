cmake_minimum_required(VERSION 3.20)

project(cpp-learning)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main src/main.cpp src/calculator.cpp)
