cmake_minimum_required(VERSION 3.16)
project(Arbitrage_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Boost (solo header) ---
find_package(Boost 1.70 REQUIRED)  # Boost::boost

# --- NLopt (Homebrew) ---
# Non usare find_package(NLopt) perché spesso non installa il Config file.
find_path(NLOPT_INCLUDE_DIR nlopt.h
        HINTS /opt/homebrew/include /opt/homebrew/opt/nlopt/include)
find_library(NLOPT_LIBRARY nlopt
        HINTS /opt/homebrew/lib /opt/homebrew/opt/nlopt/lib)

if(NLOPT_INCLUDE_DIR AND NLOPT_LIBRARY)
    message(STATUS "Found NLopt include: ${NLOPT_INCLUDE_DIR}")
    message(STATUS "Found NLopt library: ${NLOPT_LIBRARY}")
else()
    message(FATAL_ERROR "NLopt not found. Try: brew install nlopt")
endif()

add_executable(Arbitrage_cpp
        src/main.cpp
        src/DataOrdering.cpp
        src/Loaders.cpp
        src/OptimalBands.cpp
        include/utilities/StatisticalBootstrap.hpp
)

target_include_directories(Arbitrage_cpp
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${NLOPT_INCLUDE_DIR}
)

target_link_libraries(Arbitrage_cpp
        PRIVATE
        Boost::boost
        ${NLOPT_LIBRARY}     # <— usa il path assoluto trovato da find_library
)