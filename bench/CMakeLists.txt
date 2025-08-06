# MIT License
# Copyright (c) 2025 Market Feed Project

add_executable(market_feed_benchmarks
    benchmark_feed_processing.cpp
    benchmark_order_book.cpp
    benchmark_ring_buffer.cpp
)

target_include_directories(market_feed_benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_feed_benchmarks
    market_feed_core
    market_feed_feed
    market_feed_book
    market_feed_publish
    benchmark::benchmark
    benchmark::benchmark_main
    Threads::Threads
)
