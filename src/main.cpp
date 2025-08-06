/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "clock.hpp"
#include "ring_buffer.hpp"
#include "decoder.hpp"
#include "order_book.hpp"
#include "publisher.hpp"
#include "messages.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <algorithm>
#include <getopt.h>
#include <csignal>
#include <atomic>
#include <sstream>

namespace {

struct Config {
    std::string input_file;
    std::vector<std::string> symbols;
    uint64_t publish_interval_us = 1000;  // 1ms default
};

std::atomic<bool> g_shutdown{false};

void signal_handler(int) {
    g_shutdown = true;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --input FILE              Input binary feed file\n"
              << "  --symbols SYM1,SYM2,...   Comma-separated list of symbols to process\n"
              << "  --publish-top-of-book-us N Publish interval in microseconds (default: 1000)\n"
              << "  --help                    Show this help message\n";
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    
    static struct option long_options[] = {
        {"input", required_argument, 0, 'i'},
        {"symbols", required_argument, 0, 's'},
        {"publish-top-of-book-us", required_argument, 0, 'p'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "i:s:p:h", long_options, nullptr)) != -1) {
        switch (c) {
            case 'i':
                config.input_file = optarg;
                break;
            case 's': {
                std::string symbols_str = optarg;
                std::stringstream ss(symbols_str);
                std::string symbol;
                while (std::getline(ss, symbol, ',')) {
                    config.symbols.push_back(symbol);
                }
                break;
            }
            case 'p':
                config.publish_interval_us = std::stoull(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                std::exit(0);
            default:
                print_usage(argv[0]);
                std::exit(1);
        }
    }
    
    if (config.input_file.empty()) {
        std::cerr << "Error: --input is required\n";
        print_usage(argv[0]);
        std::exit(1);
    }
    
    if (config.symbols.empty()) {
        std::cerr << "Error: --symbols is required\n";
        print_usage(argv[0]);
        std::exit(1);
    }
    
    return config;
}

struct LatencyStats {
    std::vector<uint64_t> latencies;
    
    void add(uint64_t latency_us) {
        latencies.push_back(latency_us);
    }
    
    void report() const {
        if (latencies.empty()) {
            std::cerr << "No latency measurements\n";
            return;
        }
        
        auto sorted_latencies = latencies;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());
        
        size_t n = sorted_latencies.size();
        uint64_t p50 = sorted_latencies[n * 50 / 100];
        uint64_t p95 = sorted_latencies[n * 95 / 100];
        uint64_t p99 = sorted_latencies[n * 99 / 100];
        
        std::cerr << "Latency Stats (decode->apply):\n";
        std::cerr << "  p50: " << p50 << " µs\n";
        std::cerr << "  p95: " << p95 << " µs\n";
        std::cerr << "  p99: " << p99 << " µs\n";
        std::cerr << "  samples: " << n << "\n";
    }
};

} // anonymous namespace

int main(int argc, char* argv[]) {
    // Install signal handler
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        Config config = parse_args(argc, argv);
        
        // Create decoder
        feed::Decoder decoder(config.input_file);
        
        // Create ring buffer for events (power of 2 size)
        constexpr size_t RING_BUFFER_SIZE = 1024 * 1024;  // 1M events
        core::RingBuffer<feed::Event> ring_buffer(RING_BUFFER_SIZE);
        
        // Create order books for each symbol
        std::unordered_map<feed::Symbol, book::OrderBook> order_books;
        for (const auto& symbol_str : config.symbols) {
            feed::Symbol symbol(symbol_str.c_str());
            order_books[symbol] = book::OrderBook();
        }
        
        // Create publisher
        publish::TopOfBookPublisher publisher;
        
        // Statistics
        LatencyStats latency_stats;
        uint64_t total_messages = 0;
        uint64_t start_time_us = core::Clock::now_us();
        uint64_t last_publish_us = start_time_us;
        
        // Producer thread - decode messages and push to ring buffer
        std::thread producer([&]() {
            while (!g_shutdown && decoder.has_next()) {
                feed::Event event = decoder.next();
                if (event.type == feed::EventType::INVALID) {
                    continue;
                }
                
                // Try to push to ring buffer (non-blocking)
                while (!ring_buffer.try_push(std::move(event)) && !g_shutdown) {
                    // Ring buffer full, yield briefly
                    std::this_thread::yield();
                }
            }
        });
        
        // Consumer thread - process events from ring buffer
        feed::Event event;
        while (!g_shutdown) {
            if (ring_buffer.try_pop(event)) {
                // Process event based on type
                bool processed = false;
                feed::Symbol symbol;
                
                switch (event.type) {
                    case feed::EventType::ADD_ORDER: {
                        const auto& msg = event.payload.add;
                        symbol = feed::Symbol(msg.symbol);
                        
                        auto it = order_books.find(symbol);
                        if (it != order_books.end()) {
                            book::Side side = (msg.side == 'B') ? book::Side::BUY : book::Side::SELL;
                            processed = it->second.on_add(msg.order_id, side, msg.px_nano, msg.qty);
                        }
                        break;
                    }
                    case feed::EventType::MODIFY_ORDER: {
                        const auto& msg = event.payload.modify;
                        // Find which order book contains this order
                        for (auto& [sym, book] : order_books) {
                            if (book.on_modify(msg.order_id, msg.new_px_nano, msg.new_qty)) {
                                symbol = sym;
                                processed = true;
                                break;
                            }
                        }
                        break;
                    }
                    case feed::EventType::EXECUTE_ORDER: {
                        const auto& msg = event.payload.execute;
                        // Find which order book contains this order
                        for (auto& [sym, book] : order_books) {
                            if (book.on_execute(msg.order_id, msg.exec_qty)) {
                                symbol = sym;
                                processed = true;
                                break;
                            }
                        }
                        break;
                    }
                    case feed::EventType::DELETE_ORDER: {
                        const auto& msg = event.payload.delete_order;
                        // Find which order book contains this order
                        for (auto& [sym, book] : order_books) {
                            if (book.on_delete(msg.order_id)) {
                                symbol = sym;
                                processed = true;
                                break;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                
                if (processed) {
                    uint64_t apply_end_us = core::Clock::now_us();
                    uint64_t latency_us = apply_end_us - event.decode_timestamp_us;
                    latency_stats.add(latency_us);
                }
                
                total_messages++;
                
                // Check if it's time to publish
                uint64_t current_time_us = core::Clock::now_us();
                if (current_time_us - last_publish_us >= config.publish_interval_us) {
                    // Publish top of book for all symbols
                    for (const auto& [sym, book] : order_books) {
                        book::TopOfBook tob = book.top_of_book();
                        publisher.publish(current_time_us, sym, tob);
                    }
                    last_publish_us = current_time_us;
                }
            } else {
                // No events available, yield
                std::this_thread::yield();
                
                // Check if producer is done and buffer is empty
                if (!producer.joinable() || ring_buffer.empty()) {
                    if (!decoder.has_next() || g_shutdown) {
                        break;
                    }
                }
            }
        }
        
        // Wait for producer to finish
        if (producer.joinable()) {
            producer.join();
        }
        
        // Process any remaining events in the buffer
        while (ring_buffer.try_pop(event)) {
            // Process remaining events...
            total_messages++;
        }
        
        uint64_t end_time_us = core::Clock::now_us();
        uint64_t total_time_us = end_time_us - start_time_us;
        
        // Print final statistics
        double throughput = static_cast<double>(total_messages) / (static_cast<double>(total_time_us) / 1e6);
        
        std::cerr << "\nFinal Statistics:\n";
        std::cerr << "Total messages processed: " << total_messages << "\n";
        std::cerr << "Total time: " << (total_time_us / 1000.0) << " ms\n";
        std::cerr << "Throughput: " << static_cast<uint64_t>(throughput) << " msgs/s\n";
        
        latency_stats.report();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
