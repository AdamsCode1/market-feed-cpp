/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#pragma once

#include "messages.hpp"
#include <map>
#include <unordered_map>
#include <cstdint>
#include <optional>

namespace book {

/**
 * @brief Side of the order book
 */
enum class Side : char {
    BUY = 'B',
    SELL = 'S'
};

/**
 * @brief Price level in the order book
 */
struct PriceLevel {
    int64_t price;
    uint32_t quantity;
    
    PriceLevel(int64_t p, uint32_t q) : price(p), quantity(q) {}
};

/**
 * @brief Order information
 */
struct OrderInfo {
    Side side;
    int64_t price;
    uint32_t quantity;
    
    OrderInfo(Side s, int64_t p, uint32_t q) : side(s), price(p), quantity(q) {}
};

/**
 * @brief Top of book snapshot
 */
struct TopOfBook {
    int64_t best_bid_px = 0;
    uint32_t bid_sz = 0;
    int64_t best_ask_px = 0;
    uint32_t ask_sz = 0;
    
    bool has_bid() const { return bid_sz > 0; }
    bool has_ask() const { return ask_sz > 0; }
};

/**
 * @brief Limit order book implementation
 */
class OrderBook {
public:
    /**
     * @brief Constructor
     */
    OrderBook() = default;
    
    /**
     * @brief Add a new order to the book
     * @param order_id Unique order identifier
     * @param side Order side (BUY/SELL)
     * @param price Order price in nano-units
     * @param quantity Order quantity
     * @return true if successful, false if order already exists
     */
    bool on_add(uint64_t order_id, Side side, int64_t price, uint32_t quantity);
    
    /**
     * @brief Modify an existing order
     * @param order_id Order identifier
     * @param new_price New price in nano-units
     * @param new_quantity New quantity
     * @return true if successful, false if order doesn't exist
     */
    bool on_modify(uint64_t order_id, int64_t new_price, uint32_t new_quantity);
    
    /**
     * @brief Execute (partially fill) an order
     * @param order_id Order identifier
     * @param exec_quantity Quantity to execute
     * @return true if successful, false if order doesn't exist or insufficient quantity
     */
    bool on_execute(uint64_t order_id, uint32_t exec_quantity);
    
    /**
     * @brief Delete an order from the book
     * @param order_id Order identifier
     * @return true if successful, false if order doesn't exist
     */
    bool on_delete(uint64_t order_id);
    
    /**
     * @brief Get current top of book
     * @return Top of book snapshot
     */
    TopOfBook top_of_book() const;
    
    /**
     * @brief Get number of orders in the book
     * @return Total number of orders
     */
    size_t order_count() const { return orders_.size(); }
    
    /**
     * @brief Check if book is empty
     * @return true if no orders
     */
    bool empty() const { return orders_.empty(); }

private:
    // Bids: higher price first (descending)
    std::map<int64_t, uint32_t, std::greater<int64_t>> bids_;
    
    // Asks: lower price first (ascending)  
    std::map<int64_t, uint32_t> asks_;
    
    // Order tracking
    std::unordered_map<uint64_t, OrderInfo> orders_;
    
    void add_to_level(Side side, int64_t price, uint32_t quantity);
    void remove_from_level(Side side, int64_t price, uint32_t quantity);
    bool has_crossing(Side side, int64_t price) const;
};

} // namespace book
