#include "AcsEntity.hpp"

#include "UdpUnicastReceiver.hpp"

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
                     std::shared_ptr<EventBus> eventBus)
    : config_(config),
      eventBus_(std::move(eventBus)),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
}

void AcsEntity::start() {
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

void AcsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
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

    auto incomingEvent = std::make_shared<AcsIncomingJsonEvent>();
    incomingEvent->packet = packet;
    incomingEvent->sourceInfo = sourceInfo;
    incomingEvent->payload = payload;
    incomingEvent->messageType = extract_message_type(payload);
    eventBus_->publish(incomingEvent);

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