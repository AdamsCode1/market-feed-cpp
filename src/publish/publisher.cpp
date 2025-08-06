/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "publisher.hpp"
#include <sstream>

namespace publish {

TopOfBookPublisher::TopOfBookPublisher(std::ostream& output) 
    : output_(output), header_printed_(false) {
}

void TopOfBookPublisher::publish(uint64_t timestamp_us, const feed::Symbol& symbol, const book::TopOfBook& tob) {
    if (!header_printed_) {
        print_header();
        header_printed_ = true;
    }
    
    output_ << timestamp_us << ","
            << symbol.to_string() << ",";
    
    if (tob.has_bid()) {
        output_ << format_price(tob.best_bid_px) << "," << tob.bid_sz;
    } else {
        output_ << ",";
    }
    
    output_ << ",";
    
    if (tob.has_ask()) {
        output_ << format_price(tob.best_ask_px) << "," << tob.ask_sz;
    } else {
        output_ << ",";
    }
    
    output_ << "\n";
    output_.flush();
}

void TopOfBookPublisher::print_header() {
    output_ << "ts_us,symbol,bid_px,bid_sz,ask_px,ask_sz\n";
}

std::string TopOfBookPublisher::format_price(int64_t price_nano) const {
    // Convert nano-units to decimal representation
    // Assuming 9 decimal places for nano precision
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(9) << (static_cast<double>(price_nano) / 1e9);
    return oss.str();
}

} // namespace publish
