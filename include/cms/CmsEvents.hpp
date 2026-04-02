#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IEvent.hpp"
#include "IEntity.hpp"
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

struct CmsPeriodicMessageTickEvent : public IEvent {
    static const std::string Topic;

    const std::string& topic() const override { return Topic; }
};

struct CmsPeriodicUnicastPacketEvent : public IEvent {
    static const std::string Topic;

    RawPacket packet;

    const std::string& topic() const override { return Topic; }
};

class TcpSendEventHandler : public IEventHandler {
public:
    TcpSendEventHandler(std::shared_ptr<ISender> sender,
                        std::shared_ptr<EventBus> eventBus,
                        std::map<uint16_t, LradDestination> lradConfig);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<ISender> sender_;
    std::shared_ptr<EventBus> eventBus_;
    std::map<uint16_t, LradDestination> lradConfig_;
};

class AckSendEventHandler : public IEventHandler {
public:
    AckSendEventHandler(std::shared_ptr<IAckSender> ackSender,
                        std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<IAckSender> ackSender_;
    std::shared_ptr<EventBus> eventBus_;
};

class StateUpdateEventHandler : public IEventHandler {
public:
    StateUpdateEventHandler(std::shared_ptr<SystemState> systemState,
                            std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<SystemState> systemState_;
    std::shared_ptr<EventBus> eventBus_;
};

class PeriodicHealthStatusBuildEventHandler : public IEventHandler {
public:
    PeriodicHealthStatusBuildEventHandler(std::shared_ptr<IStateProvider> stateProvider,
                                          std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<IStateProvider> stateProvider_;
    std::shared_ptr<EventBus> eventBus_;
};

class CmsUdpUnicastSendEventHandler : public IEventHandler {
public:
    CmsUdpUnicastSendEventHandler(std::shared_ptr<ISender> sender,
                                  std::shared_ptr<EventBus> eventBus,
                                  std::string targetIp,
                                  uint16_t targetPort,
                                  bool enabled);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<ISender> sender_;
    std::shared_ptr<EventBus> eventBus_;
    std::string targetIp_;
    uint16_t targetPort_ = 0;
    bool enabled_ = false;
};
