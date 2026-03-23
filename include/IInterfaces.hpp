#pragma once
#include "RawPacket.hpp"
#include <functional>
#include <memory>

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
    virtual void send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) = 0;
};

class IProtocolConverter {
public:
    virtual ~IProtocolConverter() = default;
    virtual std::vector<RawPacket> convert(const RawPacket& input) = 0;
};