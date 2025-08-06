/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "decoder.hpp"
#include "order_book.hpp"
#include "ring_buffer.hpp"
#include "publisher.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace {

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_filename = "integration_test_XXXXXX";
        temp_fd = mkstemp(&temp_filename[0]);
        ASSERT_NE(temp_fd, -1);
        close(temp_fd);
    }
    
    void TearDown() override {
        std::remove(temp_filename.c_str());
    }
    
    void create_sample_feed() {
        std::ofstream file(temp_filename, std::ios::binary);
        
        // Add order AAPL
        feed::AddOrderMsg add1;
        add1.type = 'A';
        add1.ts_us = 1000;
        add1.order_id = 1;
        std::memcpy(add1.symbol, "AAPL  ", 6);
        add1.side = 'B';
        add1.px_nano = 150000000000LL; // $150.00
        add1.qty = 100;
        file.write(reinterpret_cast<const char*>(&add1), sizeof(add1));
        
        // Add order AAPL (ask)
        feed::AddOrderMsg add2;
        add2.type = 'A';
        add2.ts_us = 2000;
        add2.order_id = 2;
        std::memcpy(add2.symbol, "AAPL  ", 6);
        add2.side = 'S';
        add2.px_nano = 151000000000LL; // $151.00
        add2.qty = 200;
        file.write(reinterpret_cast<const char*>(&add2), sizeof(add2));
        
        // Modify order
        feed::ModifyOrderMsg modify;
        modify.type = 'U';
        modify.ts_us = 3000;
        modify.order_id = 1;
        modify.new_px_nano = 150500000000LL; // $150.50
        modify.new_qty = 150;
        file.write(reinterpret_cast<const char*>(&modify), sizeof(modify));
        
        // Execute order
        feed::ExecuteOrderMsg execute;
        execute.type = 'E';
        execute.ts_us = 4000;
        execute.order_id = 1;
        execute.exec_qty = 50;
        file.write(reinterpret_cast<const char*>(&execute), sizeof(execute));
        
        // Delete order
        feed::DeleteOrderMsg delete_msg;
        delete_msg.type = 'D';
        delete_msg.ts_us = 5000;
        delete_msg.order_id = 2;
        file.write(reinterpret_cast<const char*>(&delete_msg), sizeof(delete_msg));
        
        file.close();
    }
    
    std::string temp_filename;
    int temp_fd;
};

TEST_F(IntegrationTest, EndToEndProcessing) {
    create_sample_feed();
    
    // Create components
    feed::Decoder decoder(temp_filename);
    book::OrderBook order_book;
    std::ostringstream output;
    publish::TopOfBookPublisher publisher(output);
    
    feed::Symbol aapl_symbol("AAPL");
    
    // Process all messages
    std::vector<feed::Event> events;
    while (decoder.has_next()) {
        feed::Event event = decoder.next();
        if (event.type != feed::EventType::INVALID) {
            events.push_back(event);
        }
    }
    
    EXPECT_EQ(events.size(), 5);
    
    // Apply events to order book
    for (const auto& event : events) {
        switch (event.type) {
            case feed::EventType::ADD_ORDER: {
                const auto& msg = event.payload.add;
                book::Side side = (msg.side == 'B') ? book::Side::BUY : book::Side::SELL;
                EXPECT_TRUE(order_book.on_add(msg.order_id, side, msg.px_nano, msg.qty));
                break;
            }
            case feed::EventType::MODIFY_ORDER: {
                const auto& msg = event.payload.modify;
                EXPECT_TRUE(order_book.on_modify(msg.order_id, msg.new_px_nano, msg.new_qty));
                break;
            }
            case feed::EventType::EXECUTE_ORDER: {
                const auto& msg = event.payload.execute;
                EXPECT_TRUE(order_book.on_execute(msg.order_id, msg.exec_qty));
                break;
            }
            case feed::EventType::DELETE_ORDER: {
                const auto& msg = event.payload.delete_order;
                EXPECT_TRUE(order_book.on_delete(msg.order_id));
                break;
            }
            default:
                break;
        }
        
        // Publish top of book after each event
        auto tob = order_book.top_of_book();
        publisher.publish(event.decode_timestamp_us, aapl_symbol, tob);
    }
    
    // Final state: only bid should remain (order 1 with qty 100 after execution)
    auto final_tob = order_book.top_of_book();
    EXPECT_TRUE(final_tob.has_bid());
    EXPECT_FALSE(final_tob.has_ask()); // Order 2 was deleted
    EXPECT_EQ(final_tob.best_bid_px, 150500000000LL); // Modified price
    EXPECT_EQ(final_tob.bid_sz, 100); // 150 - 50 (executed)
    
    // Check that publisher output contains CSV data
    std::string output_str = output.str();
    EXPECT_FALSE(output_str.empty());
    EXPECT_NE(output_str.find("ts_us,symbol,bid_px,bid_sz,ask_px,ask_sz"), std::string::npos);
    EXPECT_NE(output_str.find("AAPL"), std::string::npos);
}

TEST_F(IntegrationTest, RingBufferIntegration) {
    create_sample_feed();
    
    feed::Decoder decoder(temp_filename);
    core::RingBuffer<feed::Event> ring_buffer(16);
    
    // Producer: decode and push to ring buffer
    std::vector<feed::Event> produced_events;
    while (decoder.has_next()) {
        feed::Event event = decoder.next();
        if (event.type != feed::EventType::INVALID) {
            produced_events.push_back(event);
            while (!ring_buffer.try_push(event)) {
                // Buffer full, in real implementation would yield
            }
        }
    }
    
    // Consumer: pop from ring buffer
    std::vector<feed::Event> consumed_events;
    feed::Event event;
    while (ring_buffer.try_pop(event)) {
        consumed_events.push_back(event);
    }
    
    // Should have same number of events
    EXPECT_EQ(produced_events.size(), consumed_events.size());
    EXPECT_EQ(consumed_events.size(), 5);
    
    // Events should be in same order
    for (size_t i = 0; i < produced_events.size(); ++i) {
        EXPECT_EQ(produced_events[i].type, consumed_events[i].type);
    }
}

TEST_F(IntegrationTest, FullPipelineStressTest) {
    // Create larger test file
    std::ofstream file(temp_filename, std::ios::binary);
    
    constexpr int NUM_ORDERS = 1000;
    
    // Generate many orders with proper bid/ask spread
    for (int i = 1; i <= NUM_ORDERS; ++i) {
        feed::AddOrderMsg add;
        add.type = 'A';
        add.ts_us = i * 1000;
        add.order_id = i;
        std::memcpy(add.symbol, "TEST  ", 6);
        add.side = (i % 2 == 0) ? 'B' : 'S';
        
        // Ensure bid prices are lower than ask prices to avoid crossing
        if (add.side == 'B') {
            // Buy orders: prices from 99.00 down to 98.00
            add.px_nano = 99000000000LL - (i * 1000000LL); 
        } else {
            // Sell orders: prices from 101.00 up to 102.00  
            add.px_nano = 101000000000LL + (i * 1000000LL);
        }
        
        add.qty = 100;
        file.write(reinterpret_cast<const char*>(&add), sizeof(add));
    }
    file.close();
    
    // Process through full pipeline
    feed::Decoder decoder(temp_filename);
    core::RingBuffer<feed::Event> ring_buffer(2048);
    book::OrderBook order_book;
    
    size_t total_events = 0;
    
    // Decode all events
    while (decoder.has_next()) {
        feed::Event event = decoder.next();
        if (event.type != feed::EventType::INVALID) {
            ring_buffer.try_push(event);
            total_events++;
        }
    }
    
    EXPECT_EQ(total_events, NUM_ORDERS);
    
    // Process all events
    feed::Event event;
    size_t processed_events = 0;
    while (ring_buffer.try_pop(event)) {
        if (event.type == feed::EventType::ADD_ORDER) {
            const auto& msg = event.payload.add;
            book::Side side = (msg.side == 'B') ? book::Side::BUY : book::Side::SELL;
            order_book.on_add(msg.order_id, side, msg.px_nano, msg.qty);
        }
        processed_events++;
    }
    
    EXPECT_EQ(processed_events, NUM_ORDERS);
    EXPECT_EQ(order_book.order_count(), NUM_ORDERS);
    
    // Check that top of book is reasonable
    auto tob = order_book.top_of_book();
    EXPECT_TRUE(tob.has_bid());
    EXPECT_TRUE(tob.has_ask());
    EXPECT_LT(tob.best_bid_px, tob.best_ask_px); // No crossing
}

} // anonymous namespace
