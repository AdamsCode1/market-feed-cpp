/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#pragma once

#include "order_book.hpp"
#include "messages.hpp"
#include <iostream>
#include <iomanip>

namespace publish {

/**
 * @brief CSV publisher for top-of-book data
 */
class TopOfBookPublisher {
public:
    /**
     * @brief Constructor
     * @param output Output stream (default: std::cout)
     */
    explicit TopOfBookPublisher(std::ostream& output = std::cout);
    
    /**
     * @brief Publish top of book data in CSV format
     * @param timestamp_us Timestamp in microseconds
     * @param symbol Symbol
     * @param tob Top of book data
     */
    void publish(uint64_t timestamp_us, const feed::Symbol& symbol, const book::TopOfBook& tob);
    
    /**
     * @brief Print CSV header
     */
    void print_header();

private:
    std::ostream& output_;
    bool header_printed_;
    
    std::string format_price(int64_t price_nano) const;
};

} // namespace publish
