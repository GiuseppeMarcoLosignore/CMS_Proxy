#include "AckSendEventHandler.hpp"

#include "CmsEvents.hpp"

AckSendEventHandler::AckSendEventHandler(std::shared_ptr<IAckSender> ackSender,
                                         std::shared_ptr<EventBus> eventBus)
    : ackSender_(std::move(ackSender)),
      eventBus_(std::move(eventBus)) {
}

void AckSendEventHandler::start() {
    if (!eventBus_) {
        return;
    }

    eventBus_->subscribe(CmsAckPacketEvent::Topic, [this](const EventBus::EventPtr& event) {
        const auto ackEvent = std::dynamic_pointer_cast<const CmsAckPacketEvent>(event);
        if (!ackEvent || !ackSender_) {
            return;
        }
        ackSender_->send_ack(ackEvent->ackPacket);
    });
}

void AckSendEventHandler::stop() {
}
