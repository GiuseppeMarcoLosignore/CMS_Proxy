#pragma once

#include "IEvent.hpp"
#include "IInterfaces.hpp"
#include "RawPacket.hpp"
#include "SystemState.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct AcsIncomingJsonEvent : public IEvent {
    static const std::string Topic;

    RawPacket packet;
    PacketSourceInfo sourceInfo;
    nlohmann::json payload;
    std::string messageType;

    const std::string& topic() const override { return Topic; }
};

struct AcsOutgoingJsonEvent : public IEvent {
    static const std::string Topic;

    RawPacket packet;
    nlohmann::json payload;
    uint16_t destinationId = 0;

    const std::string& topic() const override { return Topic; }
};

struct AcsStateUpdateEvent : public IEvent {
    static const std::string Topic;

    std::vector<StateUpdate> updates;

    const std::string& topic() const override { return Topic; }
};