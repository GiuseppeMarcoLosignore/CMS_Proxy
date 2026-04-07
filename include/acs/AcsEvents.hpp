#pragma once

#include "IEvent.hpp"
#include "IInterfaces.hpp"
#include "Topics.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct AcsOutgoingJsonEvent : public IEvent {
    inline static const std::string Topic = Topics::AcsOutgoingJson;

    RawPacket packet;
    nlohmann::json payload;
    uint16_t destinationId = 0;

    const std::string& topic() const override { return Topic; }
};

struct AcsStateUpdateEvent : public IEvent {
    inline static const std::string Topic = Topics::AcsStateUpdate;

    std::vector<StateUpdate> updates;

    const std::string& topic() const override { return Topic; }
};