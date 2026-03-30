#pragma once
#include "RawPacket.hpp"
#include "SystemState.hpp"
#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include <vector>
#include <optional>

struct SendResult {
    bool success = false;
    int error_value = 0;
    std::string error_category;
    std::string error_message;
};

using AckBuilderFunc = std::function<RawPacket(uint32_t action_id, uint32_t source_message_id, const SendResult&)>;

struct ConversionResult {
    std::vector<RawPacket> packets;
    AckBuilderFunc ack_builder;
    bool ack_only = false;
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

class IProtocolConverter {
public:
    virtual ~IProtocolConverter() = default;
    virtual ConversionResult convert(const RawPacket& input, const SystemStateSnapshot& snapshot) = 0;
};

class IAckSender {
public:
    virtual ~IAckSender() = default;
    virtual void send_ack(const RawPacket& ack_packet) = 0;
};