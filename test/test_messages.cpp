/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "messages.hpp"
#include <gtest/gtest.h>
#include <cstring>

namespace {

TEST(MessagesTest, MessageSizes) {
    // Verify packed structure sizes - adjust for actual compiler behavior
    EXPECT_EQ(sizeof(feed::AddOrderMsg), 36);      // 1+8+8+6+1+8+4 = 36 
    EXPECT_EQ(sizeof(feed::ModifyOrderMsg), 29);   // 1+8+8+8+4 = 29  
    EXPECT_EQ(sizeof(feed::ExecuteOrderMsg), 21);  // 1+8+8+4 = 21
    EXPECT_EQ(sizeof(feed::DeleteOrderMsg), 17);   // 1+8+8 = 17
}

TEST(MessagesTest, SymbolConstruction) {
    feed::Symbol symbol1("AAPL");
    EXPECT_EQ(symbol1.to_string(), "AAPL");
    
    feed::Symbol symbol2("GOOGL");
    EXPECT_EQ(symbol2.to_string(), "GOOGL");
    
    // Test padding
    feed::Symbol symbol3("A");
    EXPECT_EQ(symbol3.to_string(), "A");
    
    // Test max length
    feed::Symbol symbol4("TOOLONG");
    EXPECT_EQ(symbol4.to_string(), "TOOLO"); // Should truncate to 5 chars
}

TEST(MessagesTest, SymbolComparison) {
    feed::Symbol symbol1("AAPL");
    feed::Symbol symbol2("AAPL");
    feed::Symbol symbol3("MSFT");
    
    EXPECT_EQ(symbol1, symbol2);
    EXPECT_NE(symbol1, symbol3);
    EXPECT_LT(symbol1, symbol3); // AAPL < MSFT
}

TEST(MessagesTest, EventConstruction) {
    feed::Event event;
    EXPECT_EQ(event.type, feed::EventType::INVALID);
    EXPECT_EQ(event.decode_timestamp_us, 0);
    
    feed::EventPayload payload;
    payload.add.type = 'A';
    payload.add.order_id = 12345;
    
    feed::Event event2(feed::EventType::ADD_ORDER, payload, 1000);
    EXPECT_EQ(event2.type, feed::EventType::ADD_ORDER);
    EXPECT_EQ(event2.decode_timestamp_us, 1000);
    EXPECT_EQ(event2.payload.add.order_id, 12345);
}

TEST(MessagesTest, AddOrderMessage) {
    feed::AddOrderMsg msg;
    msg.type = 'A';
    msg.ts_us = 1234567890;
    msg.order_id = 12345;
    std::memcpy(msg.symbol, "AAPL  ", 6);
    msg.side = 'B';
    msg.px_nano = 150000000000LL; // $150.00
    msg.qty = 100;
    
    EXPECT_EQ(msg.type, 'A');
    EXPECT_EQ(msg.ts_us, 1234567890);
    EXPECT_EQ(msg.order_id, 12345);
    EXPECT_EQ(msg.side, 'B');
    EXPECT_EQ(msg.px_nano, 150000000000LL);
    EXPECT_EQ(msg.qty, 100);
}

} // anonymous namespace
