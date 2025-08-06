/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "order_book.hpp"
#include <algorithm>

namespace book {

bool OrderBook::on_add(uint64_t order_id, Side side, int64_t price, uint32_t quantity) {
    // Check if order already exists
    if (orders_.find(order_id) != orders_.end()) {
        return false;
    }
    
    // Check for crossing (would create invalid book state)
    if (has_crossing(side, price)) {
        return false;
    }
    
    // Add to orders map
    orders_.emplace(order_id, OrderInfo(side, price, quantity));
    
    // Add to price level
    add_to_level(side, price, quantity);
    
    return true;
}

bool OrderBook::on_modify(uint64_t order_id, int64_t new_price, uint32_t new_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    if (new_quantity == 0) {
        return false;
    }
    
    OrderInfo& order = it->second;
    
    // Check for crossing with new price
    if (has_crossing(order.side, new_price)) {
        return false;
    }
    
    // Remove old quantity from old price level
    remove_from_level(order.side, order.price, order.quantity);
    
    // Update order
    order.price = new_price;
    order.quantity = new_quantity;
    
    // Add new quantity to new price level
    add_to_level(order.side, new_price, new_quantity);
    
    return true;
}

bool OrderBook::on_execute(uint64_t order_id, uint32_t exec_quantity) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    OrderInfo& order = it->second;
    if (exec_quantity > order.quantity) {
        return false; // Cannot execute more than available
    }
    
    // Remove executed quantity from price level
    remove_from_level(order.side, order.price, exec_quantity);
    
    // Update order quantity
    order.quantity -= exec_quantity;
    
    // If fully executed, remove order
    if (order.quantity == 0) {
        orders_.erase(it);
    }
    
    return true;
}

bool OrderBook::on_delete(uint64_t order_id) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    const OrderInfo& order = it->second;
    
    // Remove from price level
    remove_from_level(order.side, order.price, order.quantity);
    
    // Remove from orders map
    orders_.erase(it);
    
    return true;
}

TopOfBook OrderBook::top_of_book() const {
    TopOfBook tob;
    
    // Get best bid (highest price)
    if (!bids_.empty()) {
        auto best_bid = bids_.begin();
        tob.best_bid_px = best_bid->first;
        tob.bid_sz = best_bid->second;
    }
    
    // Get best ask (lowest price)
    if (!asks_.empty()) {
        auto best_ask = asks_.begin();
        tob.best_ask_px = best_ask->first;
        tob.ask_sz = best_ask->second;
    }
    
    return tob;
}

void OrderBook::add_to_level(Side side, int64_t price, uint32_t quantity) {
    if (side == Side::BUY) {
        bids_[price] += quantity;
    } else {
        asks_[price] += quantity;
    }
}

void OrderBook::remove_from_level(Side side, int64_t price, uint32_t quantity) {
    if (side == Side::BUY) {
        auto it = bids_.find(price);
        if (it != bids_.end()) {
            it->second -= quantity;
            if (it->second == 0) {
                bids_.erase(it);
            }
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end()) {
            it->second -= quantity;
            if (it->second == 0) {
                asks_.erase(it);
            }
        }
    }
}

bool OrderBook::has_crossing(Side side, int64_t price) const {
    if (side == Side::BUY) {
        // Buy order crosses if price >= best ask
        if (!asks_.empty()) {
            return price >= asks_.begin()->first;
        }
    } else {
        // Sell order crosses if price <= best bid
        if (!bids_.empty()) {
            return price <= bids_.begin()->first;
        }
    }
    return false;
}

} // namespace book
