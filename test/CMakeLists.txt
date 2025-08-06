# MIT License
# Copyright (c) 2025 Market Feed Project

add_executable(market_feed_tests
    test_messages.cpp
    test_ring_buffer.cpp
    test_order_book.cpp
    test_decoder.cpp
    test_integration.cpp
)

target_include_directories(market_feed_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(market_feed_tests
    market_feed_core
    market_feed_feed
    market_feed_book
    market_feed_publish
    gtest
    gtest_main
    gmock
    Threads::Threads
)

# Add test
add_test(NAME market_feed_unit_tests COMMAND market_feed_tests)
