# MIT License
# Copyright (c) 2025 Market Feed Project

add_executable(simgen simgen.cpp)

target_include_directories(simgen PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(simgen
    market_feed_core
)
