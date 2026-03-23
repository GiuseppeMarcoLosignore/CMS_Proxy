#pragma once
#include "RawPacket.hpp"
#include <functional>
#include <memory>
#include <string>

struct SendResult {
    bool success = false;
    int error_value = 0;
    std::string error_category;
    std::string error_message;
};

class IReceiver {
public:
    virtual ~IReceiver() = default;
    using MessageCallback = std::function<void(const RawPacket&)>;
    
    virtual void set_callback(MessageCallback cb) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class ISender {
public:
    virtual ~ISender() = default;
    virtual SendResult send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) = 0;
};

class IProtocolConverter {
public:
    virtual ~IProtocolConverter() = default;
    virtual std::vector<RawPacket> convert(const RawPacket& input) = 0;
};