/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#pragma once

#include "messages.hpp"
#include <span>
#include <string>
#include <memory>

namespace feed {

/**
 * @brief Memory-mapped binary feed decoder
 */
class Decoder {
public:
    /**
     * @brief Construct decoder for given file
     * @param filename Path to binary feed file
     */
    explicit Decoder(const std::string& filename);
    
    /**
     * @brief Destructor - unmaps memory
     */
    ~Decoder();
    
    /**
     * @brief Move constructor
     */
    Decoder(Decoder&& other) noexcept;
    
    /**
     * @brief Move assignment operator
     */
    Decoder& operator=(Decoder&& other) noexcept;
    
    // Disable copy
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    
    /**
     * @brief Get total number of bytes in file
     * @return File size in bytes
     */
    size_t size() const noexcept { return file_size_; }
    
    /**
     * @brief Get current position in file
     * @return Current offset in bytes
     */
    size_t position() const noexcept { return current_pos_; }
    
    /**
     * @brief Check if there are more messages to read
     * @return true if more messages available
     */
    bool has_next() const noexcept;
    
    /**
     * @brief Read next message and decode into Event
     * @return Decoded event, or Event with INVALID type if error/EOF
     */
    Event next();
    
    /**
     * @brief Reset decoder to beginning of file
     */
    void reset() noexcept;

private:
    void* mapped_data_;
    size_t file_size_;
    size_t current_pos_;
    int fd_;
    
    template<typename T>
    bool read_message(Event& event);
};

} // namespace feed
