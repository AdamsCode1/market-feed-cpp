/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#pragma once

#include <chrono>
#include <cstdint>

namespace core {

/**
 * @brief High-resolution clock for measuring latency
 */
class Clock {
public:
    /**
     * @brief Get current time in microseconds since epoch
     * @return Current time in microseconds
     */
    static uint64_t now_us() noexcept {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
};

} // namespace core
