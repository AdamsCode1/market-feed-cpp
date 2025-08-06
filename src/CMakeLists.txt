# MIT License
# Copyright (c) 2025 Market Feed Project

# Core library
add_library(market_feed_core STATIC
    core/clock.cpp
)

target_include_directories(market_feed_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Feed library
add_library(market_feed_feed STATIC
    feed/decoder.cpp
)

target_include_directories(market_feed_feed PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_feed_feed 
    market_feed_core
)

# Book library
add_library(market_feed_book STATIC
    book/order_book.cpp
)

target_include_directories(market_feed_book PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_feed_book 
    market_feed_core
)

# Publisher library
add_library(market_feed_publish STATIC
    publish/publisher.cpp
)

target_include_directories(market_feed_publish PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_feed_publish 
    market_feed_core
    market_feed_book
)

# Main executable
add_executable(market-feed main.cpp)

target_link_libraries(market-feed
    market_feed_core
    market_feed_feed
    market_feed_book
    market_feed_publish
    Threads::Threads
)
