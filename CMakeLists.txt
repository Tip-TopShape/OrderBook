cmake_minimum_required(VERSION 3.31)
project(OrderBook)

set(CMAKE_CXX_STANDARD 14)


# start
add_executable(OrderBook src/main.cpp src/FeedHandler.cpp)
target_include_directories(OrderBook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# debug
#add_executable(FeedHandlerTest test/FeedHandlerTests.cpp src/FeedHandler.cpp)
#target_include_directories(FeedHandlerTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

#add_custom_command(TARGET OrderBook POST_BUILD
#        COMMAND codesign --force --sign - --entitlements ${CMAKE_SOURCE_DIR}/entitlements.plist $<TARGET_FILE:OrderBook>
#)
