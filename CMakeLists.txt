cmake_minimum_required(VERSION 3.14)
project(OrderBook)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)


FetchContent_Declare(
        databento
        GIT_REPOSITORY https://github.com/databento/databento-cpp
        GIT_TAG v0.51.0
)
FetchContent_MakeAvailable(databento)

FetchContent_Declare(
    unordered_dense
    GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
    GIT_TAG v4.4.0
)
FetchContent_MakeAvailable(unordered_dense)



add_executable(OrderBook src/main.cpp
include/FeedHandler.h
src/FeedHandler.cpp
include/priceLevel.h
include/Metrics.h)

target_link_libraries(OrderBook PRIVATE databento::databento)
target_link_libraries(OrderBook PRIVATE unordered_dense::unordered_dense)
