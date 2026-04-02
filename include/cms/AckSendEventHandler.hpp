#pragma once

#include "EventBus.hpp"
#include "IEntity.hpp"
#include "IInterfaces.hpp"

#include <memory>

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
