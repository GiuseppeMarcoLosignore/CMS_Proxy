#include "AcsEvents.hpp"

#include <iostream>

const std::string AcsIncomingJsonEvent::Topic = "acs.incoming_json";
const std::string AcsOutgoingJsonEvent::Topic = "acs.outgoing_json";
const std::string AcsStateUpdateEvent::Topic = "acs.state_update";

AcsJsonSendEventHandler::AcsJsonSendEventHandler(std::shared_ptr<ISender> sender,
												 std::shared_ptr<EventBus> eventBus,
												 std::map<uint16_t, AcsDestination> destinations)
	: sender_(std::move(sender)),
	  eventBus_(std::move(eventBus)),
	  destinations_(std::move(destinations)) {
}

void AcsJsonSendEventHandler::start() {
	if (!eventBus_) {
		return;
	}

	eventBus_->subscribe(AcsOutgoingJsonEvent::Topic, [this](const EventBus::EventPtr& event) {
		const auto outgoing = std::dynamic_pointer_cast<const AcsOutgoingJsonEvent>(event);
		if (!outgoing || !sender_) {
			return;
		}

		const auto destinationIt = destinations_.find(outgoing->destinationId);
		if (destinationIt == destinations_.end()) {
			std::cerr << "[AcsJsonSendHandler] Destinazione ACS non configurata: "
					  << outgoing->destinationId << std::endl;
			return;
		}

		const auto& destination = destinationIt->second;
		const SendResult result = sender_->send(
			outgoing->packet,
			destination.ip_address,
			destination.port
		);

		if (!result.success) {
			std::cerr << "[AcsJsonSendHandler] Errore invio UDP JSON verso "
					  << destination.ip_address << ":" << destination.port
					  << " -> " << result.error_message << std::endl;
		}
	});
}

void AcsJsonSendEventHandler::stop() {
}

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