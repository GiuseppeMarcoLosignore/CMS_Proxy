#include "CmsEntity.hpp"

#include "AcsEvents.hpp"
#include "CmsEvents.hpp"
#include "CueingMath.hpp"
#include "Topics.hpp"
#include "UdpMulticastReceiver.hpp"

#include <algorithm>
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

constexpr std::size_t AckHeaderSize = 16;
constexpr std::size_t AckMessageSize = 28;
constexpr uint32_t MsgId_LRAS_CS_ack_INS = 576879045;
constexpr uint16_t AckAccepted = 1;
constexpr uint16_t NackNotExecuted = 2;

uint16_t map_nack_reason(const SendResult& sr) {
    using namespace boost::asio;
    if (sr.success) {
        return 0;
    }

    const auto ec = boost::system::error_code(sr.error_value, boost::system::system_category());
    if (ec == error::invalid_argument || ec == error::bad_descriptor || sr.error_category == "resolver") {
        return 2;
    }
    if (ec == error::already_started || ec == error::in_progress || ec == error::operation_aborted) {
        return 3;
    }
    if (ec == error::timed_out || ec == error::try_again || ec == error::would_block || ec == error::not_connected) {
        return 4;
    }
    if (ec == error::connection_refused || ec == error::connection_reset || ec == error::host_unreachable ||
        ec == error::network_unreachable || ec == error::network_down || ec == error::broken_pipe ||
        ec == error::eof) {
        return 5;
    }
    return 0;
}

float normalize_0_360(float angleDeg) {
    return cueing::mod360(angleDeg);
}

uint16_t encode_delta_u16(float angleDeg) {
    const float normalized = normalize_0_360(angleDeg);
    const int rounded = static_cast<int>(std::lround(normalized));
    return static_cast<uint16_t>(rounded & 0xFFFF);
}

std::string describe_transport_error(const boost::system::error_code& ec) {
    using namespace boost::asio;

    if (ec == error::connection_refused) {
        return "Connessione rifiutata dal peer (porta chiusa o servizio non in ascolto).";
    }
    if (ec == error::timed_out) {
        return "Timeout di rete (nessuna risposta entro il tempo previsto).";
    }
    if (ec == error::host_unreachable) {
        return "Host non raggiungibile (routing/host target non disponibile).";
    }
    if (ec == error::network_unreachable) {
        return "Rete non raggiungibile (problema di routing/interfaccia).";
    }
    if (ec == error::not_connected) {
        return "Socket non connesso.";
    }
    if (ec == error::connection_reset) {
        return "Connessione resettata dal peer (RST).";
    }
    if (ec == error::eof) {
        return "Connessione chiusa ordinatamente dal peer (EOF).";
    }
    if (ec == error::broken_pipe) {
        return "Scrittura su connessione non piu valida (broken pipe).";
    }
    if (ec == error::operation_aborted) {
        return "Operazione annullata (socket chiuso o stop richiesto).";
    }

    return std::string("Errore non classificato: [") + ec.category().name() + ":"
        + std::to_string(ec.value()) + "] " + ec.message();
}

SendResult make_send_result_from_ec(const boost::system::error_code& ec) {
    SendResult result;
    result.success = !ec;
    result.error_value = ec.value();
    result.error_category = ec.category().name();
    result.error_message = ec.message();
    return result;
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
      rxWorkGuard_(std::nullopt),
      tcpSocket_(rxIoContext_),
            ackSocket_(rxIoContext_) {
}

void CmsEntity::start() {
    subscribeTopics();
    initializeAckSocket();
        initializePeriodicMulticastSender();

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

    if (config_.periodic_health_status.enabled) {
        periodicHealthTimer_.emplace(rxIoContext_);
        schedulePeriodicHealthStatusTick();
    }

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[CMS Entity] Avviata su "
              << config_.multicast_group << ":" << config_.multicast_port << std::endl;
    if (config_.periodic_health_status.enabled) {
        std::cout << "[CMS Entity] Periodic CS_MULTI_health_status_INS attivo: interval_ms="
                  << config_.periodic_health_status.interval_ms << std::endl;
    }
}

void CmsEntity::stop() {
    if (receiver_) {
        receiver_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    if (periodicHealthTimer_.has_value()) {
        periodicHealthTimer_->cancel();
    }

    {
        std::lock_guard<std::mutex> lock(transportMutex_);
        boost::system::error_code ec;
        tcpSocket_.close(ec);
        ackSocket_.close(ec);
        periodicMulticastSender_.reset();
    }

    rxIoContext_.stop();
}

void CmsEntity::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    const char* dispatchTopics[] = {
        Topics::CS_LRAS_change_configuration_order_INS,
        Topics::CS_LRAS_cueing_order_cancellation_INS,
        Topics::CS_LRAS_cueing_order_INS,
        Topics::CS_LRAS_emission_control_INS
    };

    for (const char* topic : dispatchTopics) {
        eventBus_->subscribe(topic, [this](const EventBus::EventPtr& event) {
            handleDispatchTopicEvent(event);
        });
    }

    eventBus_->subscribe(Topics::CmsStateUpdate, [this](const EventBus::EventPtr& event) {
        handleStateUpdateEvent(event);
    });

    eventBus_->subscribe(Topics::CmsPeriodicMessageTick, [this](const EventBus::EventPtr& event) {
        handlePeriodicTickEvent(event);
    });
}

void CmsEntity::initializeAckSocket() {
    ackSocketReady_ = false;
    if (config_.handlers.ack_send.target_ip.empty() || config_.handlers.ack_send.target_port == 0) {
        std::cerr << "[CMS Entity] Endpoint ACK non configurato." << std::endl;
        return;
    }

    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(config_.handlers.ack_send.target_ip, ec);
    if (ec) {
        std::cerr << "[CMS Entity] IP ACK non valido: " << ec.message() << std::endl;
        return;
    }

    ackTargetEndpoint_ = boost::asio::ip::udp::endpoint(address, config_.handlers.ack_send.target_port);
    ackSocket_.open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        std::cerr << "[CMS Entity] Errore apertura socket ACK: " << ec.message() << std::endl;
        return;
    }

    ackSocketReady_ = true;
}

void CmsEntity::initializePeriodicMulticastSender() {
    periodicUnicastSocketReady_ = false;
    if (!config_.handlers.udp_unicast_send.enabled) {
        return;
    }

    try {
        periodicMulticastSender_ = std::make_unique<UdpMulticastSender>(rxIoContext_);
    } catch (const std::exception& e) {
        std::cerr << "[CMS Entity] Errore init sender multicast periodico: " << e.what() << std::endl;
        periodicMulticastSender_.reset();
        return;
    }

    periodicUnicastSocketReady_ = true;
}

void CmsEntity::schedulePeriodicHealthStatusTick() {
    if (!periodicHealthTimer_.has_value()) {
        return;
    }

    periodicHealthTimer_->expires_after(std::chrono::milliseconds(config_.periodic_health_status.interval_ms));
    periodicHealthTimer_->async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }

        publishPeriodicHealthStatusTick();
        schedulePeriodicHealthStatusTick();
    });
}

void CmsEntity::publishPeriodicHealthStatusTick() {
    if (!eventBus_) {
        return;
    }

    auto tickEvent = std::make_shared<CmsPeriodicMessageTickEvent>();
    eventBus_->publish(tickEvent);
}

void CmsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!eventBus_) {
        return;
    }

    const uint32_t sourceMessageId = extract_message_id_from_header(packet);
    const SystemStateSnapshot snapshot = systemState_ ? systemState_->getSnapshot() : SystemStateSnapshot{};
    const ConversionResult result = convertIncomingPacket(packet, snapshot);

    if (result.packets.empty() && !result.ack_only) {
        std::cerr << "[CMS Entity] Messaggio ignorato: source_id=" << sourceMessageId << std::endl;
        return;
    }

    if (result.ack_only) {
        SendResult sendResult;
        sendResult.success = true;
        sendAck(result.ack_builder(0, sourceMessageId, sendResult));
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
            dispatchTopicEvent->ackBuilder = result.ack_builder;
            dispatchTopicEvent->sourceMessageId = sourceMessageId;
            eventBus_->publish(dispatchTopicEvent);
        }
    }

    if (!result.state_updates.empty()) {
        auto stateEvent = std::make_shared<CmsStateUpdateEvent>();
        stateEvent->updates = result.state_updates;
        eventBus_->publish(stateEvent);
    }
}

void CmsEntity::handleDispatchTopicEvent(const EventBus::EventPtr& event) {
    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const SendResult sendResult = sendTcpToLrad(dispatchEvent->packet);
    const uint32_t actionId = extractActionIdFromPayload(dispatchEvent->packet);
    sendAck(dispatchEvent->ackBuilder(actionId, dispatchEvent->sourceMessageId, sendResult));
}

void CmsEntity::handleStateUpdateEvent(const EventBus::EventPtr& event) {
    const auto stateEvent = std::dynamic_pointer_cast<const CmsStateUpdateEvent>(event);
    if (!stateEvent || !systemState_) {
        return;
    }

    systemState_->applyBatch(stateEvent->updates);
}

void CmsEntity::handlePeriodicTickEvent(const EventBus::EventPtr& event) {
    const auto tickEvent = std::dynamic_pointer_cast<const CmsPeriodicMessageTickEvent>(event);
    if (!tickEvent) {
        return;
    }

    sendPeriodicUnicast(buildHealthStatusPacket());
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
    result.ack_builder = [this](uint32_t actionId, uint32_t sourceMessageId, const SendResult& sendResult) {
        return buildAckPacket(actionId, sourceMessageId, sendResult);
    };

    switch (header.messageId) {
        case MessageId_CS_LRAS_change_configuration_order_INS:
            result.packets = parse_CS_LRAS_change_configuration_order_INS(packet, result.state_updates);
            result.packet_topics.assign(result.packets.size(), Topics::CS_LRAS_change_configuration_order_INS);
            break;
        case MessageId_CS_LRAS_cueing_order_cancellation_INS:
            result.ack_only = true;
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

RawPacket CmsEntity::buildHealthStatusPacket() const {
    constexpr std::size_t commonHeaderSize = 16;
    constexpr std::size_t messageSize = 24;
    std::vector<uint8_t> bytes(messageSize, 0);

    const uint32_t word0 = htonl(MessageId_CS_MULTI_health_status_INS);
    const uint32_t word1 = htonl(static_cast<uint32_t>(messageSize - commonHeaderSize));
    const uint32_t word2 = 0;
    const uint32_t word3 = 0;
    std::memcpy(bytes.data() + 0, &word0, sizeof(uint32_t));
    std::memcpy(bytes.data() + 4, &word1, sizeof(uint32_t));
    std::memcpy(bytes.data() + 8, &word2, sizeof(uint32_t));
    std::memcpy(bytes.data() + 12, &word3, sizeof(uint32_t));

    const SystemStateSnapshot snapshot = systemState_ ? systemState_->getSnapshot() : SystemStateSnapshot{};
    const uint16_t csStatus = htons(map_cs_status_from_snapshot(snapshot));
    const uint16_t drmuStatus = htons(static_cast<uint16_t>(1));
    const uint16_t spare = htons(static_cast<uint16_t>(0));
    const uint16_t cssStatus = htons(static_cast<uint16_t>(1));
    std::memcpy(bytes.data() + 16, &csStatus, sizeof(uint16_t));
    std::memcpy(bytes.data() + 18, &drmuStatus, sizeof(uint16_t));
    std::memcpy(bytes.data() + 20, &spare, sizeof(uint16_t));
    std::memcpy(bytes.data() + 22, &cssStatus, sizeof(uint16_t));

    return RawPacket{std::move(bytes)};
}

RawPacket CmsEntity::buildAckPacket(uint32_t actionId, uint32_t sourceMessageId, const SendResult& result) const {
    std::vector<uint8_t> bytes(AckMessageSize, 0);
    const uint16_t ackNack = result.success ? AckAccepted : NackNotExecuted;
    const uint16_t nackReason = result.success ? 0 : map_nack_reason(result);
    const uint32_t word0 = htonl(MsgId_LRAS_CS_ack_INS);
    const uint32_t word1 = htonl(static_cast<uint32_t>(AckMessageSize - AckHeaderSize));
    const uint32_t word2 = 0;
    const uint32_t word3 = 0;
    std::memcpy(bytes.data() + 0, &word0, 4);
    std::memcpy(bytes.data() + 4, &word1, 4);
    std::memcpy(bytes.data() + 8, &word2, 4);
    std::memcpy(bytes.data() + 12, &word3, 4);

    const uint32_t actionIdNet = htonl(actionId);
    const uint32_t sourceIdNet = htonl(sourceMessageId);
    const uint16_t ackNackNet = htons(ackNack);
    const uint16_t nackReasonNet = htons(nackReason);
    std::memcpy(bytes.data() + 16, &actionIdNet, 4);
    std::memcpy(bytes.data() + 20, &sourceIdNet, 4);
    std::memcpy(bytes.data() + 24, &ackNackNet, 2);
    std::memcpy(bytes.data() + 26, &nackReasonNet, 2);
    return RawPacket{std::move(bytes)};
}

uint32_t CmsEntity::extractActionIdFromPayload(const RawPacket& packet) const {
    try {
        const auto payload = json::parse(packet.data.begin(), packet.data.end());
        if (!payload.contains("param") || !payload.at("param").is_object()) {
            return 0;
        }
        const auto& param = payload.at("param");
        if (!param.contains("action_id")) {
            return 0;
        }
        return param.at("action_id").get<uint32_t>();
    } catch (...) {
        return 0;
    }
}

SendResult CmsEntity::sendTcpToLrad(const RawPacket& packet) {
    std::lock_guard<std::mutex> lock(transportMutex_);

    auto destinationIt = config_.handlers.tcp_send.lrad_destinations.find(packet.destinationLradId);
    if (destinationIt == config_.handlers.tcp_send.lrad_destinations.end()) {
        SendResult result;
        result.success = false;
        result.error_value = -1;
        result.error_category = "handler";
        result.error_message = "LRAD ID non configurato";
        std::cerr << "[CMS Entity] LRAD ID non configurato: " << packet.destinationLradId << std::endl;
        return result;
    }

    const auto& destination = destinationIt->second;
    const std::string cacheKey = destination.ip_address + ":" + std::to_string(destination.port);
    boost::asio::ip::tcp::endpoint targetEndpoint;

    const auto cacheIt = tcpEndpointCache_.find(cacheKey);
    if (cacheIt != tcpEndpointCache_.end()) {
        targetEndpoint = cacheIt->second;
    } else {
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver resolver(rxIoContext_);
        auto results = resolver.resolve(destination.ip_address, std::to_string(destination.port), ec);
        if (ec || results.begin() == results.end()) {
            std::cerr << "[CMS Entity] Errore risoluzione host " << destination.ip_address
                      << " -> " << (ec ? describe_transport_error(ec) : std::string("nessun endpoint valido"))
                      << std::endl;
            return ec ? make_send_result_from_ec(ec) : SendResult{false, -1, "resolver", "Nessun endpoint valido trovato"};
        }
        targetEndpoint = *results.begin();
        tcpEndpointCache_[cacheKey] = targetEndpoint;
    }

    boost::system::error_code ec;
    bool needReconnect = !tcpSocket_.is_open();
    if (!needReconnect) {
        try {
            if (tcpSocket_.remote_endpoint() != targetEndpoint) {
                tcpSocket_.close(ec);
                needReconnect = true;
            }
        } catch (const boost::system::system_error&) {
            tcpSocket_.close(ec);
            needReconnect = true;
        }
    }

    if (needReconnect) {
        tcpSocket_.connect(targetEndpoint, ec);
        if (ec) {
            std::cerr << "[CMS Entity] Errore connessione a " << destination.ip_address
                      << ":" << destination.port << " -> " << describe_transport_error(ec) << std::endl;
            return make_send_result_from_ec(ec);
        }
    }

    boost::asio::write(tcpSocket_, boost::asio::buffer(packet.data), ec);
    if (ec) {
        std::cerr << "[CMS Entity] Errore invio TCP -> " << describe_transport_error(ec) << std::endl;
        tcpSocket_.close(ec);
        return make_send_result_from_ec(ec);
    }

    SendResult result;
    result.success = true;
    return result;
}

void CmsEntity::sendAck(const RawPacket& packet) {
    std::lock_guard<std::mutex> lock(transportMutex_);
    if (!ackSocketReady_) {
        std::cerr << "[CMS Entity] Socket ACK non pronto." << std::endl;
        return;
    }

    boost::system::error_code ec;
    ackSocket_.send_to(boost::asio::buffer(packet.data), ackTargetEndpoint_, 0, ec);
    if (ec) {
        std::cerr << "[CMS Entity] Errore invio ACK UDP: " << ec.message() << std::endl;
    }
}

void CmsEntity::sendPeriodicUnicast(const RawPacket& packet) {
    if (!config_.handlers.udp_unicast_send.enabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(transportMutex_);
    if (!periodicUnicastSocketReady_ || !periodicMulticastSender_) {
        std::cerr << "[CMS Entity] Sender multicast periodico non pronto." << std::endl;
        return;
    }

    const SendResult result = periodicMulticastSender_->send(
        packet,
        config_.handlers.udp_unicast_send.target_ip,
        config_.handlers.udp_unicast_send.target_port
    );
    if (!result.success) {
        std::cerr << "[CMS Entity] Errore invio periodic multicast: "
                  << result.error_category << " (" << result.error_value << ") "
                  << result.error_message << std::endl;
    }
}
