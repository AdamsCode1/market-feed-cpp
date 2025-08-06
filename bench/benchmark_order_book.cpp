/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "order_book.hpp"
#include <benchmark/benchmark.h>
#include <random>

static void BM_OrderBookAdd(benchmark::State& state) {
    book::OrderBook order_book;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(100000000000LL, 105000000000LL);
    std::uniform_int_distribution<uint32_t> qty_dist(100, 1000);
    
    uint64_t order_id = 1;
    
    for (auto _ : state) {
        int64_t price = price_dist(rng);
        uint32_t qty = qty_dist(rng);
        book::Side side = (order_id % 2 == 0) ? book::Side::BUY : book::Side::SELL;
        
        bool result = order_book.on_add(order_id++, side, price, qty);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}

static void BM_OrderBookModify(benchmark::State& state) {
    book::OrderBook order_book;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(100000000000LL, 105000000000LL);
    std::uniform_int_distribution<uint32_t> qty_dist(100, 1000);
    
    // Pre-populate with orders
    const size_t num_orders = state.range(0);
    std::vector<uint64_t> order_ids;
    
    for (size_t i = 0; i < num_orders; ++i) {
        uint64_t order_id = i + 1;
        int64_t price = price_dist(rng);
        uint32_t qty = qty_dist(rng);
        book::Side side = (i % 2 == 0) ? book::Side::BUY : book::Side::SELL;
        
        if (order_book.on_add(order_id, side, price, qty)) {
            order_ids.push_back(order_id);
        }
    }
    
    std::uniform_int_distribution<size_t> order_dist(0, order_ids.size() - 1);
    
    for (auto _ : state) {
        if (!order_ids.empty()) {
            uint64_t order_id = order_ids[order_dist(rng)];
            int64_t new_price = price_dist(rng);
            uint32_t new_qty = qty_dist(rng);
            
            bool result = order_book.on_modify(order_id, new_price, new_qty);
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}

static void BM_OrderBookExecute(benchmark::State& state) {
    book::OrderBook order_book;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(100000000000LL, 105000000000LL);
    std::uniform_int_distribution<uint32_t> qty_dist(100, 1000);
    
    // Pre-populate with orders
    const size_t num_orders = state.range(0);
    std::vector<uint64_t> order_ids;
    
    for (size_t i = 0; i < num_orders; ++i) {
        uint64_t order_id = i + 1;
        int64_t price = price_dist(rng);
        uint32_t qty = qty_dist(rng);
        book::Side side = (i % 2 == 0) ? book::Side::BUY : book::Side::SELL;
        
        if (order_book.on_add(order_id, side, price, qty)) {
            order_ids.push_back(order_id);
        }
    }
    
    std::uniform_int_distribution<size_t> order_dist(0, order_ids.size() - 1);
    std::uniform_int_distribution<uint32_t> exec_dist(10, 50);
    
    for (auto _ : state) {
        if (!order_ids.empty()) {
            uint64_t order_id = order_ids[order_dist(rng)];
            uint32_t exec_qty = exec_dist(rng);
            
            bool result = order_book.on_execute(order_id, exec_qty);
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}

static void BM_OrderBookTopOfBook(benchmark::State& state) {
    book::OrderBook order_book;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(100000000000LL, 105000000000LL);
    std::uniform_int_distribution<uint32_t> qty_dist(100, 1000);
    
    // Pre-populate with orders
    const size_t num_orders = state.range(0);
    for (size_t i = 0; i < num_orders; ++i) {
        uint64_t order_id = i + 1;
        int64_t price = price_dist(rng);
        uint32_t qty = qty_dist(rng);
        book::Side side = (i % 2 == 0) ? book::Side::BUY : book::Side::SELL;
        order_book.on_add(order_id, side, price, qty);
    }
    
    for (auto _ : state) {
        auto tob = order_book.top_of_book();
        benchmark::DoNotOptimize(tob);
    }
    
    state.SetItemsProcessed(state.iterations());
}

static void BM_OrderBookMixedOperations(benchmark::State& state) {
    book::OrderBook order_book;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);
    std::uniform_int_distribution<int64_t> price_dist(100000000000LL, 105000000000LL);
    std::uniform_int_distribution<uint32_t> qty_dist(100, 1000);
    
    uint64_t next_order_id = 1;
    std::vector<uint64_t> active_orders;
    
    for (auto _ : state) {
        double op = op_dist(rng);
        
        if (active_orders.empty() || op < 0.4) {
            // Add order
            int64_t price = price_dist(rng);
            uint32_t qty = qty_dist(rng);
            book::Side side = (next_order_id % 2 == 0) ? book::Side::BUY : book::Side::SELL;
            
            if (order_book.on_add(next_order_id, side, price, qty)) {
                active_orders.push_back(next_order_id);
            }
            next_order_id++;
            
        } else if (op < 0.6) {
            // Modify order
            if (!active_orders.empty()) {
                size_t idx = static_cast<size_t>(op_dist(rng) * active_orders.size());
                uint64_t order_id = active_orders[idx];
                int64_t new_price = price_dist(rng);
                uint32_t new_qty = qty_dist(rng);
                order_book.on_modify(order_id, new_price, new_qty);
            }
            
        } else if (op < 0.8) {
            // Execute order
            if (!active_orders.empty()) {
                size_t idx = static_cast<size_t>(op_dist(rng) * active_orders.size());
                uint64_t order_id = active_orders[idx];
                uint32_t exec_qty = static_cast<uint32_t>(op_dist(rng) * 100) + 10;
                
                if (order_book.on_execute(order_id, exec_qty)) {
                    // Sometimes remove fully executed orders
                    if (op_dist(rng) < 0.3) {
                        active_orders.erase(active_orders.begin() + idx);
                    }
                }
            }
            
        } else {
            // Delete order
            if (!active_orders.empty()) {
                size_t idx = static_cast<size_t>(op_dist(rng) * active_orders.size());
                uint64_t order_id = active_orders[idx];
                
                if (order_book.on_delete(order_id)) {
                    active_orders.erase(active_orders.begin() + idx);
                }
            }
        }
        
        // Get top of book (simulates publishing)
        auto tob = order_book.top_of_book();
        benchmark::DoNotOptimize(tob);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Register benchmarks
BENCHMARK(BM_OrderBookAdd)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_OrderBookModify)->Range(100, 10000)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_OrderBookExecute)->Range(100, 10000)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_OrderBookTopOfBook)->Range(100, 10000)->Unit(benchmark::kNanosecond);
BENCHMARK(BM_OrderBookMixedOperations)->Unit(benchmark::kNanosecond);
