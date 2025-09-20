cmake_minimum_required(VERSION 3.16)
project(ArbitrageCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Arbitrage_cpp
        src/main.cpp
        src/DataOrdering.cpp
        src/Loaders.cpp
        include/utilities/OptimalBands.hpp
)

target_include_directories(Arbitrage_cpp PRIVATE ${CMAKE_SOURCE_DIR}/include)