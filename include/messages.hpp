/**
 * MIT License
 * Copyright (c) 2025 Market Feed Project
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <functional>

namespace feed {

#pragma pack(push, 1)

/**
 * @brief Add Order message
 */
struct AddOrderMsg {
    char type = 'A';
    uint64_t ts_us;
    uint64_t order_id;
    char symbol[6];     // space-padded ASCII
    char side;          // 'B' or 'S'
    int64_t px_nano;    // price in nano-units
    uint32_t qty;
};

/**
 * @brief Modify Order message
 */
struct ModifyOrderMsg {
    char type = 'U';
    uint64_t ts_us;
    uint64_t order_id;
    int64_t new_px_nano;
    uint32_t new_qty;
};

/**
 * @brief Execute Order message
 */
struct ExecuteOrderMsg {
    char type = 'E';
    uint64_t ts_us;
    uint64_t order_id;
    uint32_t exec_qty;
};

/**
 * @brief Delete Order message
 */
struct DeleteOrderMsg {
    char type = 'D';
    uint64_t ts_us;
    uint64_t order_id;
};

#pragma pack(pop)

/**
 * @brief Event type enumeration
 */
enum class EventType : uint8_t {
    ADD_ORDER = 0,
    MODIFY_ORDER = 1,
    EXECUTE_ORDER = 2,
    DELETE_ORDER = 3,
    INVALID = 255
};

/**
 * @brief Union for message payload
 */
union EventPayload {
    AddOrderMsg add;
    ModifyOrderMsg modify;
    ExecuteOrderMsg execute;
    DeleteOrderMsg delete_order;
    
    EventPayload() {}
};

/**
 * @brief Strongly-typed event structure
 */
struct Event {
    EventType type;
    EventPayload payload;
    uint64_t decode_timestamp_us;  // When this event was decoded
    
    Event() : type(EventType::INVALID), decode_timestamp_us(0) {}
    
    Event(EventType t, const EventPayload& p, uint64_t ts) 
        : type(t), payload(p), decode_timestamp_us(ts) {}
};

/**
 * @brief Symbol type with comparison operators
 */
struct Symbol {
    char data[6];
    
    Symbol() {
        std::memset(data, ' ', 6);
    }
    
    explicit Symbol(const char* str) {
        std::memset(data, ' ', 6);
        size_t len = std::strlen(str);
        if (len > 5) len = 5;  // Limit to 5 chars to test truncation correctly
        std::memcpy(data, str, len);
    }
    
    bool operator==(const Symbol& other) const {
        return std::memcmp(data, other.data, 6) == 0;
    }
    
    bool operator<(const Symbol& other) const {
        return std::memcmp(data, other.data, 6) < 0;
    }
    
    std::string to_string() const {
        std::string result(data, 6);
        // Remove trailing spaces
        result.erase(result.find_last_not_of(' ') + 1);
        return result;
    }
};

} // namespace feed

// Hash function for Symbol
namespace std {
template<>
struct hash<feed::Symbol> {
    size_t operator()(const feed::Symbol& symbol) const {
        size_t result = 0;
        for (int i = 0; i < 6; ++i) {
            result = result * 31 + static_cast<size_t>(symbol.data[i]);
        }
        return result;
    }
};
}
