/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#include "decoder.hpp"
#include "clock.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>

namespace feed {

Decoder::Decoder(const std::string& filename) 
    : mapped_data_(nullptr), file_size_(0), current_pos_(0), fd_(-1) {
    
    // Open file
    fd_ = open(filename.c_str(), O_RDONLY);
    if (fd_ == -1) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    // Get file size
    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        close(fd_);
        throw std::runtime_error("Failed to get file size: " + filename);
    }
    
    file_size_ = sb.st_size;
    if (file_size_ == 0) {
        close(fd_);
        throw std::runtime_error("File is empty: " + filename);
    }
    
    // Memory map the file
    mapped_data_ = mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mapped_data_ == MAP_FAILED) {
        close(fd_);
        throw std::runtime_error("Failed to memory map file: " + filename);
    }
}

Decoder::~Decoder() {
    if (mapped_data_ != nullptr && mapped_data_ != MAP_FAILED) {
        munmap(mapped_data_, file_size_);
    }
    if (fd_ != -1) {
        close(fd_);
    }
}

Decoder::Decoder(Decoder&& other) noexcept 
    : mapped_data_(other.mapped_data_), file_size_(other.file_size_),
      current_pos_(other.current_pos_), fd_(other.fd_) {
    other.mapped_data_ = nullptr;
    other.file_size_ = 0;
    other.current_pos_ = 0;
    other.fd_ = -1;
}

Decoder& Decoder::operator=(Decoder&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        if (mapped_data_ != nullptr && mapped_data_ != MAP_FAILED) {
            munmap(mapped_data_, file_size_);
        }
        if (fd_ != -1) {
            close(fd_);
        }
        
        // Move resources
        mapped_data_ = other.mapped_data_;
        file_size_ = other.file_size_;
        current_pos_ = other.current_pos_;
        fd_ = other.fd_;
        
        // Reset other
        other.mapped_data_ = nullptr;
        other.file_size_ = 0;
        other.current_pos_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

bool Decoder::has_next() const noexcept {
    return current_pos_ < file_size_;
}

Event Decoder::next() {
    if (!has_next()) {
        return Event(); // Invalid event
    }
    
    const char* data = static_cast<const char*>(mapped_data_);
    if (current_pos_ >= file_size_) {
        return Event(); // Invalid event
    }
    
    char msg_type = data[current_pos_];
    Event event;
    event.decode_timestamp_us = core::Clock::now_us();
    
    switch (msg_type) {
        case 'A':
            if (read_message<AddOrderMsg>(event)) {
                event.type = EventType::ADD_ORDER;
                return event;
            }
            break;
        case 'U':
            if (read_message<ModifyOrderMsg>(event)) {
                event.type = EventType::MODIFY_ORDER;
                return event;
            }
            break;
        case 'E':
            if (read_message<ExecuteOrderMsg>(event)) {
                event.type = EventType::EXECUTE_ORDER;
                return event;
            }
            break;
        case 'D':
            if (read_message<DeleteOrderMsg>(event)) {
                event.type = EventType::DELETE_ORDER;
                return event;
            }
            break;
        default:
            // Skip unknown message type
            current_pos_++;
            return next();
    }
    
    return Event(); // Invalid event
}

void Decoder::reset() noexcept {
    current_pos_ = 0;
}

template<typename T>
bool Decoder::read_message(Event& event) {
    if (current_pos_ + sizeof(T) > file_size_) {
        return false; // Not enough data
    }
    
    const char* data = static_cast<const char*>(mapped_data_);
    const T* msg = reinterpret_cast<const T*>(data + current_pos_);
    
    // Validate message
    if constexpr (std::is_same_v<T, AddOrderMsg>) {
        if (msg->side != 'B' && msg->side != 'S') {
            return false;
        }
        if (msg->qty == 0) {
            return false;
        }
        event.payload.add = *msg;
    } else if constexpr (std::is_same_v<T, ModifyOrderMsg>) {
        if (msg->new_qty == 0) {
            return false;
        }
        event.payload.modify = *msg;
    } else if constexpr (std::is_same_v<T, ExecuteOrderMsg>) {
        if (msg->exec_qty == 0) {
            return false;
        }
        event.payload.execute = *msg;
    } else if constexpr (std::is_same_v<T, DeleteOrderMsg>) {
        event.payload.delete_order = *msg;
    }
    
    current_pos_ += sizeof(T);
    return true;
}

} // namespace feed
