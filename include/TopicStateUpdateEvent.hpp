#pragma once

#include "IEvent.hpp"
#include "StateUpdate.hpp"

#include <string>
#include <vector>

struct TopicStateUpdateEvent : public IEvent {
    std::string sourceTopic;
    std::vector<StateUpdate> updates;
    const std::string& topic() const override { return sourceTopic; }
};
