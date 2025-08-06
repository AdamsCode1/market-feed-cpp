/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "order_book.hpp"
#include <gtest/gtest.h>

namespace {

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = std::make_unique<book::OrderBook>();
    }
    
    std::unique_ptr<book::OrderBook> book;
};

TEST_F(OrderBookTest, EmptyBook) {
    EXPECT_TRUE(book->empty());
    EXPECT_EQ(book->order_count(), 0);
    
    auto tob = book->top_of_book();
    EXPECT_FALSE(tob.has_bid());
    EXPECT_FALSE(tob.has_ask());
}

TEST_F(OrderBookTest, AddOrders) {
    // Add buy order
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100)); // $100.00, qty 100
    EXPECT_FALSE(book->empty());
    EXPECT_EQ(book->order_count(), 1);
    
    // Add sell order
    EXPECT_TRUE(book->on_add(2, book::Side::SELL, 101000000000LL, 200)); // $101.00, qty 200
    EXPECT_EQ(book->order_count(), 2);
    
    auto tob = book->top_of_book();
    EXPECT_TRUE(tob.has_bid());
    EXPECT_TRUE(tob.has_ask());
    EXPECT_EQ(tob.best_bid_px, 100000000000LL);
    EXPECT_EQ(tob.bid_sz, 100);
    EXPECT_EQ(tob.best_ask_px, 101000000000LL);
    EXPECT_EQ(tob.ask_sz, 200);
}

TEST_F(OrderBookTest, DuplicateOrderId) {
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    // Try to add order with same ID
    EXPECT_FALSE(book->on_add(1, book::Side::SELL, 101000000000LL, 200));
    EXPECT_EQ(book->order_count(), 1);
}

TEST_F(OrderBookTest, CrossingOrders) {
    // Add buy order at $100
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    
    // Try to add sell order at $99 (would cross)
    EXPECT_FALSE(book->on_add(2, book::Side::SELL, 99000000000LL, 200));
    EXPECT_EQ(book->order_count(), 1);
    
    // Add valid sell order at $101
    EXPECT_TRUE(book->on_add(2, book::Side::SELL, 101000000000LL, 200));
    EXPECT_EQ(book->order_count(), 2);
}

TEST_F(OrderBookTest, ModifyOrder) {
    // Add order
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    
    // Modify price and quantity
    EXPECT_TRUE(book->on_modify(1, 99000000000LL, 150));
    
    auto tob = book->top_of_book();
    EXPECT_EQ(tob.best_bid_px, 99000000000LL);
    EXPECT_EQ(tob.bid_sz, 150);
    
    // Try to modify non-existent order
    EXPECT_FALSE(book->on_modify(999, 100000000000LL, 100));
}

TEST_F(OrderBookTest, ExecuteOrder) {
    // Add order
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    
    // Partial execution
    EXPECT_TRUE(book->on_execute(1, 30));
    
    auto tob = book->top_of_book();
    EXPECT_EQ(tob.best_bid_px, 100000000000LL);
    EXPECT_EQ(tob.bid_sz, 70); // 100 - 30
    EXPECT_EQ(book->order_count(), 1);
    
    // Full execution
    EXPECT_TRUE(book->on_execute(1, 70));
    
    tob = book->top_of_book();
    EXPECT_FALSE(tob.has_bid()); // Order should be gone
    EXPECT_EQ(book->order_count(), 0);
    
    // Try to execute non-existent order
    EXPECT_FALSE(book->on_execute(999, 10));
}

TEST_F(OrderBookTest, ExecuteMoreThanAvailable) {
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    
    // Try to execute more than available
    EXPECT_FALSE(book->on_execute(1, 150));
    
    // Order should still exist with original quantity
    auto tob = book->top_of_book();
    EXPECT_EQ(tob.bid_sz, 100);
}

TEST_F(OrderBookTest, DeleteOrder) {
    // Add order
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    EXPECT_EQ(book->order_count(), 1);
    
    // Delete order
    EXPECT_TRUE(book->on_delete(1));
    EXPECT_EQ(book->order_count(), 0);
    EXPECT_TRUE(book->empty());
    
    auto tob = book->top_of_book();
    EXPECT_FALSE(tob.has_bid());
    
    // Try to delete non-existent order
    EXPECT_FALSE(book->on_delete(999));
}

TEST_F(OrderBookTest, PriceLevelAggregation) {
    // Add multiple orders at same price
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    EXPECT_TRUE(book->on_add(2, book::Side::BUY, 100000000000LL, 200));
    EXPECT_TRUE(book->on_add(3, book::Side::BUY, 100000000000LL, 50));
    
    auto tob = book->top_of_book();
    EXPECT_EQ(tob.best_bid_px, 100000000000LL);
    EXPECT_EQ(tob.bid_sz, 350); // 100 + 200 + 50
    
    // Delete one order
    EXPECT_TRUE(book->on_delete(2));
    
    tob = book->top_of_book();
    EXPECT_EQ(tob.best_bid_px, 100000000000LL);
    EXPECT_EQ(tob.bid_sz, 150); // 100 + 50
}

TEST_F(OrderBookTest, BestPriceOrdering) {
    // Add bids at different prices
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100)); // $100
    EXPECT_TRUE(book->on_add(2, book::Side::BUY, 99000000000LL, 200));  // $99
    EXPECT_TRUE(book->on_add(3, book::Side::BUY, 101000000000LL, 50));  // $101
    
    // Add asks at different prices
    EXPECT_TRUE(book->on_add(4, book::Side::SELL, 102000000000LL, 100)); // $102
    EXPECT_TRUE(book->on_add(5, book::Side::SELL, 103000000000LL, 200)); // $103
    EXPECT_TRUE(book->on_add(6, book::Side::SELL, 101500000000LL, 50));  // $101.50
    
    auto tob = book->top_of_book();
    
    // Best bid should be highest price ($101)
    EXPECT_EQ(tob.best_bid_px, 101000000000LL);
    EXPECT_EQ(tob.bid_sz, 50);
    
    // Best ask should be lowest price ($101.50)
    EXPECT_EQ(tob.best_ask_px, 101500000000LL);
    EXPECT_EQ(tob.ask_sz, 50);
}

TEST_F(OrderBookTest, CompleteLifecycle) {
    // Complete order lifecycle: add -> modify -> execute -> delete
    
    // 1. Add order
    EXPECT_TRUE(book->on_add(1, book::Side::BUY, 100000000000LL, 100));
    EXPECT_EQ(book->order_count(), 1);
    
    // 2. Modify order
    EXPECT_TRUE(book->on_modify(1, 99000000000LL, 150));
    auto tob = book->top_of_book();
    EXPECT_EQ(tob.best_bid_px, 99000000000LL);
    EXPECT_EQ(tob.bid_sz, 150);
    
    // 3. Execute partial
    EXPECT_TRUE(book->on_execute(1, 50));
    tob = book->top_of_book();
    EXPECT_EQ(tob.bid_sz, 100);
    EXPECT_EQ(book->order_count(), 1);
    
    // 4. Delete remaining
    EXPECT_TRUE(book->on_delete(1));
    EXPECT_EQ(book->order_count(), 0);
    EXPECT_TRUE(book->empty());
    
    tob = book->top_of_book();
    EXPECT_FALSE(tob.has_bid());
    EXPECT_FALSE(tob.has_ask());
}

} // anonymous namespace
