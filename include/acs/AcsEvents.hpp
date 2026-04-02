#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IEvent.hpp"
#include "IEntity.hpp"
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

class AcsJsonSendEventHandler : public IEventHandler {
public:
    AcsJsonSendEventHandler(std::shared_ptr<ISender> sender,
                            std::shared_ptr<EventBus> eventBus,
                            std::map<uint16_t, AcsDestination> destinations);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<ISender> sender_;
    std::shared_ptr<EventBus> eventBus_;
    std::map<uint16_t, AcsDestination> destinations_;
};

class AcsStateUpdateEventHandler : public IEventHandler {
public:
    AcsStateUpdateEventHandler(std::shared_ptr<SystemState> systemState,
                               std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<SystemState> systemState_;
    std::shared_ptr<EventBus> eventBus_;
};