#pragma once

#include "IEvent.hpp"
#include "IInterfaces.hpp"
#include "Topics.hpp"

#include <string>
#include <vector>

struct CmsStateUpdateEvent : public IEvent {
    inline static const std::string Topic = Topics::CmsStateUpdate;

    std::vector<StateUpdate> updates;

    const std::string& topic() const override { return Topic; }
};

struct CmsPeriodicMessageTickEvent : public IEvent {
    inline static const std::string Topic = Topics::CmsPeriodicMessageTick;

    const std::string& topic() const override { return Topic; }
};

struct CmsDispatchTopicPacketEvent : public IEvent {
    std::string dispatchTopic;
    RawPacket packet;
    AckBuilderFunc ackBuilder;
    uint32_t sourceMessageId = 0;

    const std::string& topic() const override { return dispatchTopic; }
};
