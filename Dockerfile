# MIT License
# Copyright (c) 2025 Market Feed Project

FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang-15 \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set clang as default compiler
ENV CC=clang-15
ENV CXX=clang++-15

# Copy source code
WORKDIR /app
COPY . .

# Build project
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j

# Run tests
RUN ctest --test-dir build --output-on-failure

# Generate sample data
RUN mkdir -p data
RUN ./build/tools/simgen --messages 1000000 --symbols AAPL,MSFT --output data/sim.bin

# Default command: run benchmarks
CMD ["./build/bench/market_feed_benchmarks"]
