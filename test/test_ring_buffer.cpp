/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "ring_buffer.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

namespace {

TEST(RingBufferTest, BasicOperations) {
    core::RingBuffer<int> buffer(4); // Power of 2
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
    EXPECT_EQ(buffer.capacity(), 4);
    
    // Test push
    EXPECT_TRUE(buffer.try_push(1));
    EXPECT_TRUE(buffer.try_push(2));
    EXPECT_TRUE(buffer.try_push(3));
    
    EXPECT_FALSE(buffer.empty());
    EXPECT_EQ(buffer.size(), 3);
    
    // Test pop
    int value;
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 1);
    
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 2);
    
    EXPECT_EQ(buffer.size(), 1);
    
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 3);
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);
    
    // Test pop from empty
    EXPECT_FALSE(buffer.try_pop(value));
}

TEST(RingBufferTest, FullBuffer) {
    core::RingBuffer<int> buffer(4);
    
    // Fill buffer to capacity-1 (ring buffer can't distinguish full from empty)
    EXPECT_TRUE(buffer.try_push(1));
    EXPECT_TRUE(buffer.try_push(2));
    EXPECT_TRUE(buffer.try_push(3));
    
    // Next push should fail (buffer full)
    EXPECT_FALSE(buffer.try_push(4));
    
    // Pop one element
    int value;
    EXPECT_TRUE(buffer.try_pop(value));
    EXPECT_EQ(value, 1);
    
    // Now we can push again
    EXPECT_TRUE(buffer.try_push(4));
}

TEST(RingBufferTest, MoveSemantics) {
    core::RingBuffer<std::unique_ptr<int>> buffer(4);
    
    auto ptr1 = std::make_unique<int>(42);
    auto ptr2 = std::make_unique<int>(99);
    
    // Test move push
    EXPECT_TRUE(buffer.try_push(std::move(ptr1)));
    EXPECT_TRUE(buffer.try_push(std::move(ptr2)));
    
    EXPECT_EQ(ptr1, nullptr); // Should be moved
    EXPECT_EQ(ptr2, nullptr); // Should be moved
    
    // Test move pop
    std::unique_ptr<int> popped;
    EXPECT_TRUE(buffer.try_pop(popped));
    EXPECT_NE(popped, nullptr);
    EXPECT_EQ(*popped, 42);
    
    EXPECT_TRUE(buffer.try_pop(popped));
    EXPECT_NE(popped, nullptr);
    EXPECT_EQ(*popped, 99);
}

TEST(RingBufferTest, SingleProducerSingleConsumer) {
    constexpr size_t NUM_ITEMS = 10000;
    core::RingBuffer<int> buffer(1024);
    
    std::atomic<bool> producer_done{false};
    std::vector<int> consumed;
    consumed.reserve(NUM_ITEMS);
    
    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            while (!buffer.try_push(static_cast<int>(i))) {
                std::this_thread::yield();
            }
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (!producer_done || !buffer.empty()) {
            if (buffer.try_pop(value)) {
                consumed.push_back(value);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Verify all items were consumed in order
    EXPECT_EQ(consumed.size(), NUM_ITEMS);
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
        EXPECT_EQ(consumed[i], static_cast<int>(i));
    }
}

TEST(RingBufferTest, PowerOfTwoAssertion) {
    // Valid power of 2 sizes should work
    EXPECT_NO_THROW(core::RingBuffer<int>(2));
    EXPECT_NO_THROW(core::RingBuffer<int>(4));
    EXPECT_NO_THROW(core::RingBuffer<int>(8));
    EXPECT_NO_THROW(core::RingBuffer<int>(1024));
    
    // Invalid sizes should assert (in debug mode)
    // Note: In release mode, these might not assert
#ifdef DEBUG
    EXPECT_DEATH(core::RingBuffer<int>(3), ".*");
    EXPECT_DEATH(core::RingBuffer<int>(5), ".*");
    EXPECT_DEATH(core::RingBuffer<int>(0), ".*");
#endif
}

} // anonymous namespace
