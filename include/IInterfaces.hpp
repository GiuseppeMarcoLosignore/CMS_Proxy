#pragma once
#include "StateUpdate.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include <vector>
#include <optional>

struct RawPacket {
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
    uint16_t destinationLradId;
    int nackreason = 0;

    RawPacket() : timestamp(std::chrono::steady_clock::now()), destinationLradId(0) {}
    explicit RawPacket(std::vector<uint8_t> d)
        : data(std::move(d)), timestamp(std::chrono::steady_clock::now()), destinationLradId(0) {}
};

class IEntity {
public:
    virtual ~IEntity() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class IEvent {
public:
    virtual ~IEvent() = default;
    virtual const std::string& topic() const = 0;
};

struct SendResult {
    bool success = false;
    int error_value = 0;
    std::string error_category;
    std::string error_message;
};

struct ConversionResult {
    std::optional<RawPacket> packet;
    std::string packet_topic;
    std::vector<StateUpdate> state_updates;
};

enum class TransportProtocol {
    Udp,
    Tcp
};

struct PacketSourceInfo {
    TransportProtocol protocol = TransportProtocol::Udp;
    std::string source_ip;
    uint16_t source_port = 0;
};

class IReceiver {
public:
    virtual ~IReceiver() = default;
    using MessageCallback = std::function<void(const RawPacket&, const PacketSourceInfo&)>;
    
    virtual void set_callback(MessageCallback cb) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class ISender {
public:
    virtual ~ISender() = default;
    virtual SendResult send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) = 0;
};