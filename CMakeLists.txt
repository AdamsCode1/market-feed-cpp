# MIT License
# Copyright (c) 2025 Market Feed Project

cmake_minimum_required(VERSION 3.22)
project(market-feed VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address -fsanitize=undefined")

# Find required packages
find_package(Threads REQUIRED)

# FetchContent for external dependencies
include(FetchContent)

# GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Google Benchmark
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Include directories
include_directories(include)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(test)
add_subdirectory(bench)

# Enable testing
enable_testing()
