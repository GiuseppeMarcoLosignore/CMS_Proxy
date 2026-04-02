#include "CmsEvents.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>

#include <nlohmann/json.hpp>

#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <arpa/inet.h>
#endif

namespace {

constexpr uint32_t MessageId_CS_MULTI_health_status_INS = 1684229565;
constexpr std::size_t CommonHeaderSize = 16;
constexpr std::size_t MessageSize = 24;

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

uint16_t map_cs_status_from_snapshot(const SystemStateSnapshot& snapshot) {
	std::string mode = snapshot.systemMode;
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});

	if (mode.find("TRAIN") != std::string::npos) {
		return 3;
	}
	if (mode.find("OPER") != std::string::npos) {
		return 2;
	}
	return 1;
}

RawPacket build_health_status_packet(const SystemStateSnapshot& snapshot) {
	std::vector<uint8_t> bytes(MessageSize, 0);

	const uint32_t word0 = htonl(MessageId_CS_MULTI_health_status_INS);
	const uint32_t word1 = htonl(static_cast<uint32_t>(MessageSize - CommonHeaderSize));
	const uint32_t word2 = 0;
	const uint32_t word3 = 0;

	std::memcpy(bytes.data() + 0, &word0, sizeof(uint32_t));
	std::memcpy(bytes.data() + 4, &word1, sizeof(uint32_t));
	std::memcpy(bytes.data() + 8, &word2, sizeof(uint32_t));
	std::memcpy(bytes.data() + 12, &word3, sizeof(uint32_t));

	const uint16_t cs_status = htons(map_cs_status_from_snapshot(snapshot));
	const uint16_t drmu_status = htons(static_cast<uint16_t>(1));
	const uint16_t spare = htons(static_cast<uint16_t>(0));
	const uint16_t css_status = htons(static_cast<uint16_t>(1));

	std::memcpy(bytes.data() + 16, &cs_status, sizeof(uint16_t));
	std::memcpy(bytes.data() + 18, &drmu_status, sizeof(uint16_t));
	std::memcpy(bytes.data() + 20, &spare, sizeof(uint16_t));
	std::memcpy(bytes.data() + 22, &css_status, sizeof(uint16_t));

	return RawPacket{std::move(bytes)};
}

} // namespace

//const std::string CmsOutgoingPacketEvent::Topic = "cms.outgoing_packet";
const std::string CmsOutgoingPacketEvent::Topic = "acs.outgoing_json"; // Usare topic ACS per outgoing packet in modo che AckSendEventHandler possa gestire anche gli ack dei relay unicast

const std::string CmsAckPacketEvent::Topic = "cms.ack_packet";
const std::string CmsStateUpdateEvent::Topic = "cms.state_update";
const std::string CmsPeriodicMessageTickEvent::Topic = "cms.periodic_message_tick";
const std::string CmsPeriodicUnicastPacketEvent::Topic = "cms.periodic_unicast_packet";

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

PeriodicHealthStatusBuildEventHandler::PeriodicHealthStatusBuildEventHandler(
	std::shared_ptr<IStateProvider> stateProvider,
	std::shared_ptr<EventBus> eventBus)
	: stateProvider_(std::move(stateProvider)),
	  eventBus_(std::move(eventBus)) {
}

void PeriodicHealthStatusBuildEventHandler::start() {
	if (!eventBus_) {
		return;
	}

	eventBus_->subscribe(CmsPeriodicMessageTickEvent::Topic, [this](const EventBus::EventPtr& event) {
		const auto tickEvent = std::dynamic_pointer_cast<const CmsPeriodicMessageTickEvent>(event);
		if (!tickEvent || !eventBus_) {
			return;
		}

		SystemStateSnapshot snapshot;
		if (stateProvider_) {
			snapshot = stateProvider_->getSnapshot();
		}

		auto packetEvent = std::make_shared<CmsPeriodicUnicastPacketEvent>();
		packetEvent->packet = build_health_status_packet(snapshot);
		eventBus_->publish(packetEvent);
	});
}

void PeriodicHealthStatusBuildEventHandler::stop() {
}

CmsUdpUnicastSendEventHandler::CmsUdpUnicastSendEventHandler(std::shared_ptr<ISender> sender,
															 std::shared_ptr<EventBus> eventBus,
															 std::string targetIp,
															 uint16_t targetPort,
															 bool enabled)
	: sender_(std::move(sender)),
	  eventBus_(std::move(eventBus)),
	  targetIp_(std::move(targetIp)),
	  targetPort_(targetPort),
	  enabled_(enabled) {
}

void CmsUdpUnicastSendEventHandler::start() {
	if (!enabled_) {
		std::cout << "[CMS UDP Unicast] Handler disabilitato da configurazione." << std::endl;
		return;
	}

	if (!eventBus_ || !sender_) {
		std::cerr << "[CMS UDP Unicast] Handler non avviato: dipendenze mancanti." << std::endl;
		return;
	}

	eventBus_->subscribe(CmsPeriodicUnicastPacketEvent::Topic, [this](const EventBus::EventPtr& event) {
		const auto packetEvent = std::dynamic_pointer_cast<const CmsPeriodicUnicastPacketEvent>(event);
		if (!packetEvent) {
			return;
		}

		const SendResult result = sender_->send(packetEvent->packet, targetIp_, targetPort_);
		if (!result.success) {
			std::cerr << "[CMS UDP Unicast] Errore invio: "
					  << result.error_category << " (" << result.error_value << ") "
					  << result.error_message << std::endl;
		}
	});

	std::cout << "[CMS UDP Unicast] Invio periodico attivo su "
			  << targetIp_ << ":" << targetPort_ << std::endl;
}

void CmsUdpUnicastSendEventHandler::stop() {
}
