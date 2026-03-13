#pragma once
#include <vector>
#include <cstdint>
#include <chrono>

struct RawPacket {
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;

    RawPacket() : timestamp(std::chrono::steady_clock::now()) {}
    explicit RawPacket(std::vector<uint8_t> d) 
        : data(std::move(d)), timestamp(std::chrono::steady_clock::now()) {}
};