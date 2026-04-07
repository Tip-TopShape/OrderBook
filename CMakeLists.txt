cmake_minimum_required(VERSION 3.31)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)


FetchContent_Declare(
        databento
        GIT_REPOSITORY https://github.com/databento/databento-cpp
        GIT_TAG main
)
FetchContent_MakeAvailable(databento)


add_executable(OrderBook src/main.cpp
        include/FeedHandler.h
        src/FeedHandler.cpp
        include/priceLevel.h
        include/MatchingMetrics.h)
target_link_libraries(OrderBook PRIVATE databento::databento)
