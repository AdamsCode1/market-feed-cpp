# MIT License
# Copyright (c) 2025 Market Feed Project

.PHONY: build test bench run clean install

# Default target
all: build

# Build the project
build:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j

# Build debug version
debug:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
	cmake --build build -j

# Run tests
test: build
	ctest --test-dir build --output-on-failure

# Run benchmarks
bench: build
	./build/bench/market_feed_benchmarks

# Generate sample data and run feed handler
run: build
	mkdir -p data
	./build/tools/simgen --messages 1000000 --symbols AAPL,MSFT --output data/sim.bin
	./build/src/market-feed --input data/sim.bin --symbols AAPL,MSFT --publish-top-of-book-us 1000

# Clean build artifacts
clean:
	rm -rf build

# Install dependencies (Linux)
install-deps:
	sudo apt-get update
	sudo apt-get install -y build-essential cmake ninja-build clang-15 git

# Docker build and benchmark
docker-bench:
	docker build -t market-feed .
	docker run --rm market-feed
