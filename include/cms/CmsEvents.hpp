#pragma once

#include "IEvent.hpp"
#include "IInterfaces.hpp"
#include "RawPacket.hpp"
#include "SystemState.hpp"

#include <string>
#include <vector>

struct CmsOutgoingPacketEvent : public IEvent {
    static const std::string Topic;

    RawPacket packet;
    AckBuilderFunc ackBuilder;
    uint32_t sourceMessageId = 0;

    const std::string& topic() const override { return Topic; }
};

struct CmsAckPacketEvent : public IEvent {
    static const std::string Topic;

    RawPacket ackPacket;

    const std::string& topic() const override { return Topic; }
};

struct CmsStateUpdateEvent : public IEvent {
    static const std::string Topic;

    std::vector<StateUpdate> updates;

    const std::string& topic() const override { return Topic; }
};
