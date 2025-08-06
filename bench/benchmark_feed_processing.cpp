/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "decoder.hpp"
#include "order_book.hpp"
#include "ring_buffer.hpp"
#include "messages.hpp"
#include "clock.hpp"
#include <benchmark/benchmark.h>
#include <fstream>
#include <vector>
#include <random>
#include <cstdio>
#include <unistd.h>

namespace {

// Create a temporary test file with synthetic data
std::string create_test_feed(size_t num_messages) {
    static std::string temp_filename = "bench_feed_XXXXXX";
    static bool created = false;
    
    if (!created) {
        int fd = mkstemp(&temp_filename[0]);
        if (fd == -1) {
            throw std::runtime_error("Cannot create temp file");
        }
        close(fd);
        
        std::ofstream file(temp_filename, std::ios::binary);
        std::mt19937 rng(42); // Fixed seed for reproducibility
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        uint64_t order_id = 1;
        std::vector<uint64_t> active_orders;
        
        for (size_t i = 0; i < num_messages; ++i) {
            uint64_t timestamp = i * 1000; // 1µs per message
            
            double msg_type = dist(rng);
            
            if (active_orders.empty() || msg_type < 0.4) {
                // Add order
                feed::AddOrderMsg msg;
                msg.type = 'A';
                msg.ts_us = timestamp;
                msg.order_id = order_id++;
                std::memcpy(msg.symbol, "AAPL  ", 6);
                msg.side = (dist(rng) < 0.5) ? 'B' : 'S';
                msg.px_nano = 100000000000LL + static_cast<int64_t>((dist(rng) - 0.5) * 5000000000LL);
                msg.qty = 100 + static_cast<uint32_t>(dist(rng) * 900);
                
                file.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
                active_orders.push_back(msg.order_id);
                
            } else if (msg_type < 0.6) {
                // Modify order
                if (!active_orders.empty()) {
                    size_t idx = static_cast<size_t>(dist(rng) * active_orders.size());
                    
                    feed::ModifyOrderMsg msg;
                    msg.type = 'U';
                    msg.ts_us = timestamp;
                    msg.order_id = active_orders[idx];
                    msg.new_px_nano = 100000000000LL + static_cast<int64_t>((dist(rng) - 0.5) * 5000000000LL);
                    msg.new_qty = 50 + static_cast<uint32_t>(dist(rng) * 450);
                    
                    file.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
                }
                
            } else if (msg_type < 0.8) {
                // Execute order
                if (!active_orders.empty()) {
                    size_t idx = static_cast<size_t>(dist(rng) * active_orders.size());
                    
                    feed::ExecuteOrderMsg msg;
                    msg.type = 'E';
                    msg.ts_us = timestamp;
                    msg.order_id = active_orders[idx];
                    msg.exec_qty = 10 + static_cast<uint32_t>(dist(rng) * 90);
                    
                    file.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
                    
                    // Sometimes remove fully executed orders
                    if (dist(rng) < 0.3) {
                        active_orders.erase(active_orders.begin() + idx);
                    }
                }
                
            } else {
                // Delete order
                if (!active_orders.empty()) {
                    size_t idx = static_cast<size_t>(dist(rng) * active_orders.size());
                    
                    feed::DeleteOrderMsg msg;
                    msg.type = 'D';
                    msg.ts_us = timestamp;
                    msg.order_id = active_orders[idx];
                    
                    file.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
                    active_orders.erase(active_orders.begin() + idx);
                }
            }
        }
        
        file.close();
        created = true;
    }
    
    return temp_filename;
}

} // anonymous namespace

static void BM_DecodeMessages(benchmark::State& state) {
    const size_t num_messages = state.range(0);
    std::string filename = create_test_feed(num_messages);
    
    for (auto _ : state) {
        feed::Decoder decoder(filename);
        size_t decoded_count = 0;
        
        while (decoder.has_next()) {
            feed::Event event = decoder.next();
            if (event.type != feed::EventType::INVALID) {
                decoded_count++;
            }
            benchmark::DoNotOptimize(event);
        }
        
        benchmark::DoNotOptimize(decoded_count);
    }
    
    state.SetItemsProcessed(state.iterations() * num_messages);
    state.SetBytesProcessed(state.iterations() * num_messages * 30); // Avg message size
}

static void BM_FullPipelineProcessing(benchmark::State& state) {
    const size_t num_messages = state.range(0);
    std::string filename = create_test_feed(num_messages);
    
    for (auto _ : state) {
        feed::Decoder decoder(filename);
        core::RingBuffer<feed::Event> ring_buffer(1024 * 1024);
        book::OrderBook order_book;
        
        std::vector<uint64_t> latencies;
        latencies.reserve(num_messages);
        
        // Decode phase
        while (decoder.has_next()) {
            feed::Event event = decoder.next();
            if (event.type != feed::EventType::INVALID) {
                while (!ring_buffer.try_push(event)) {
                    // Would yield in real implementation
                }
            }
        }
        
        // Process phase
        feed::Event event;
        while (ring_buffer.try_pop(event)) {
            switch (event.type) {
                case feed::EventType::ADD_ORDER: {
                    const auto& msg = event.payload.add;
                    book::Side side = (msg.side == 'B') ? book::Side::BUY : book::Side::SELL;
                    order_book.on_add(msg.order_id, side, msg.px_nano, msg.qty);
                    break;
                }
                case feed::EventType::MODIFY_ORDER: {
                    const auto& msg = event.payload.modify;
                    order_book.on_modify(msg.order_id, msg.new_px_nano, msg.new_qty);
                    break;
                }
                case feed::EventType::EXECUTE_ORDER: {
                    const auto& msg = event.payload.execute;
                    order_book.on_execute(msg.order_id, msg.exec_qty);
                    break;
                }
                case feed::EventType::DELETE_ORDER: {
                    const auto& msg = event.payload.delete_order;
                    order_book.on_delete(msg.order_id);
                    break;
                }
                default:
                    break;
            }
            
            uint64_t end_time = core::Clock::now_us();
            uint64_t latency = end_time - event.decode_timestamp_us;
            latencies.push_back(latency);
            
            benchmark::DoNotOptimize(latency);
        }
        
        // Calculate latency percentiles
        if (!latencies.empty()) {
            std::sort(latencies.begin(), latencies.end());
            size_t n = latencies.size();
            uint64_t p99 = latencies[n * 99 / 100];
            benchmark::DoNotOptimize(p99);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * num_messages);
}

static void BM_ThroughputTest(benchmark::State& state) {
    const size_t num_messages = 10000000; // 10M messages
    std::string filename = create_test_feed(num_messages);
    
    for (auto _ : state) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        feed::Decoder decoder(filename);
        book::OrderBook order_book;
        size_t processed = 0;
        
        while (decoder.has_next()) {
            feed::Event event = decoder.next();
            if (event.type != feed::EventType::INVALID) {
                // Minimal processing to measure max throughput
                switch (event.type) {
                    case feed::EventType::ADD_ORDER: {
                        const auto& msg = event.payload.add;
                        book::Side side = (msg.side == 'B') ? book::Side::BUY : book::Side::SELL;
                        order_book.on_add(msg.order_id, side, msg.px_nano, msg.qty);
                        break;
                    }
                    case feed::EventType::MODIFY_ORDER: {
                        const auto& msg = event.payload.modify;
                        order_book.on_modify(msg.order_id, msg.new_px_nano, msg.new_qty);
                        break;
                    }
                    case feed::EventType::EXECUTE_ORDER: {
                        const auto& msg = event.payload.execute;
                        order_book.on_execute(msg.order_id, msg.exec_qty);
                        break;
                    }
                    case feed::EventType::DELETE_ORDER: {
                        const auto& msg = event.payload.delete_order;
                        order_book.on_delete(msg.order_id);
                        break;
                    }
                    default:
                        break;
                }
                processed++;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        double throughput = static_cast<double>(processed) / (static_cast<double>(duration.count()) / 1e6);
        state.counters["Throughput"] = benchmark::Counter(throughput, benchmark::Counter::kIsRate);
        state.counters["ProcessedMessages"] = processed;
        
        // Verify we hit the target throughput
        if (throughput < 2000000) { // 2M msgs/s
            state.SkipWithError("Throughput too low");
        }
    }
    
    state.SetItemsProcessed(state.iterations() * num_messages);
}

// Register benchmarks
BENCHMARK(BM_DecodeMessages)->Range(1000, 1000000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FullPipelineProcessing)->Range(1000, 100000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_ThroughputTest)->Unit(benchmark::kSecond)->Iterations(1);
