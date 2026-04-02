#include "TcpSendEventHandler.hpp"

#include "CmsEvents.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

namespace {

uint32_t extract_action_id_from_payload(const RawPacket& packet) {
    try {
        const auto j = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (!j.contains("param")) {
            return 0;
        }
        const auto& param = j.at("param");
        if (!param.contains("action_id")) {
            return 0;
        }
        return param.at("action_id").get<uint32_t>();
    } catch (...) {
        return 0;
    }
}

} // namespace

TcpSendEventHandler::TcpSendEventHandler(std::shared_ptr<ISender> sender,
                                         std::shared_ptr<EventBus> eventBus,
                                         std::map<uint16_t, LradDestination> lradConfig)
    : sender_(std::move(sender)),
      eventBus_(std::move(eventBus)),
      lradConfig_(std::move(lradConfig)) {
}

void TcpSendEventHandler::start() {
    if (!eventBus_) {
        return;
    }

    eventBus_->subscribe(CmsOutgoingPacketEvent::Topic, [this](const EventBus::EventPtr& event) {
        const auto outgoing = std::dynamic_pointer_cast<const CmsOutgoingPacketEvent>(event);
        if (!outgoing || !sender_) {
            return;
        }

        SendResult sendResult;
        auto destinationIt = lradConfig_.find(outgoing->packet.destinationLradId);
        if (destinationIt != lradConfig_.end()) {
            sendResult = sender_->send(
                outgoing->packet,
                destinationIt->second.ip_address,
                destinationIt->second.port
            );
        } else {
            sendResult.success = false;
            sendResult.error_value = -1;
            sendResult.error_category = "handler";
            sendResult.error_message = "LRAD ID non configurato";
            std::cerr << "[TcpSendHandler] LRAD ID non configurato: "
                      << outgoing->packet.destinationLradId << std::endl;
        }

        auto ackEvent = std::make_shared<CmsAckPacketEvent>();
        const uint32_t actionId = extract_action_id_from_payload(outgoing->packet);
        ackEvent->ackPacket = outgoing->ackBuilder(actionId, outgoing->sourceMessageId, sendResult);
        eventBus_->publish(ackEvent);
    });
}

void TcpSendEventHandler::stop() {
}
