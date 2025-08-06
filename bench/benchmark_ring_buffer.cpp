/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "ring_buffer.hpp"
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>

static void BM_RingBufferSingleThreaded(benchmark::State& state) {
    const size_t buffer_size = state.range(0);
    core::RingBuffer<int> buffer(buffer_size);
    
    for (auto _ : state) {
        // Fill buffer
        for (size_t i = 0; i < buffer_size - 1; ++i) {
            bool success = buffer.try_push(static_cast<int>(i));
            benchmark::DoNotOptimize(success);
        }
        
        // Empty buffer
        int value;
        for (size_t i = 0; i < buffer_size - 1; ++i) {
            bool success = buffer.try_pop(value);
            benchmark::DoNotOptimize(success);
            benchmark::DoNotOptimize(value);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * (buffer_size - 1) * 2);
}

static void BM_RingBufferSPSC(benchmark::State& state) {
    const size_t num_items = state.range(0);
    const size_t buffer_size = 1024;
    
    for (auto _ : state) {
        core::RingBuffer<int> buffer(buffer_size);
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> items_consumed{0};
        
        // Producer thread
        std::thread producer([&]() {
            for (size_t i = 0; i < num_items; ++i) {
                while (!buffer.try_push(static_cast<int>(i))) {
                    std::this_thread::yield();
                }
            }
            producer_done = true;
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            int value;
            size_t consumed = 0;
            while (!producer_done || !buffer.empty()) {
                if (buffer.try_pop(value)) {
                    consumed++;
                    benchmark::DoNotOptimize(value);
                } else {
                    std::this_thread::yield();
                }
            }
            items_consumed = consumed;
        });
        
        producer.join();
        consumer.join();
        
        benchmark::DoNotOptimize(items_consumed.load());
    }
    
    state.SetItemsProcessed(state.iterations() * num_items);
}

static void BM_RingBufferContention(benchmark::State& state) {
    const size_t buffer_size = 1024;
    const size_t items_per_iteration = 10000;
    
    for (auto _ : state) {
        core::RingBuffer<int> buffer(buffer_size);
        std::atomic<bool> producer_done{false};
        std::atomic<size_t> total_produced{0};
        std::atomic<size_t> total_consumed{0};
        
        // Multiple producer threads (simulating contention)
        std::thread producer1([&]() {
            size_t produced = 0;
            for (size_t i = 0; i < items_per_iteration / 2; ++i) {
                while (!buffer.try_push(static_cast<int>(i))) {
                    std::this_thread::yield();
                }
                produced++;
            }
            total_produced += produced;
        });
        
        std::thread producer2([&]() {
            size_t produced = 0;
            for (size_t i = items_per_iteration / 2; i < items_per_iteration; ++i) {
                while (!buffer.try_push(static_cast<int>(i))) {
                    std::this_thread::yield();
                }
                produced++;
            }
            total_produced += produced;
            producer_done = true;
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            int value;
            while (total_consumed < items_per_iteration) {
                if (buffer.try_pop(value)) {
                    total_consumed++;
                    benchmark::DoNotOptimize(value);
                } else {
                    std::this_thread::yield();
                }
            }
        });
        
        producer1.join();
        producer2.join();
        consumer.join();
        
        benchmark::DoNotOptimize(total_produced.load());
        benchmark::DoNotOptimize(total_consumed.load());
    }
    
    state.SetItemsProcessed(state.iterations() * items_per_iteration);
}

static void BM_RingBufferThroughput(benchmark::State& state) {
    const size_t buffer_size = 1024 * 1024; // Large buffer
    const size_t num_items = 1000000; // 1M items
    
    for (auto _ : state) {
        core::RingBuffer<uint64_t> buffer(buffer_size);
        std::atomic<bool> producer_done{false};
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Producer thread
        std::thread producer([&]() {
            for (size_t i = 0; i < num_items; ++i) {
                while (!buffer.try_push(i)) {
                    // Busy wait for maximum throughput
                }
            }
            producer_done = true;
        });
        
        // Consumer thread
        std::thread consumer([&]() {
            uint64_t value;
            size_t consumed = 0;
            while (consumed < num_items) {
                if (buffer.try_pop(value)) {
                    consumed++;
                    benchmark::DoNotOptimize(value);
                }
            }
        });
        
        producer.join();
        consumer.join();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(num_items) / (static_cast<double>(duration.count()) / 1e6);
        state.counters["Throughput"] = benchmark::Counter(throughput, benchmark::Counter::kIsRate);
    }
    
    state.SetItemsProcessed(state.iterations() * num_items);
}

// Register benchmarks
BENCHMARK(BM_RingBufferSingleThreaded)->Range(64, 1024*1024)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_RingBufferSPSC)->Range(1000, 1000000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_RingBufferContention)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_RingBufferThroughput)->Unit(benchmark::kSecond)->Iterations(3);
