/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#pragma once

#include <atomic>
#include <memory>
#include <cassert>

namespace core {

/**
 * @brief Lock-free single-producer single-consumer ring buffer
 * @tparam T Type of elements stored in the buffer
 */
template<typename T>
class RingBuffer {
public:
    /**
     * @brief Construct ring buffer with given capacity (must be power of 2)
     * @param capacity Buffer capacity (must be power of 2)
     */
    explicit RingBuffer(size_t capacity) 
        : capacity_(capacity), mask_(capacity - 1), buffer_(std::make_unique<T[]>(capacity)) {
        assert((capacity & (capacity - 1)) == 0 && "Capacity must be power of 2");
        assert(capacity > 0);
    }

    /**
     * @brief Try to push an element (producer side)
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    bool try_push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & mask_;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer is full
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to push an element using move semantics (producer side)
     * @param item Item to push
     * @return true if successful, false if buffer is full
     */
    bool try_push(T&& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & mask_;
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Buffer is full
        }
        
        buffer_[current_tail] = std::move(item);
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Try to pop an element (consumer side)
     * @param item Reference to store the popped item
     * @return true if successful, false if buffer is empty
     */
    bool try_pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Buffer is empty
        }
        
        item = std::move(buffer_[current_head]);
        head_.store((current_head + 1) & mask_, std::memory_order_release);
        return true;
    }

    /**
     * @brief Check if buffer is empty
     * @return true if empty
     */
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get approximate size of buffer
     * @return Approximate number of elements
     */
    size_t size() const noexcept {
        const size_t tail = tail_.load(std::memory_order_acquire);
        const size_t head = head_.load(std::memory_order_acquire);
        return (tail - head) & mask_;
    }

    /**
     * @brief Get buffer capacity
     * @return Buffer capacity
     */
    size_t capacity() const noexcept {
        return capacity_;
    }

private:
    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<T[]> buffer_;
    
    alignas(64) std::atomic<size_t> head_{0};  // Consumer index
    alignas(64) std::atomic<size_t> tail_{0};  // Producer index
};

} // namespace core
