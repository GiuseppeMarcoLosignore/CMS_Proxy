#include "AcsEntity.hpp"

#include "Topics.hpp"
#include "UdpUnicastReceiver.hpp"
#include "cms/CmsEvents.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

namespace {

std::string extract_message_type(const nlohmann::json& payload) {
    if (!payload.contains("header") || !payload.at("header").is_object()) {
        return {};
    }

    const auto& header = payload.at("header");
    if (!header.contains("type") || !header.at("type").is_string()) {
        return {};
    }

    return header.at("type").get<std::string>();
}

std::optional<uint16_t> extract_destination_id(const nlohmann::json& payload) {
    const char* keys[] = { "destination_id", "target_id", "id" };
    for (const char* key : keys) {
        if (payload.contains(key) && payload.at(key).is_number_unsigned()) {
            return static_cast<uint16_t>(payload.at(key).get<uint32_t>());
        }
    }

    if (payload.contains("header") && payload.at("header").is_object()) {
        const auto& header = payload.at("header");
        for (const char* key : keys) {
            if (header.contains(key) && header.at(key).is_number_unsigned()) {
                return static_cast<uint16_t>(header.at(key).get<uint32_t>());
            }
        }
    }

    return std::nullopt;
}

std::optional<StateUpdate> parse_state_update(const nlohmann::json& payload) {
    const nlohmann::json* source = nullptr;
    if (payload.contains("state_update") && payload.at("state_update").is_object()) {
        source = &payload.at("state_update");
    } else if (payload.contains("state") && payload.at("state").is_object()) {
        source = &payload.at("state");
    }

    if (!source) {
        return std::nullopt;
    }

    StateUpdate update;
    if (source->contains("system_mode") && source->at("system_mode").is_string()) {
        update.systemMode = source->at("system_mode").get<std::string>();
    }
    if (source->contains("lrad_id") && source->at("lrad_id").is_number_unsigned()) {
        update.lradId = static_cast<uint16_t>(source->at("lrad_id").get<uint32_t>());
    }
    if (source->contains("cueing_status") && source->at("cueing_status").is_string()) {
        update.cueingStatus = source->at("cueing_status").get<std::string>();
    }
    if (source->contains("configuration") && source->at("configuration").is_string()) {
        update.configuration = source->at("configuration").get<std::string>();
    }
    if (source->contains("online") && source->at("online").is_boolean()) {
        update.online = source->at("online").get<bool>();
    }
    if (source->contains("engaged") && source->at("engaged").is_boolean()) {
        update.engaged = source->at("engaged").get<bool>();
    }
    if (source->contains("audio_enabled") && source->at("audio_enabled").is_boolean()) {
        update.audioEnabled = source->at("audio_enabled").get<bool>();
    }
    if (source->contains("lad_enabled") && source->at("lad_enabled").is_boolean()) {
        update.ladEnabled = source->at("lad_enabled").get<bool>();
    }
    if (source->contains("lrf_enabled") && source->at("lrf_enabled").is_boolean()) {
        update.lrfEnabled = source->at("lrf_enabled").get<bool>();
    }

    return update;
}

} // namespace

AcsEntity::AcsEntity(const AcsConfig& config,
                                         std::shared_ptr<EventBus> eventBus,
                                         std::shared_ptr<ISender> sender,
                                         std::shared_ptr<SystemState> systemState)
    : config_(config),
      eventBus_(std::move(eventBus)),
            sender_(std::move(sender)),
            systemState_(std::move(systemState)),
            destinations_(config_.destinations),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
}

void AcsEntity::start() {
        subscribeTopics();

    receiver_ = std::make_shared<UdpUnicastReceiver>(
        rxIoContext_,
        config_.listen_ip,
        config_.listen_port
    );

    receiver_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    receiver_->start();

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[ACS Entity] Avviata su "
              << config_.listen_ip << ":" << config_.listen_port << std::endl;
}

void AcsEntity::stop() {
    if (receiver_) {
        receiver_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}

void AcsEntity::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    //eventBus_->subscribe(AcsOutgoingJsonEvent::Topic, [this](const EventBus::EventPtr& event) {
    //    handleOutgoingJsonEvent(event);
    //});

    //eventBus_->subscribe(AcsStateUpdateEvent::Topic, [this](const EventBus::EventPtr& event) {
    //    handleStateUpdateEvent(event);
    //});

    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const EventBus::EventPtr& event) {
        createMASTER(event);
    });
}

void AcsEntity::handleOutgoingJsonEvent(const EventBus::EventPtr& event) {
    const auto outgoing = std::dynamic_pointer_cast<const AcsOutgoingJsonEvent>(event);
    if (!outgoing || !sender_) {
        return;
    }

    const auto destinationIt = destinations_.find(outgoing->destinationId);
    if (destinationIt == destinations_.end()) {
        std::cerr << "[ACS Entity] Destinazione ACS non configurata: "
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
        std::cerr << "[ACS Entity] Errore invio TCP JSON verso "
                  << destination.ip_address << ":" << destination.port
                  << " -> " << result.error_message << std::endl;
    }
}

void AcsEntity::handleStateUpdateEvent(const EventBus::EventPtr& event) {
    const auto stateEvent = std::dynamic_pointer_cast<const AcsStateUpdateEvent>(event);
    if (!stateEvent || !systemState_) {
        return;
    }

    systemState_->applyBatch(stateEvent->updates);
}

void AcsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!eventBus_) {
        return;
    }

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido: " << e.what() << std::endl;
        return;
    }

    const auto destinationId = extract_destination_id(payload);
    if (destinationId.has_value()) {
        auto outgoingEvent = std::make_shared<AcsOutgoingJsonEvent>();
        outgoingEvent->packet = packet;
        outgoingEvent->payload = payload;
        outgoingEvent->destinationId = *destinationId;
        eventBus_->publish(outgoingEvent);
    }

    const auto stateUpdate = parse_state_update(payload);
    if (stateUpdate.has_value()) {
        auto stateEvent = std::make_shared<AcsStateUpdateEvent>();
        stateEvent->updates.push_back(*stateUpdate);
        eventBus_->publish(stateEvent);
    }
}

void AcsEntity::createHeader(std::string header, std::string type, std::string sender, nlohmann::json param, nlohmann::json& outPayload) {
    outPayload["header"] = header;
    outPayload["type"] = type;
    outPayload["sender"] = sender;
    outPayload["param"] = param;
}

void AcsEntity::createMASTER(const EventBus::EventPtr& event) {
    if (!eventBus_ || !sender_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;
    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());

        std::string mode = "REQ";
        if (inputPayload.contains("param") && inputPayload.at("param").is_object()) {
            const auto& inputParam = inputPayload.at("param");
            if (inputParam.contains("mode")) {
                if (inputParam.at("mode").is_number_integer()) {
                    mode = (inputParam.at("mode").get<int>() == 0) ? "RELEASE" : "REQ";
                } else if (inputParam.at("mode").is_string()) {
                    const std::string inputMode = inputParam.at("mode").get<std::string>();
                    mode = (inputMode == "0") ? "RELEASE" : inputMode;
                }
            }
        }

        param["mode"] = mode;
        createHeader("MASTER", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        const SendResult sendResult = sender_->send(
            outPacket,
            destinationIt->second.ip_address,
            destinationIt->second.port
        );
        if (!sendResult.success) {
            std::cerr << "[ACS Entity] Errore invio MASTER verso "
                      << destinationIt->second.ip_address << ":" << destinationIt->second.port
                      << " -> " << sendResult.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createERROR(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createAUDIO(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createLAD(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createSEARCHLIGHT(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createLRF(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createSTABIL(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createSHADOW(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createZOOM(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createCONTEXT(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createPOSITION(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createDELTA(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createTRACKING(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createCONFIG(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createIMU(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createHOURS(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}