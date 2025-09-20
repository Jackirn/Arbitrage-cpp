cmake_minimum_required(VERSION 3.16)
project(Arbitrage_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Boost ===
# Se usi solo boost::math (header-only) non serve find_package
find_package(Boost REQUIRED)

# === NLopt ===
find_path(NLOPT_INCLUDE_DIR nlopt.h
        HINTS /opt/homebrew/include /opt/homebrew/opt/nlopt/include)

find_library(NLOPT_LIBRARY nlopt
        HINTS /opt/homebrew/lib /opt/homebrew/opt/nlopt/lib)

if(NLOPT_INCLUDE_DIR AND NLOPT_LIBRARY)
    message(STATUS "Found NLopt: ${NLOPT_LIBRARY}")
    include_directories(${NLOPT_INCLUDE_DIR})
else()
    message(FATAL_ERROR "NLopt not found")
endif()

# === Executable ===
add_executable(Arbitrage_cpp
        src/main.cpp
        src/DataOrdering.cpp
        src/Loaders.cpp
        src/OptimalBands.cpp
        src/StatisticalBootstrap.cpp
)

# === Includes ===
target_include_directories(Arbitrage_cpp
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# === Link ===
target_link_libraries(Arbitrage_cpp
        PRIVATE
        Boost::boost
        ${NLOPT_LIBRARY}
)