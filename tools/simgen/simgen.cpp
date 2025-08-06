/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "messages.hpp"
#include "clock.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <getopt.h>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <chrono>

namespace {

struct Config {
    uint64_t num_messages = 1000000;
    std::vector<std::string> symbols;
    std::string output_file = "data/sim.bin";
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  --messages N              Number of messages to generate (default: 1000000)\n"
              << "  --symbols SYM1,SYM2,...   Comma-separated list of symbols (default: AAPL,MSFT)\n"
              << "  --output FILE             Output file path (default: data/sim.bin)\n"
              << "  --help                    Show this help message\n";
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    config.symbols = {"AAPL", "MSFT"}; // Default symbols
    
    static struct option long_options[] = {
        {"messages", required_argument, 0, 'm'},
        {"symbols", required_argument, 0, 's'},
        {"output", required_argument, 0, 'o'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "m:s:o:h", long_options, nullptr)) != -1) {
        switch (c) {
            case 'm':
                config.num_messages = std::stoull(optarg);
                break;
            case 's': {
                config.symbols.clear();
                std::string symbols_str = optarg;
                std::stringstream ss(symbols_str);
                std::string symbol;
                while (std::getline(ss, symbol, ',')) {
                    config.symbols.push_back(symbol);
                }
                break;
            }
            case 'o':
                config.output_file = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                std::exit(0);
            default:
                print_usage(argv[0]);
                std::exit(1);
        }
    }
    
    return config;
}

class FeedGenerator {
public:
    FeedGenerator(const std::vector<std::string>& symbols, std::mt19937& rng)
        : symbols_(symbols), rng_(rng), dist_(0.0, 1.0) {
        
        // Initialize order ID counter
        next_order_id_ = 1;
        
        // Initialize base prices for symbols (in nano-units)
        for (const auto& symbol : symbols_) {
            base_prices_[symbol] = 100'000'000'000LL; // $100.00
        }
    }
    
    void generate(std::ostream& output, uint64_t num_messages) {
        uint64_t start_time_us = core::Clock::now_us();
        uint64_t current_time_us = start_time_us;
        
        for (uint64_t i = 0; i < num_messages; ++i) {
            // Advance time by random small amount (0-10 µs)
            current_time_us += static_cast<uint64_t>(dist_(rng_) * 10);
            
            // Choose random symbol
            const std::string& symbol = symbols_[std::uniform_int_distribution<size_t>(0, symbols_.size() - 1)(rng_)];
            
            // Decide message type based on current state
            double msg_type_rand = dist_(rng_);
            
            if (active_orders_[symbol].empty() || msg_type_rand < 0.4) {
                // 40% chance to add order (or forced if no active orders)
                generate_add_order(output, current_time_us, symbol);
            } else if (msg_type_rand < 0.6) {
                // 20% chance to modify order
                generate_modify_order(output, current_time_us, symbol);
            } else if (msg_type_rand < 0.8) {
                // 20% chance to execute order
                generate_execute_order(output, current_time_us, symbol);
            } else {
                // 20% chance to delete order
                generate_delete_order(output, current_time_us, symbol);
            }
        }
    }

private:
    struct OrderInfo {
        uint64_t order_id;
        char side;
        int64_t price;
        uint32_t quantity;
    };
    
    std::vector<std::string> symbols_;
    std::mt19937& rng_;
    std::uniform_real_distribution<double> dist_;
    uint64_t next_order_id_;
    std::unordered_map<std::string, int64_t> base_prices_;
    std::unordered_map<std::string, std::vector<OrderInfo>> active_orders_;
    
    void generate_add_order(std::ostream& output, uint64_t timestamp_us, const std::string& symbol) {
        feed::AddOrderMsg msg;
        msg.type = 'A';
        msg.ts_us = timestamp_us;
        msg.order_id = next_order_id_++;
        
        // Copy symbol (space-padded)
        std::memset(msg.symbol, ' ', 6);
        size_t len = std::min(symbol.length(), size_t(6));
        std::memcpy(msg.symbol, symbol.c_str(), len);
        
        // Random side
        msg.side = (dist_(rng_) < 0.5) ? 'B' : 'S';
        
        // Price within ±5% of base price
        int64_t base_price = base_prices_[symbol];
        double price_factor = 0.95 + dist_(rng_) * 0.1; // 0.95 to 1.05
        msg.px_nano = static_cast<int64_t>(base_price * price_factor);
        
        // Random quantity (100 to 10000)
        msg.qty = 100 + static_cast<uint32_t>(dist_(rng_) * 9900);
        
        output.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
        
        // Track active order
        active_orders_[symbol].push_back({msg.order_id, msg.side, msg.px_nano, msg.qty});
    }
    
    void generate_modify_order(std::ostream& output, uint64_t timestamp_us, const std::string& symbol) {
        auto& orders = active_orders_[symbol];
        if (orders.empty()) return;
        
        // Pick random order
        size_t index = std::uniform_int_distribution<size_t>(0, orders.size() - 1)(rng_);
        OrderInfo& order = orders[index];
        
        feed::ModifyOrderMsg msg;
        msg.type = 'U';
        msg.ts_us = timestamp_us;
        msg.order_id = order.order_id;
        
        // Modify price slightly (±1%)
        double price_factor = 0.99 + dist_(rng_) * 0.02;
        msg.new_px_nano = static_cast<int64_t>(order.price * price_factor);
        
        // Modify quantity slightly
        double qty_factor = 0.5 + dist_(rng_) * 1.0; // 0.5 to 1.5
        msg.new_qty = std::max(1U, static_cast<uint32_t>(order.quantity * qty_factor));
        
        output.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
        
        // Update tracked order
        order.price = msg.new_px_nano;
        order.quantity = msg.new_qty;
    }
    
    void generate_execute_order(std::ostream& output, uint64_t timestamp_us, const std::string& symbol) {
        auto& orders = active_orders_[symbol];
        if (orders.empty()) return;
        
        // Pick random order
        size_t index = std::uniform_int_distribution<size_t>(0, orders.size() - 1)(rng_);
        OrderInfo& order = orders[index];
        
        feed::ExecuteOrderMsg msg;
        msg.type = 'E';
        msg.ts_us = timestamp_us;
        msg.order_id = order.order_id;
        
        // Execute 10% to 100% of remaining quantity
        uint32_t max_exec = order.quantity;
        msg.exec_qty = std::max(1U, static_cast<uint32_t>(max_exec * (0.1 + dist_(rng_) * 0.9)));
        msg.exec_qty = std::min(msg.exec_qty, max_exec);
        
        output.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
        
        // Update tracked order
        order.quantity -= msg.exec_qty;
        if (order.quantity == 0) {
            orders.erase(orders.begin() + index);
        }
    }
    
    void generate_delete_order(std::ostream& output, uint64_t timestamp_us, const std::string& symbol) {
        auto& orders = active_orders_[symbol];
        if (orders.empty()) return;
        
        // Pick random order
        size_t index = std::uniform_int_distribution<size_t>(0, orders.size() - 1)(rng_);
        const OrderInfo& order = orders[index];
        
        feed::DeleteOrderMsg msg;
        msg.type = 'D';
        msg.ts_us = timestamp_us;
        msg.order_id = order.order_id;
        
        output.write(reinterpret_cast<const char*>(&msg), sizeof(msg));
        
        // Remove tracked order
        orders.erase(orders.begin() + index);
    }
};

} // anonymous namespace

int main(int argc, char* argv[]) {
    try {
        Config config = parse_args(argc, argv);
        
        std::cout << "Generating " << config.num_messages << " messages for symbols: ";
        for (size_t i = 0; i < config.symbols.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << config.symbols[i];
        }
        std::cout << "\n";
        std::cout << "Output file: " << config.output_file << "\n";
        
        // Create output file
        std::ofstream output(config.output_file, std::ios::binary);
        if (!output) {
            std::cerr << "Error: Cannot create output file: " << config.output_file << "\n";
            return 1;
        }
        
        // Create random number generator
        std::random_device rd;
        std::mt19937 rng(rd());
        
        // Generate feed
        FeedGenerator generator(config.symbols, rng);
        
        auto start = std::chrono::steady_clock::now();
        generator.generate(output, config.num_messages);
        auto end = std::chrono::steady_clock::now();
        
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Generated " << config.num_messages << " messages in " 
                  << duration_ms.count() << " ms\n";
        std::cout << "File size: " << output.tellp() << " bytes\n";
        std::cout << "Generation rate: " 
                  << (config.num_messages * 1000) / duration_ms.count() << " msgs/s\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
