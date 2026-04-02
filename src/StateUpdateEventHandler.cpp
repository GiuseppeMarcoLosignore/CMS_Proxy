#include "StateUpdateEventHandler.hpp"

#include "CmsEvents.hpp"

StateUpdateEventHandler::StateUpdateEventHandler(std::shared_ptr<SystemState> systemState,
                                                 std::shared_ptr<EventBus> eventBus)
    : systemState_(std::move(systemState)),
      eventBus_(std::move(eventBus)) {
}

void StateUpdateEventHandler::start() {
    if (!eventBus_) {
        return;
    }

    eventBus_->subscribe(CmsStateUpdateEvent::Topic, [this](const EventBus::EventPtr& event) {
        const auto stateEvent = std::dynamic_pointer_cast<const CmsStateUpdateEvent>(event);
        if (!stateEvent || !systemState_) {
            return;
        }
        systemState_->applyBatch(stateEvent->updates);
    });
}

void StateUpdateEventHandler::stop() {
}
