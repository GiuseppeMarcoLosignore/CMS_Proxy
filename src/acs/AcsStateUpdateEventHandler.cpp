#include "AcsStateUpdateEventHandler.hpp"

#include "AcsEvents.hpp"

AcsStateUpdateEventHandler::AcsStateUpdateEventHandler(std::shared_ptr<SystemState> systemState,
                                                       std::shared_ptr<EventBus> eventBus)
    : systemState_(std::move(systemState)),
      eventBus_(std::move(eventBus)) {
}

void AcsStateUpdateEventHandler::start() {
    if (!eventBus_) {
        return;
    }

    eventBus_->subscribe(AcsStateUpdateEvent::Topic, [this](const EventBus::EventPtr& event) {
        const auto stateEvent = std::dynamic_pointer_cast<const AcsStateUpdateEvent>(event);
        if (!stateEvent || !systemState_) {
            return;
        }

        systemState_->applyBatch(stateEvent->updates);
    });
}

void AcsStateUpdateEventHandler::stop() {
}