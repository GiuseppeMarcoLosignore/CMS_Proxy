#include "CmsEntity.hpp"

#include "acs/AcsEntity.hpp"
#include "CueingMath.hpp"
#include "Topics.hpp"
#include "UdpMulticastReceiver.hpp"
#include "UdpMulticastSender.hpp"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

namespace {

using json = nlohmann::json;

constexpr std::size_t HeaderSize = 16;
constexpr uint32_t MessageId_LRAS_CS_ack_INS = 576879045;
constexpr uint32_t MessageId_CS_LRAS_change_configuration_order_INS = 1679949825;
constexpr uint32_t MessageId_CS_LRAS_cueing_order_cancellation_INS = 1679949826;
constexpr uint32_t MessageId_CS_LRAS_cueing_order_INS = 1679949827;
constexpr uint32_t MessageId_CS_LRAS_emission_control_INS = 1679949828;
constexpr uint32_t MessageId_CS_LRAS_emission_mode_INS = 1679949829;
constexpr uint32_t MessageId_CS_LRAS_inhibition_sectors_INS = 1679949830;
constexpr uint32_t MessageId_CS_LRAS_joystick_control_lrad_1_INS = 1679949831;
constexpr uint32_t MessageId_CS_LRAS_joystick_control_lrad_2_INS = 1679949832;
constexpr uint32_t MessageId_CS_LRAS_recording_command_INS = 1679949833;
constexpr uint32_t MessageId_CS_LRAS_request_engagement_capability_INS = 1679949834;
constexpr uint32_t MessageId_CS_LRAS_request_full_status_INS = 1679949835;
constexpr uint32_t MessageId_CS_LRAS_request_message_table_INS = 1679949836;
constexpr uint32_t MessageId_CS_LRAS_request_software_version_INS = 1679949837;
constexpr uint32_t MessageId_CS_LRAS_request_thresholds_INS = 1679949838;
constexpr uint32_t MessageId_CS_LRAS_request_translation_INS = 1679949839;
constexpr uint32_t MessageId_CS_LRAS_video_tracking_command_INS = 1679949840;
constexpr uint32_t MessageId_CS_LRAS_request_emission_mode_INS = 1679949841;
constexpr uint32_t MessageId_CS_LRAS_request_installation_data_INS = 1679949842;
constexpr uint32_t MessageId_CS_MULTI_health_status_INS = 1684229565;
constexpr uint32_t MessageId_CS_MULTI_update_cst_kinematics_INS = 1684229569;

float normalize_0_360(float angleDeg) {
    return cueing::mod360(angleDeg);
}

uint16_t encode_delta_u16(float angleDeg) {
    const float normalized = normalize_0_360(angleDeg);
    const int rounded = static_cast<int>(std::lround(normalized));
    return static_cast<uint16_t>(rounded & 0xFFFF);
}

void append_u32_be(std::vector<uint8_t>& buffer, uint32_t value) {
    const uint32_t netValue = htonl(value);
    const auto* bytes = reinterpret_cast<const uint8_t*>(&netValue);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(netValue));
}

void append_u16_be(std::vector<uint8_t>& buffer, uint16_t value) {
    const uint16_t netValue = htons(value);
    const auto* bytes = reinterpret_cast<const uint8_t*>(&netValue);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(netValue));
}

uint32_t source_message_id_from_topic(const std::string& topic) {
    if (topic == Topics::CS_LRAS_change_configuration_order_INS) {
        return MessageId_CS_LRAS_change_configuration_order_INS;
    }

    if (topic == Topics::CS_LRAS_cueing_order_cancellation_INS) {
        return MessageId_CS_LRAS_cueing_order_cancellation_INS;
    }

    if (topic == Topics::CS_LRAS_cueing_order_INS) {
        return MessageId_CS_LRAS_cueing_order_INS;
    }

    if (topic == Topics::CS_LRAS_emission_control_INS) {
        return MessageId_CS_LRAS_emission_control_INS;
    }

    if (topic == Topics::CS_LRAS_emission_mode_INS) {
        return MessageId_CS_LRAS_emission_mode_INS;
    }

    if (topic == Topics::CS_LRAS_inhibition_sectors_INS) {
        return MessageId_CS_LRAS_inhibition_sectors_INS;
    }

    if (topic == Topics::CS_LRAS_joystick_control_lrad_1_INS) {
        return MessageId_CS_LRAS_joystick_control_lrad_1_INS;
    }

    if (topic == Topics::CS_LRAS_joystick_control_lrad_2_INS) {
        return MessageId_CS_LRAS_joystick_control_lrad_2_INS;
    }

    if (topic == Topics::CS_LRAS_recording_command_INS) {
        return MessageId_CS_LRAS_recording_command_INS;
    }

    if (topic == Topics::CS_LRAS_request_engagement_capability_INS) {
        return MessageId_CS_LRAS_request_engagement_capability_INS;
    }

    if (topic == Topics::CS_LRAS_request_full_status_INS) {
        return MessageId_CS_LRAS_request_full_status_INS;
    }

    if (topic == Topics::CS_LRAS_request_message_table_INS) {
        return MessageId_CS_LRAS_request_message_table_INS;
    }

    if (topic == Topics::CS_LRAS_request_software_version_INS) {
        return MessageId_CS_LRAS_request_software_version_INS;
    }

    if (topic == Topics::CS_LRAS_request_thresholds_INS) {
        return MessageId_CS_LRAS_request_thresholds_INS;
    }

    if (topic == Topics::CS_LRAS_request_translation_INS) {
        return MessageId_CS_LRAS_request_translation_INS;
    }

    if (topic == Topics::CS_LRAS_video_tracking_command_INS) {
        return MessageId_CS_LRAS_video_tracking_command_INS;
    }

    if (topic == Topics::CS_LRAS_request_emission_mode_INS) {
        return MessageId_CS_LRAS_request_emission_mode_INS;
    }

    if (topic == Topics::CS_LRAS_request_installation_data_INS) {
        return MessageId_CS_LRAS_request_installation_data_INS;
    }

    if (topic == Topics::CS_MULTI_health_status_INS) {
        return MessageId_CS_MULTI_health_status_INS;
    }

    if (topic == Topics::CS_MULTI_update_cst_kinematics_INS) {
        return MessageId_CS_MULTI_update_cst_kinematics_INS;
    }

    return 0;
}

std::optional<uint32_t> json_u32_value(const json& value) {
    if (value.is_number_unsigned()) {
        return value.get<uint32_t>();
    }

    if (value.is_number_integer()) {
        const auto signedValue = value.get<int64_t>();
        if (signedValue >= 0 && signedValue <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
            return static_cast<uint32_t>(signedValue);
        }
        return std::nullopt;
    }

    if (value.is_string()) {
        try {
            return static_cast<uint32_t>(std::stoul(value.get<std::string>()));
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<uint32_t> extract_action_id(const json& payload) {
    if (payload.contains("Action Id")) {
        if (const auto actionId = json_u32_value(payload.at("Action Id")); actionId.has_value()) {
            return actionId;
        }
    }

    if (payload.contains("action_id")) {
        if (const auto actionId = json_u32_value(payload.at("action_id")); actionId.has_value()) {
            return actionId;
        }
    }

    if (payload.contains("meta") && payload.at("meta").is_object() && payload.at("meta").contains("action_id")) {
        if (const auto actionId = json_u32_value(payload.at("meta").at("action_id")); actionId.has_value()) {
            return actionId;
        }
    }

    if (payload.contains("param") && payload.at("param").is_object() && payload.at("param").contains("action_id")) {
        if (const auto actionId = json_u32_value(payload.at("param").at("action_id")); actionId.has_value()) {
            return actionId;
        }
    }

    return std::nullopt;
}


uint32_t read_u32_be(const std::vector<uint8_t>& data, std::size_t offset) {
    if (data.size() < offset + sizeof(uint32_t)) {
        return 0;
    }
    uint32_t net_value = 0;
    std::memcpy(&net_value, data.data() + offset, sizeof(uint32_t));
    return ntohl(net_value);
}

uint16_t read_u16_be(const std::vector<uint8_t>& data, std::size_t offset) {
    uint16_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(uint16_t));
    return ntohs(value);
}

float read_f32_be(const std::vector<uint8_t>& data, std::size_t offset) {
    uint32_t raw = read_u32_be(data, offset);
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(float));
    return value;
}

uint32_t extract_message_id_from_header(const RawPacket& packet) {
    return read_u32_be(packet.data, 0);
}

RawPacket make_empty_packet() {
    return RawPacket{};
}

} // namespace

CmsEntity::CmsEntity(const CmsConfig& config,
                     std::shared_ptr<EventBus> eventBus,
                     std::shared_ptr<SystemState> systemState)
    : config_(config),
      eventBus_(std::move(eventBus)),
      systemState_(std::move(systemState)),
      rxIoContext_(),
    rxWorkGuard_(std::nullopt) {
}

void CmsEntity::start() {
    subscribeTopics();

    receiver_ = std::make_shared<UdpMulticastReceiver>(
        rxIoContext_,
        config_.listen_ip,
        config_.multicast_group,
        config_.multicast_port
    );

    receiver_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    receiver_->start();

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[CMS Entity] Avviata su "
              << config_.multicast_group << ":" << config_.multicast_port << std::endl;
}

void CmsEntity::stop() {
    if (receiver_) {
        receiver_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}

void CmsEntity::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    eventBus_->subscribe(Topics::CmsStateUpdate, [this](const EventBus::EventPtr& event) {
        handleStateUpdateEvent(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_CS_ack_INS(event);
    });



}

void CmsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!eventBus_) {
        return;
    }

    const uint32_t sourceMessageId = extract_message_id_from_header(packet);
    const SystemStateSnapshot snapshot = systemState_ ? systemState_->getSnapshot() : SystemStateSnapshot{};
    const ConversionResult result = convertIncomingPacket(packet, snapshot);

    if (result.packets.empty()) {
        std::cerr << "[CMS Entity] Messaggio ignorato: source_id=" << sourceMessageId << std::endl;
        return;
    }

    for (std::size_t i = 0; i < result.packets.size(); ++i) {
        const auto& packetToSend = result.packets[i];

        auto acsOutgoingEvent = std::make_shared<AcsOutgoingJsonEvent>();
        acsOutgoingEvent->packet = packetToSend;
        acsOutgoingEvent->destinationId = packetToSend.destinationLradId;
        eventBus_->publish(acsOutgoingEvent);

        if (i < result.packet_topics.size() && !result.packet_topics[i].empty()) {
            auto dispatchTopicEvent = std::make_shared<CmsDispatchTopicPacketEvent>();
            dispatchTopicEvent->dispatchTopic = result.packet_topics[i];
            dispatchTopicEvent->packet = packetToSend;
            eventBus_->publish(dispatchTopicEvent);
        }
    }

    if (!result.state_updates.empty()) {
        auto stateEvent = std::make_shared<CmsStateUpdateEvent>();
        stateEvent->updates = result.state_updates;
        eventBus_->publish(stateEvent);
    }
}

void CmsEntity::handleStateUpdateEvent(const EventBus::EventPtr& event) {
    const auto stateEvent = std::dynamic_pointer_cast<const CmsStateUpdateEvent>(event);
    if (!stateEvent || !systemState_) {
        return;
    }

    systemState_->applyBatch(stateEvent->updates);
}

bool CmsEntity::parseHeader(const RawPacket& packet, ParsedHeader& out) const {
    if (packet.data.size() < HeaderSize) {
        return false;
    }

    out.messageId = read_u32_be(packet.data, 0);
    out.messageLength = static_cast<uint16_t>(read_u32_be(packet.data, 4) & 0xFFFF);
    return packet.data.size() >= HeaderSize + out.messageLength;
}

ConversionResult CmsEntity::convertIncomingPacket(const RawPacket& packet, const SystemStateSnapshot&) const {
    using ParserFn = std::vector<RawPacket> (CmsEntity::*)(
        const RawPacket&,
        std::vector<StateUpdate>&) const;

    struct ParserBinding {
        ParserFn parser;
        const char* topic;
    };

    static const std::unordered_map<uint32_t, ParserBinding> additionalParserBindings = {
        { MessageId_CS_LRAS_emission_mode_INS, { &CmsEntity::parse_CS_LRAS_emission_mode_INS, Topics::CS_LRAS_emission_mode_INS } },
        { MessageId_CS_LRAS_inhibition_sectors_INS, { &CmsEntity::parse_CS_LRAS_inhibition_sectors_INS, Topics::CS_LRAS_inhibition_sectors_INS } },
        { MessageId_CS_LRAS_joystick_control_lrad_1_INS, { &CmsEntity::parse_CS_LRAS_joystick_control_lrad_1_INS, Topics::CS_LRAS_joystick_control_lrad_1_INS } },
        { MessageId_CS_LRAS_joystick_control_lrad_2_INS, { &CmsEntity::parse_CS_LRAS_joystick_control_lrad_2_INS, Topics::CS_LRAS_joystick_control_lrad_2_INS } },
        { MessageId_CS_LRAS_recording_command_INS, { &CmsEntity::parse_CS_LRAS_recording_command_INS, Topics::CS_LRAS_recording_command_INS } },
        { MessageId_CS_LRAS_request_emission_mode_INS, { &CmsEntity::parse_CS_LRAS_request_emission_mode_INS, Topics::CS_LRAS_request_emission_mode_INS } },
        { MessageId_CS_LRAS_request_engagement_capability_INS, { &CmsEntity::parse_CS_LRAS_request_engagement_capability_INS, Topics::CS_LRAS_request_engagement_capability_INS } },
        { MessageId_CS_LRAS_request_full_status_INS, { &CmsEntity::parse_CS_LRAS_request_full_status_INS, Topics::CS_LRAS_request_full_status_INS } },
        { MessageId_CS_LRAS_request_installation_data_INS, { &CmsEntity::parse_CS_LRAS_request_installation_data_INS, Topics::CS_LRAS_request_installation_data_INS } },
        { MessageId_CS_LRAS_request_message_table_INS, { &CmsEntity::parse_CS_LRAS_request_message_table_INS, Topics::CS_LRAS_request_message_table_INS } },
        { MessageId_CS_LRAS_request_software_version_INS, { &CmsEntity::parse_CS_LRAS_request_software_version_INS, Topics::CS_LRAS_request_software_version_INS } },
        { MessageId_CS_LRAS_request_thresholds_INS, { &CmsEntity::parse_CS_LRAS_request_thresholds_INS, Topics::CS_LRAS_request_thresholds_INS } },
        { MessageId_CS_LRAS_request_translation_INS, { &CmsEntity::parse_CS_LRAS_request_translation_INS, Topics::CS_LRAS_request_translation_INS } },
        { MessageId_CS_LRAS_video_tracking_command_INS, { &CmsEntity::parse_CS_LRAS_video_tracking_command_INS, Topics::CS_LRAS_video_tracking_command_INS } },
        { MessageId_CS_MULTI_health_status_INS, { &CmsEntity::parse_CS_MULTI_health_status_INS, Topics::CS_MULTI_health_status_INS } },
        { MessageId_CS_MULTI_update_cst_kinematics_INS, { &CmsEntity::parse_CS_MULTI_update_cst_kinematics_INS, Topics::CS_MULTI_update_cst_kinematics_INS } }
    };

    ParsedHeader header;
    if (!parseHeader(packet, header)) {
        return {};
    }

    ConversionResult result;

    switch (header.messageId) {
        case MessageId_CS_LRAS_change_configuration_order_INS:
            result.packets = parse_CS_LRAS_change_configuration_order_INS(packet, result.state_updates);
            result.packet_topics.assign(result.packets.size(), Topics::CS_LRAS_change_configuration_order_INS);
            break;
        case MessageId_CS_LRAS_cueing_order_cancellation_INS:
            result.packets = parse_CS_LRAS_cueing_order_cancellation_INS(packet, result.state_updates);
            result.packet_topics.assign(result.packets.size(), Topics::CS_LRAS_cueing_order_cancellation_INS);
            break;
        case MessageId_CS_LRAS_cueing_order_INS:
            result.packets = parse_CS_LRAS_cueing_order_INS(packet, result.state_updates);
            result.packet_topics.assign(result.packets.size(), Topics::CS_LRAS_cueing_order_INS);
            break;
        case MessageId_CS_LRAS_emission_control_INS:
            result.packets = parse_CS_LRAS_emission_control_INS(packet, result.state_updates);
            result.packet_topics.assign(result.packets.size(), Topics::CS_LRAS_emission_control_INS);
            break;
        default:
            {
                const auto bindingIt = additionalParserBindings.find(header.messageId);
                if (bindingIt != additionalParserBindings.end()) {
                    result.packets = (this->*(bindingIt->second.parser))(packet, result.state_updates);
                    result.packet_topics.assign(result.packets.size(), bindingIt->second.topic);
                }
            }
            break;
    }

    return result;
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_change_configuration_order_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>& stateUpdates) const {
    std::vector<RawPacket> results;
    constexpr std::size_t offset = 16;
    constexpr std::size_t blockSize = 8;

    std::size_t currentOffset = offset;
    while (currentOffset + blockSize <= packet.data.size()) {
        const uint16_t actionId = read_u16_be(packet.data, currentOffset);
        const uint16_t lradId = read_u16_be(packet.data, currentOffset + 4);
        const uint16_t rawConfig = read_u16_be(packet.data, currentOffset + 6);

        json payload;
        payload["Action Id"] = std::to_string(actionId);
        payload["LRAD ID"] = std::to_string(lradId);
        payload["Configuration"] = std::to_string(rawConfig);


        const std::string jsonString = payload.dump();
        RawPacket converted;
        converted.data.assign(jsonString.begin(), jsonString.end());
        converted.destinationLradId = lradId;
        results.push_back(converted);

        StateUpdate update;
        update.lradId = lradId;
        update.engaged = (rawConfig != 0);
        stateUpdates.push_back(update);
        currentOffset += blockSize;
    }

    return results;
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_cueing_order_cancellation_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    std::vector<RawPacket> results;
    constexpr std::size_t offset = 16;
    constexpr std::size_t blockSize = 6;
    if (offset + blockSize > packet.data.size()) {
        return results;
    }

    const uint16_t lradId = read_u16_be(packet.data, offset + 4);

    json payload;
    payload["LRAD ID"] = std::to_string(lradId);

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;
    results.push_back(converted);
    return results;
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_cueing_order_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    std::vector<RawPacket> results;
    constexpr std::size_t minPayloadSize = 22;
    if (packet.data.size() < HeaderSize + minPayloadSize) {
        return results;
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t cueingType = read_u16_be(packet.data, 22);
    const uint32_t cstn = read_u32_be(packet.data, 24);
    const uint16_t kinematicsType = read_u16_be(packet.data, 36);

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool hasCartesianCoordinates = false;

    switch (kinematicsType) {
        case 1:
        case 2:
            if (packet.data.size() >= 52) {
                x = read_f32_be(packet.data, 40);
                y = read_f32_be(packet.data, 44);
                z = read_f32_be(packet.data, 48);
                hasCartesianCoordinates = true;
            }
            break;
        case 3:
        case 4:
            if (packet.data.size() >= 48) {
                x = read_f32_be(packet.data, 40);
                y = read_f32_be(packet.data, 44);
                hasCartesianCoordinates = true;
            }
            break;
        default:
            break;
    }

    float azimuthDeg = 0.0f;
    float elevationDeg = 0.0f;
    if (hasCartesianCoordinates) {
        float range = 0.0f;
        float azAbs = 0.0f;
        cueing::cartesian2target(
            x,
            y,
            z,
            azimuthDeg,
            elevationDeg,
            range,
            false,
            azAbs,
            0.0f,
            0.0f,
            0.0f,
            0.0f);
    }

    json payload;
    if (cueingType == 1) {
        payload["header"] = "MOVE";
        payload["type"] = "CMD";
        payload["sender"] = "CC";
        payload["param"] = {
            {"goTo", hasCartesianCoordinates ? "ABS" : "HOME"},
            {"az", normalize_0_360(azimuthDeg)},
            {"el", normalize_0_360(elevationDeg)}
        };
    } else {
        payload["header"] = "DELTA";
        payload["type"] = "CMD";
        payload["sender"] = "CC";
        payload["param"] = {
            {"az", encode_delta_u16(azimuthDeg)},
            {"el", encode_delta_u16(elevationDeg)}
        };
    }

    payload["meta"] = {
        {"action_id", actionId},
        {"lrad_id", lradId},
        {"cueing_type", cueingType},
        {"cstn", cstn},
        {"kinematics_type", kinematicsType}
    };

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;
    results.push_back(converted);
    return results;
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_emission_control_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    std::vector<RawPacket> results;
    if (packet.data.size() < 838) {
        return results;
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t audioModeValidity = read_u16_be(packet.data, 22);
    const uint16_t volumeLevel = read_u16_be(packet.data, 24);
    const float audioVolumeDb = read_f32_be(packet.data, 26);
    const uint16_t mute = read_u16_be(packet.data, 30);
    const uint16_t audioMode = read_u16_be(packet.data, 32);
    const uint32_t recordedMessageId = read_u32_be(packet.data, 34);
    const uint16_t recordedLanguage = read_u16_be(packet.data, 38);
    const uint16_t recordedLoop = read_u16_be(packet.data, 40);
    const uint16_t freeTextLanguageIn = read_u16_be(packet.data, 42);
    const uint16_t freeTextLanguageOut = read_u16_be(packet.data, 44);

    std::string freeTextMessage;
    freeTextMessage.reserve(768);
    for (std::size_t i = 46; i < 814; ++i) {
        const char c = static_cast<char>(packet.data[i]);
        if (c == '\0') {
            break;
        }
        freeTextMessage.push_back(c);
    }

    const uint16_t freeTextLoop = read_u16_be(packet.data, 814);
    const uint16_t laserModeValidity = read_u16_be(packet.data, 816);
    const uint16_t laserMode = read_u16_be(packet.data, 818);
    const uint16_t lightModeValidity = read_u16_be(packet.data, 820);
    const uint16_t lightPower = read_u16_be(packet.data, 822);
    const uint16_t lightZoom = read_u16_be(packet.data, 824);
    const uint16_t lrfModeValidity = read_u16_be(packet.data, 826);
    const uint16_t lrfOnOff = read_u16_be(packet.data, 828);
    const uint16_t cameraZoomValidity = read_u16_be(packet.data, 830);
    const uint16_t cameraZoom = read_u16_be(packet.data, 832);
    const uint16_t horizontalReferenceValidity = read_u16_be(packet.data, 834);
    const uint16_t horizontalReference = read_u16_be(packet.data, 836);

    json payload;
    payload["header"] = "EMISS";
    payload["type"] = "CMD";
    payload["sender"] = "CMS";
    payload["message_name"] = "CS_LRAS_emission_control_INS";
    payload["message_id"] = MessageId_CS_LRAS_emission_control_INS;
    payload["param"] = {
        {"action_id", actionId},
        {"lrad_id", lradId},
        {"audio_mode_validity", audioModeValidity},
        {"audio_mode", {
            {"volume_mode", {
                {"level", volumeLevel},
                {"audio_volume_db", audioVolumeDb},
                {"mute", mute},
                {"audio_mode", audioMode}
            }},
            {"recorded_message_tone", {
                {"message_id", recordedMessageId},
                {"language", recordedLanguage},
                {"loop", recordedLoop}
            }},
            {"free_text", {
                {"text", {
                    {"language_in", freeTextLanguageIn},
                    {"language_out", freeTextLanguageOut},
                    {"message_text", freeTextMessage}
                }},
                {"loop", freeTextLoop}
            }}
        }},
        {"laser_mode_validity", laserModeValidity},
        {"laser_mode", laserMode},
        {"light_mode_validity", lightModeValidity},
        {"light_mode", {
            {"light_power", lightPower},
            {"light_zoom", lightZoom}
        }},
        {"lrf_mode_validity", lrfModeValidity},
        {"lrf_on_off", lrfOnOff},
        {"camera_zoom_validity", cameraZoomValidity},
        {"camera_zoom", cameraZoom},
        {"horizontal_reference_validity", horizontalReferenceValidity},
        {"horizontal_reference", horizontalReference}
    };

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;
    results.push_back(converted);
    return results;
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_emission_mode_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_inhibition_sectors_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_joystick_control_lrad_1_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_joystick_control_lrad_2_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_recording_command_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_emission_mode_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_engagement_capability_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_full_status_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_installation_data_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_message_table_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_software_version_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_thresholds_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_request_translation_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_LRAS_video_tracking_command_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_MULTI_health_status_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

std::vector<RawPacket> CmsEntity::parse_CS_MULTI_update_cst_kinematics_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return { make_empty_packet() };
}

void CmsEntity::sendLRAS_CS_ack_INS(const EventBus::EventPtr& event) const {
    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const uint32_t sourceMessageId = source_message_id_from_topic(dispatchEvent->dispatchTopic);
    if (sourceMessageId == 0) {
        std::cerr << "[CMS Entity] Impossibile determinare source_message_id per ACK: topic="
                  << dispatchEvent->dispatchTopic << std::endl;
        return;
    }

    json payload;
    try {
        payload = json::parse(dispatchEvent->packet.data.begin(), dispatchEvent->packet.data.end());
    } catch (const std::exception& e) {
        std::cerr << "[CMS Entity] Payload non valido per ACK LRAS_CS_ack_INS: "
                  << e.what() << std::endl;
        return;
    }

    const auto actionId = extract_action_id(payload);
    if (!actionId.has_value()) {
        std::cerr << "[CMS Entity] Action Id mancante nel payload per ACK LRAS_CS_ack_INS"
                  << std::endl;
        return;
    }

    constexpr uint16_t ackNackAccepted = 1;
    constexpr uint16_t nackReasonNone = 0;
    constexpr uint32_t payloadLength = 12;

    RawPacket ackPacket;
    ackPacket.data.reserve(HeaderSize + payloadLength);
    append_u32_be(ackPacket.data, MessageId_LRAS_CS_ack_INS);
    append_u32_be(ackPacket.data, payloadLength);
    append_u32_be(ackPacket.data, 0);
    append_u32_be(ackPacket.data, 0);
    append_u32_be(ackPacket.data, *actionId);
    append_u32_be(ackPacket.data, sourceMessageId);
    append_u16_be(ackPacket.data, ackNackAccepted);
    append_u16_be(ackPacket.data, nackReasonNone);

    try {
        boost::asio::io_context txIoContext;
        UdpMulticastSender sender(txIoContext);
        const SendResult result = sender.send(ackPacket, "226.1.1.43", 55010);
        if (!result.success) {
            std::cerr << "[CMS Entity] Errore invio ACK multicast verso "
                      << "226.1.1.43" << ":" << 55010
                      << " -> " << result.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[CMS Entity] Eccezione durante invio ACK multicast: "
                  << e.what() << std::endl;
    }
}


