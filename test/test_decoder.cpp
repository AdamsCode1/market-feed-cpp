/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "decoder.hpp"
#include "messages.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

namespace {

class DecoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_filename = "test_feed_XXXXXX";
        // Create a temporary file
        temp_fd = mkstemp(&temp_filename[0]);
        ASSERT_NE(temp_fd, -1);
        close(temp_fd);
    }
    
    void TearDown() override {
        // Remove temporary file
        std::remove(temp_filename.c_str());
    }
    
    void write_message(const void* msg, size_t size) {
        std::ofstream file(temp_filename, std::ios::binary | std::ios::app);
        file.write(static_cast<const char*>(msg), size);
    }
    
    std::string temp_filename;
    int temp_fd;
};

TEST_F(DecoderTest, EmptyFile) {
    // Create empty file
    std::ofstream file(temp_filename, std::ios::binary);
    file.close();
    
    EXPECT_THROW(feed::Decoder decoder(temp_filename), std::runtime_error);
}

TEST_F(DecoderTest, NonExistentFile) {
    EXPECT_THROW(feed::Decoder decoder("nonexistent.bin"), std::runtime_error);
}

TEST_F(DecoderTest, AddOrderMessage) {
    // Create add order message
    feed::AddOrderMsg msg;
    msg.type = 'A';
    msg.ts_us = 1234567890;
    msg.order_id = 12345;
    std::memcpy(msg.symbol, "AAPL  ", 6);
    msg.side = 'B';
    msg.px_nano = 150000000000LL;
    msg.qty = 100;
    
    write_message(&msg, sizeof(msg));
    
    feed::Decoder decoder(temp_filename);
    EXPECT_EQ(decoder.size(), sizeof(msg));
    EXPECT_EQ(decoder.position(), 0);
    EXPECT_TRUE(decoder.has_next());
    
    feed::Event event = decoder.next();
    EXPECT_EQ(event.type, feed::EventType::ADD_ORDER);
    EXPECT_EQ(event.payload.add.ts_us, 1234567890);
    EXPECT_EQ(event.payload.add.order_id, 12345);
    EXPECT_EQ(event.payload.add.side, 'B');
    EXPECT_EQ(event.payload.add.px_nano, 150000000000LL);
    EXPECT_EQ(event.payload.add.qty, 100);
    
    EXPECT_FALSE(decoder.has_next());
}

TEST_F(DecoderTest, ModifyOrderMessage) {
    feed::ModifyOrderMsg msg;
    msg.type = 'U';
    msg.ts_us = 1234567891;
    msg.order_id = 12345;
    msg.new_px_nano = 151000000000LL;
    msg.new_qty = 150;
    
    write_message(&msg, sizeof(msg));
    
    feed::Decoder decoder(temp_filename);
    feed::Event event = decoder.next();
    
    EXPECT_EQ(event.type, feed::EventType::MODIFY_ORDER);
    EXPECT_EQ(event.payload.modify.ts_us, 1234567891);
    EXPECT_EQ(event.payload.modify.order_id, 12345);
    EXPECT_EQ(event.payload.modify.new_px_nano, 151000000000LL);
    EXPECT_EQ(event.payload.modify.new_qty, 150);
}

TEST_F(DecoderTest, ExecuteOrderMessage) {
    feed::ExecuteOrderMsg msg;
    msg.type = 'E';
    msg.ts_us = 1234567892;
    msg.order_id = 12345;
    msg.exec_qty = 50;
    
    write_message(&msg, sizeof(msg));
    
    feed::Decoder decoder(temp_filename);
    feed::Event event = decoder.next();
    
    EXPECT_EQ(event.type, feed::EventType::EXECUTE_ORDER);
    EXPECT_EQ(event.payload.execute.ts_us, 1234567892);
    EXPECT_EQ(event.payload.execute.order_id, 12345);
    EXPECT_EQ(event.payload.execute.exec_qty, 50);
}

TEST_F(DecoderTest, DeleteOrderMessage) {
    feed::DeleteOrderMsg msg;
    msg.type = 'D';
    msg.ts_us = 1234567893;
    msg.order_id = 12345;
    
    write_message(&msg, sizeof(msg));
    
    feed::Decoder decoder(temp_filename);
    feed::Event event = decoder.next();
    
    EXPECT_EQ(event.type, feed::EventType::DELETE_ORDER);
    EXPECT_EQ(event.payload.delete_order.ts_us, 1234567893);
    EXPECT_EQ(event.payload.delete_order.order_id, 12345);
}

TEST_F(DecoderTest, MultipleMessages) {
    // Write multiple messages
    feed::AddOrderMsg add_msg;
    add_msg.type = 'A';
    add_msg.ts_us = 1000;
    add_msg.order_id = 1;
    std::memcpy(add_msg.symbol, "AAPL  ", 6);
    add_msg.side = 'B';
    add_msg.px_nano = 100000000000LL;
    add_msg.qty = 100;
    write_message(&add_msg, sizeof(add_msg));
    
    feed::ExecuteOrderMsg exec_msg;
    exec_msg.type = 'E';
    exec_msg.ts_us = 2000;
    exec_msg.order_id = 1;
    exec_msg.exec_qty = 30;
    write_message(&exec_msg, sizeof(exec_msg));
    
    feed::DeleteOrderMsg del_msg;
    del_msg.type = 'D';
    del_msg.ts_us = 3000;
    del_msg.order_id = 1;
    write_message(&del_msg, sizeof(del_msg));
    
    feed::Decoder decoder(temp_filename);
    
    // Read first message
    EXPECT_TRUE(decoder.has_next());
    feed::Event event1 = decoder.next();
    EXPECT_EQ(event1.type, feed::EventType::ADD_ORDER);
    EXPECT_EQ(event1.payload.add.order_id, 1);
    
    // Read second message
    EXPECT_TRUE(decoder.has_next());
    feed::Event event2 = decoder.next();
    EXPECT_EQ(event2.type, feed::EventType::EXECUTE_ORDER);
    EXPECT_EQ(event2.payload.execute.order_id, 1);
    
    // Read third message
    EXPECT_TRUE(decoder.has_next());
    feed::Event event3 = decoder.next();
    EXPECT_EQ(event3.type, feed::EventType::DELETE_ORDER);
    EXPECT_EQ(event3.payload.delete_order.order_id, 1);
    
    EXPECT_FALSE(decoder.has_next());
}

TEST_F(DecoderTest, InvalidMessageType) {
    // Write message with invalid type
    char invalid_msg[10];
    invalid_msg[0] = 'X'; // Invalid type
    write_message(invalid_msg, sizeof(invalid_msg));
    
    feed::Decoder decoder(temp_filename);
    EXPECT_TRUE(decoder.has_next());
    
    // Should skip invalid message and return invalid event
    feed::Event event = decoder.next();
    EXPECT_EQ(event.type, feed::EventType::INVALID);
}

TEST_F(DecoderTest, IncompleteMessage) {
    // Write incomplete message (only type byte)
    char incomplete[1] = {'A'};
    write_message(incomplete, sizeof(incomplete));
    
    feed::Decoder decoder(temp_filename);
    EXPECT_TRUE(decoder.has_next());
    
    feed::Event event = decoder.next();
    EXPECT_EQ(event.type, feed::EventType::INVALID);
}

TEST_F(DecoderTest, Reset) {
    feed::AddOrderMsg msg;
    msg.type = 'A';
    msg.ts_us = 1000;
    msg.order_id = 1;
    std::memcpy(msg.symbol, "AAPL  ", 6);
    msg.side = 'B';
    msg.px_nano = 100000000000LL;
    msg.qty = 100;
    write_message(&msg, sizeof(msg));
    
    feed::Decoder decoder(temp_filename);
    
    // Read message
    EXPECT_TRUE(decoder.has_next());
    feed::Event event1 = decoder.next();
    EXPECT_EQ(event1.type, feed::EventType::ADD_ORDER);
    EXPECT_FALSE(decoder.has_next());
    
    // Reset and read again
    decoder.reset();
    EXPECT_EQ(decoder.position(), 0);
    EXPECT_TRUE(decoder.has_next());
    
    feed::Event event2 = decoder.next();
    EXPECT_EQ(event2.type, feed::EventType::ADD_ORDER);
    EXPECT_EQ(event2.payload.add.order_id, 1);
}

TEST_F(DecoderTest, MoveSemantics) {
    feed::AddOrderMsg msg;
    msg.type = 'A';
    msg.ts_us = 1000;
    msg.order_id = 1;
    std::memcpy(msg.symbol, "AAPL  ", 6);
    msg.side = 'B';
    msg.px_nano = 100000000000LL;
    msg.qty = 100;
    write_message(&msg, sizeof(msg));
    
    feed::Decoder decoder1(temp_filename);
    EXPECT_TRUE(decoder1.has_next());
    
    // Move constructor
    feed::Decoder decoder2 = std::move(decoder1);
    EXPECT_TRUE(decoder2.has_next());
    
    feed::Event event = decoder2.next();
    EXPECT_EQ(event.type, feed::EventType::ADD_ORDER);
}

} // anonymous namespace
