#pragma once

#include "EventBus.hpp"
#include "IEntity.hpp"
#include "SystemState.hpp"

#include <memory>

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
