#include "CmsEntity.hpp"

#include "NetworkConfigChangedEvent.hpp"
#include "AcsEntity.hpp"
#include "CueingMath.hpp"
#include "Topics.hpp"
#include "UdpSocket.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
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
constexpr uint32_t MessageId_LRAS_CS_ack_INS = 576978945;
constexpr uint32_t MessageId_LRAS_CS_lrad_1_status_INS = 576978949;
constexpr uint32_t MessageId_LRAS_CS_lrad_2_status_INS = 576978950;
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
constexpr uint32_t MessageLength_LRAS_CS_lrad_status_INS = 32;
constexpr uint32_t MessageId_LRAS_MULTI_full_status_v2_INS = 576913411;
constexpr uint32_t MessageLength_LRAS_MULTI_full_status_v2_INS = 88; // 2 LRADs x 44 bytes each
constexpr uint32_t MessageId_LRAS_MULTI_health_status_INS = 576913410;
constexpr uint32_t MessageLength_LRAS_MULTI_health_status_INS = 1728; // 6 (sys) + 2x856 (LRADs) + 10 (consoles)
constexpr const char* LrasStatusMulticastGroup = "226.1.1.43";
constexpr uint16_t LrasStatusMulticastPort = 55010;

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

void append_i16_be(std::vector<uint8_t>& buffer, int16_t value) {
    const uint16_t netValue = htons(static_cast<uint16_t>(value));
    const auto* bytes = reinterpret_cast<const uint8_t*>(&netValue);
    buffer.insert(buffer.end(), bytes, bytes + sizeof(netValue));
}

void append_f32_be(std::vector<uint8_t>& buffer, float value) {
    uint32_t rawValue = 0;
    std::memcpy(&rawValue, &value, sizeof(rawValue));
    append_u32_be(buffer, rawValue);
}

std::string to_lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

uint16_t derive_lrad_status(const StateUpdate& state) {
    if (state.lradStatus.has_value()) {
        return *state.lradStatus;
    }

    if (state.online.has_value()) {
        return *state.online ? 1 : 3;
    }

    return 1;
}

uint16_t derive_lrad_mode(const StateUpdate& state) {
    if (state.lradMode.has_value()) {
        return *state.lradMode;
    }

    if (state.cueingStatus.has_value()) {
        const std::string cueingStatus = to_lower_ascii(*state.cueingStatus);
        if (cueingStatus == "2" || cueingStatus == "blind arc" || cueingStatus == "cueing in blind arc") {
            return 4;
        }

        if (cueingStatus == "1" || cueingStatus == "cueing" || cueingStatus == "cueing in progress") {
            return 3;
        }

        if (cueingStatus == "manual search") {
            return 2;
        }

        if (cueingStatus == "video tracking") {
            return 5;
        }
    }

    if (state.engaged.has_value() && *state.engaged) {
        return 3;
    }

    return 1;
}

uint16_t derive_cueing_status(const StateUpdate& state) {
    if (state.cueingStatus.has_value()) {
        const std::string cueingStatus = to_lower_ascii(*state.cueingStatus);
        if (cueingStatus == "0" || cueingStatus == "no cueing") {
            return 0;
        }

        if (cueingStatus == "1" || cueingStatus == "cueing" || cueingStatus == "cueing in progress") {
            return 1;
        }

        if (cueingStatus == "2" || cueingStatus == "blind arc" || cueingStatus == "cueing in blind arc") {
            return 2;
        }

        if (cueingStatus == "3" || cueingStatus == "cueing in pause") {
            return 3;
        }
    }

    if (state.engaged.has_value() && *state.engaged) {
        return 1;
    }

    return 0;
}

uint16_t derive_video_tracking_status(const StateUpdate& state) {
    if (state.videoTrackingStatus.has_value()) {
        return *state.videoTrackingStatus;
    }

    if (state.lradMode.has_value() && *state.lradMode == 5) {
        return 1;
    }

    return 0;
}

int16_t derive_lrf_distance(const StateUpdate& state) {
    return state.lrfDistance.value_or(static_cast<int16_t>(-1));
}

uint16_t derive_inhibition_sector_flag(const StateUpdate& state) {
    if (state.withinInhibitionSector.has_value()) {
        return *state.withinInhibitionSector ? 1 : 0;
    }

    return 0;
}

uint16_t derive_laser_dazzler_mode(const StateUpdate& state) {
    if (state.laserDazzlerMode.has_value()) {
        return *state.laserDazzlerMode;
    }

    if (state.ladEnabled.has_value()) {
        return *state.ladEnabled ? 1 : 0;
    }

    return 0;
}

RawPacket build_lrad_status_packet(const StateUpdate& state, uint32_t messageId) {
    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_lrad_status_INS);

    append_u32_be(packet.data, messageId);
    append_u32_be(packet.data, MessageLength_LRAS_CS_lrad_status_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    append_u16_be(packet.data, derive_lrad_status(state));
    append_u16_be(packet.data, derive_lrad_mode(state));
    append_u16_be(packet.data, derive_cueing_status(state));
    append_u16_be(packet.data, derive_video_tracking_status(state));
    append_f32_be(packet.data, state.azimuth.value_or(0.0f));
    append_f32_be(packet.data, state.elevation.value_or(0.0f));
    append_i16_be(packet.data, derive_lrf_distance(state));
    append_u16_be(packet.data, derive_inhibition_sector_flag(state));
    append_u16_be(packet.data, state.searchlightPower.value_or(0));
    append_u16_be(packet.data, state.searchlightZoom.value_or(0));
    append_u16_be(packet.data, derive_laser_dazzler_mode(state));
    append_u16_be(packet.data, state.videoZoom.value_or(0));
    append_u16_be(packet.data, state.gyroSelection.value_or(0));
    append_u16_be(packet.data, state.gyroUsed.value_or(0));

    return packet;
}

void send_multicast_packet(const RawPacket& packet, const char* messageName) {
    try {
        boost::asio::io_context txIoContext;
        UdpSocket sender(txIoContext);
        const SendResult result = sender.send(packet, LrasStatusMulticastGroup, LrasStatusMulticastPort);
        if (!result.success) {
            std::cerr << "[CMS Entity] Errore invio " << messageName << " verso "
                      << LrasStatusMulticastGroup << ":" << LrasStatusMulticastPort
                      << " -> " << result.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[CMS Entity] Eccezione durante invio " << messageName << ": "
                  << e.what() << std::endl;
    }
}

// Appends the 44-byte "LRAD x full status" block (Lrad_full 40-byte struct + IR camera 4 bytes)
void append_lrad_full_status(std::vector<uint8_t>& buffer, const StateUpdate& state) {
    // Lrad_full (40 bytes)
    append_u16_be(buffer, state.communication.value_or(0));
    append_u16_be(buffer, state.motionAzimuthStatus.value_or(0));
    append_u16_be(buffer, state.motionElevationStatus.value_or(0));
    // Audio Emitter
    append_u16_be(buffer, state.audioEmitterOvertemperature.value_or(0));
    append_u16_be(buffer, state.audioEmitterCommFailure.value_or(0));
    // Searchlight
    append_u16_be(buffer, state.searchlightOvertemperature.value_or(0));
    append_u16_be(buffer, state.searchlightCommFailure.value_or(0));
    // Laser Dazzler
    append_u16_be(buffer, state.laserDazzlerOvertemperature.value_or(0));
    append_u16_be(buffer, state.laserDazzlerCommFailure.value_or(0));
    // LRF
    append_u16_be(buffer, state.lrfOvertemperature.value_or(0));
    append_u16_be(buffer, state.lrfCommFailure.value_or(0));
    // Remaining Lrad_full fields
    append_u16_be(buffer, state.trackingBoardStatus.value_or(0));
    append_u16_be(buffer, state.visibleCameraStatus.value_or(0));
    append_u16_be(buffer, state.visibleCameraSignal.value_or(0));
    append_u16_be(buffer, state.internalImuStatus.value_or(0));
    append_u16_be(buffer, state.canBus1.value_or(0));
    append_u16_be(buffer, state.canBus2.value_or(0));
    append_u16_be(buffer, state.cpuSlaveStatus.value_or(0));
    append_u16_be(buffer, state.electronicBoxTemperature.value_or(0));
    append_u16_be(buffer, state.interfaceBoxTemperature.value_or(0));
    // Outside Lrad_full but within LRAD x full status block (4 bytes)
    append_u16_be(buffer, state.irCameraStatus.value_or(0));
    append_u16_be(buffer, state.irCameraSignal.value_or(0));
}

// Appends the 856-byte "LRAD x health" block for LRAS_MULTI_health_status_INS.
// Layout (byte offsets relative to block start):
//   0  Configuration (2)
//   2  Condition (2)
//   4  Operative State (2)
//   6  HW emission authorization (2)
//   8  Audio Emitter Condition (2)
//  10  Audio Mode struct (792):
//         VolumeMode(10): Level(2)+AudioVolumedB(4)+Mute(2)+AudioMode(2)
//         RecordedMessageTone(8): messId(4)+Language(2)+Loop(2)
//         FreeText(774): LanguageIn(2)+LanguageOut(2)+messageText(768)+Loop(2)
// 802  Searchlight Condition (2)
// 804  Searchlight Mode (4): LightPower(2)+LightZoom(2)
// 808  Laser Dazzler Condition (2)
// 810  Laser Mode (2)
// 812  LRF Condition (2)
// 814  LRF on off (2)
// 816  Camera Condition (2)
// 818  Camera Zoom (2)
// 820  IMU Condition (2)
// 822  Recorder Condition (2)
// 824  Recorder Mode (2)
// 826  Recorder Time limit (8): ElapsedSec(4)+ElapsedUsec(4)
// 834  Horizontal Reference (2)
// 836  Inhibit Sector 1 (10): OnOff(2)+AzStart(4)+AzStop(4)
// 846  Inhibit Sector 2 (10): OnOff(2)+AzStart(4)+AzStop(4)
// Total = 856 bytes
void append_lrad_health_block(std::vector<uint8_t>& buffer, const StateUpdate& state) {
    // Pre-AudioMode fields (10 bytes)
    append_u16_be(buffer, state.lradConfiguration.value_or(0));
    append_u16_be(buffer, state.lradCondition.value_or(0));
    append_u16_be(buffer, state.lradOperativeState.value_or(0));
    append_u16_be(buffer, state.hwEmissionAuth.value_or(0));
    append_u16_be(buffer, state.audioEmitterCondition.value_or(0));

    // Audio Mode struct (792 bytes)
    // Volume Mode (10 bytes)
    append_u16_be(buffer, state.volumeLevel.value_or(0));
    append_f32_be(buffer, state.audioVolumeDb.value_or(0.0f));
    append_u16_be(buffer, state.mute.value_or(0));
    append_u16_be(buffer, state.audioMode.value_or(0));
    // Recorded Message-Tone (8 bytes)
    append_u32_be(buffer, state.recordedMessageId.value_or(0));
    append_u16_be(buffer, state.recordedMessageLanguage.value_or(0));
    append_u16_be(buffer, state.recordedMessageLoop.value_or(0));
    // Free Text (774 bytes): Language in(2) + Language out(2) + text(768) + Loop(2)
    append_u16_be(buffer, state.freeTextLanguageIn.value_or(0));
    append_u16_be(buffer, state.freeTextLanguageOut.value_or(0));
    {
        // message text: fixed 768-byte UTF-8 field, zero-padded
        constexpr std::size_t textSize = 768;
        const std::string& text = state.freeTextMessage.value_or(std::string{});
        const std::size_t copyLen = std::min(text.size(), textSize);
        buffer.insert(buffer.end(),
                      reinterpret_cast<const uint8_t*>(text.data()),
                      reinterpret_cast<const uint8_t*>(text.data()) + copyLen);
        buffer.insert(buffer.end(), textSize - copyLen, 0x00);
    }
    append_u16_be(buffer, state.freeTextLoop.value_or(0));

    // Searchlight Condition + Mode (6 bytes)
    append_u16_be(buffer, state.searchlightCondition.value_or(0));
    append_u16_be(buffer, state.searchlightPower.value_or(0));   // Light Power
    append_u16_be(buffer, state.searchlightZoom.value_or(0));    // Light Zoom

    // Laser Dazzler, LRF, Camera (10 bytes)
    append_u16_be(buffer, state.laserDazzlerCondition.value_or(0));
    append_u16_be(buffer, state.laserMode.value_or(0));
    append_u16_be(buffer, state.lrfCondition.value_or(0));
    append_u16_be(buffer, state.lrfOnOff.value_or(0));
    append_u16_be(buffer, state.cameraCondition.value_or(0));
    append_u16_be(buffer, state.cameraZoom.value_or(0));

    // IMU, Recorder (10 bytes + 8 bytes time limit)
    append_u16_be(buffer, state.imuCondition.value_or(0));
    append_u16_be(buffer, state.recorderCondition.value_or(0));
    append_u16_be(buffer, state.recorderMode.value_or(0));
    append_u32_be(buffer, state.recorderElapsedSeconds.value_or(0));
    append_u32_be(buffer, state.recorderElapsedMicroseconds.value_or(0));

    // Horizontal Reference (2 bytes)
    append_u16_be(buffer, state.horizontalReference.value_or(0));

    // Inhibit Sector 1 (10 bytes)
    append_u16_be(buffer, state.inhibitSector1Active.value_or(0));
    append_f32_be(buffer, state.inhibitSector1Start.value_or(0.0f));
    append_f32_be(buffer, state.inhibitSector1Stop.value_or(0.0f));

    // Inhibit Sector 2 (10 bytes)
    append_u16_be(buffer, state.inhibitSector2Active.value_or(0));
    append_f32_be(buffer, state.inhibitSector2Start.value_or(0.0f));
    append_f32_be(buffer, state.inhibitSector2Stop.value_or(0.0f));
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

int16_t read_i16_be(const std::vector<uint8_t>& data, std::size_t offset) {
    return static_cast<int16_t>(read_u16_be(data, offset));
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

// Helper function: assume LRAD 1 and 2 are always known
bool has_known_lrad(uint16_t lradId) {
    return (lradId == 1 || lradId == 2);
}

} // namespace

CmsEntity::CmsEntity(const CmsConfig& config,
                     std::shared_ptr<EventBus> eventBus)
    : config_(config),
      eventBus_(std::move(eventBus)),
      rxIoContext_(),
            rxWorkGuard_(std::nullopt),
            periodicTimer_(std::nullopt) {
}

void CmsEntity::start() {
    if (!subscribed_.exchange(true)) {
        subscribeTopics();
    }

    receiver_ = std::make_shared<UdpSocket>(
        rxIoContext_,
        "0.0.0.0", //useless, receiver will bind to the multicast group address directly
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

    boost::asio::post(rxIoContext_, [this]() {
        periodicMessages(); // Commentare per test.
    });

    running_.store(true);
     

    std::cout << "[CMS Entity] Avviata su "
              << config_.multicast_group << ":" << config_.multicast_port << std::endl;
}

void CmsEntity::stop() {
    running_.store(false);

    if (receiver_) {
        receiver_->stop();
    }

    if (periodicTimer_.has_value()) {
        periodicTimer_->cancel();
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

    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_CS_ack_INS(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_cueing_order_cancellation_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_CS_ack_INS(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_emission_mode_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_CS_ack_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_CS_lrad_1_status_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_CS_lrad_1_status_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_CS_lrad_2_status_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_CS_lrad_2_status_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_MULTI_full_status_v2_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_MULTI_full_status_v2_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_MULTI_health_status_INS, [this](const EventBus::EventPtr& event) {
        sendLRAS_MULTI_health_status_INS(event);
    });

    eventBus_->subscribe(Topics::NetworkConfigChanged, [this](const EventBus::EventPtr& event) {
        handleConfigChanged(event);
    });

}

void CmsEntity::handleConfigChanged(const EventBus::EventPtr& event) {
    const auto configEvent = std::dynamic_pointer_cast<const NetworkConfigChangedEvent>(event);
    if (!configEvent) {
        return;
    }

    const CmsConfig newConfig = configEvent->cms;
    boost::asio::post(rxIoContext_, [this, newConfig]() {
        const bool endpointChanged =
            (config_.multicast_group != newConfig.multicast_group) ||
            (config_.multicast_port != newConfig.multicast_port);

        config_ = newConfig;

        if (!running_.load() || !endpointChanged) {
            return;
        }

        try {
            if (receiver_) {
                receiver_->stop();
            }

            receiver_ = std::make_shared<UdpSocket>(
                rxIoContext_,
                "0.0.0.0",
                config_.multicast_group,
                config_.multicast_port
            );

            receiver_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
                onPacketReceived(packet, sourceInfo);
            });

            receiver_->start();
            std::cout << "[CMS Entity] Config aggiornata: listener su "
                      << config_.multicast_group << ":" << config_.multicast_port << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[CMS Entity] Errore hot reload config: " << e.what() << std::endl;
        }
    });
}

void CmsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!eventBus_) {
        return;
    }

    const uint32_t sourceMessageId = extract_message_id_from_header(packet);
    const ConversionResult result = convertIncomingPacket(packet);

    if (!result.packet.has_value()) {
        std::cerr << "[CMS Entity] Messaggio ignorato: source_id=" << sourceMessageId << std::endl;
        return;
    }

    const RawPacket& packetToSend = *result.packet;


    if (!result.packet_topic.empty()) {
        auto dispatchTopicEvent = std::make_shared<CmsDispatchTopicPacketEvent>();
        dispatchTopicEvent->dispatchTopic = result.packet_topic;
        dispatchTopicEvent->packet = packetToSend;
        dispatchTopicEvent->nackreason = packetToSend.nackreason;
        eventBus_->publish(dispatchTopicEvent);

        if (!result.state_updates.empty()) {
            auto stateEvent = std::make_shared<TopicStateUpdateEvent>();
            stateEvent->sourceTopic = result.packet_topic;
            stateEvent->updates = result.state_updates;
            eventBus_->publish(stateEvent);
        }
    }
}

bool CmsEntity::parseHeader(const RawPacket& packet, ParsedHeader& out) const {
    if (packet.data.size() < HeaderSize) {
        return false;
    }

    out.messageId = read_u32_be(packet.data, 0);
    const uint32_t rawLength32 = read_u32_be(packet.data, 4);

    // In current INS frames this 32-bit word is composite; the effective length is in low 16 bits.
    const uint32_t length16 = rawLength32 & 0xFFFFu;

    // Convention A: length field is payload length.
    if (packet.data.size() >= HeaderSize + length16) {
        out.messageLength = static_cast<uint16_t>(length16);
        return true;
    }

    // Convention B: length field is total packet length (header + payload).
    if (length16 >= HeaderSize && packet.data.size() >= length16) {
        out.messageLength = static_cast<uint16_t>(length16 - HeaderSize);
        return true;
    }

    // Fallback for senders that really use a plain 32-bit total-length field.
    if (rawLength32 >= HeaderSize && packet.data.size() >= rawLength32) {
        out.messageLength = static_cast<uint16_t>((rawLength32 - HeaderSize) & 0xFFFFu);
        return true;
    }

    return false;
}

ConversionResult CmsEntity::convertIncomingPacket(const RawPacket& packet) const {
    using ParserFn = RawPacket (CmsEntity::*)(
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
            result.packet = parse_CS_LRAS_change_configuration_order_INS(packet, result.state_updates);
            result.packet_topic = Topics::CS_LRAS_change_configuration_order_INS;
            break;
        case MessageId_CS_LRAS_cueing_order_cancellation_INS:
            result.packet = parse_CS_LRAS_cueing_order_cancellation_INS(packet, result.state_updates);
            result.packet_topic = Topics::CS_LRAS_cueing_order_cancellation_INS;
            break;
        case MessageId_CS_LRAS_cueing_order_INS:
            result.packet = parse_CS_LRAS_cueing_order_INS(packet, result.state_updates);
            result.packet_topic = Topics::CS_LRAS_cueing_order_INS;
            break;
        case MessageId_CS_LRAS_emission_control_INS:
            result.packet = parse_CS_LRAS_emission_control_INS(packet, result.state_updates);
            result.packet_topic = Topics::CS_LRAS_emission_control_INS;
            break;
        default:
            {
                const auto bindingIt = additionalParserBindings.find(header.messageId);
                if (bindingIt != additionalParserBindings.end()) {
                    result.packet = (this->*(bindingIt->second.parser))(packet, result.state_updates);
                    result.packet_topic = bindingIt->second.topic;
                }
            }
            break;
    }

    return result;
}

RawPacket CmsEntity::parse_CS_LRAS_change_configuration_order_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>& stateUpdates) const {
    constexpr std::size_t offset = 16;
    constexpr std::size_t blockSize = 8;

    if (offset + blockSize > packet.data.size()) {
        return make_empty_packet();
    }

    const uint16_t actionId = read_u16_be(packet.data, offset);
    const uint16_t lradId = read_u16_be(packet.data, offset + 4);
    const uint16_t rawConfig = read_u16_be(packet.data, offset + 6);

    json payload;
    payload["Action Id"] = std::to_string(actionId);
    payload["LRAD ID"] = std::to_string(lradId);
    payload["Configuration"] = std::to_string(rawConfig);

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;
    if (rawConfig != 0 && rawConfig != 1) {
        converted.nackreason = 2;
    }
    

    StateUpdate update;
    update.lradId = lradId;
    update.engaged = (rawConfig != 0);
    stateUpdates.push_back(update);

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }
    // Assume LRAD is operativefor LRAD 1 and 2
    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_cueing_order_cancellation_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    constexpr std::size_t offset = 16;
    constexpr std::size_t blockSize = 6;
    if (offset + blockSize > packet.data.size()) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, offset);
    const uint16_t lradId = read_u16_be(packet.data, offset + 4);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = std::to_string(lradId);

    

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }
    // Assume LRAD is operativefor LRAD 1 and 2
    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_cueing_order_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    constexpr std::size_t minPayloadSize = 22;
    if (packet.data.size() < HeaderSize + minPayloadSize) {
        return make_empty_packet();
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

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }
    // Assume LRAD is operative for LRAD 1 and 2
    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_emission_control_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    if (packet.data.size() < 838) {
        return make_empty_packet();
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
    payload["actionId"] = actionId;
    payload["lradId"] = lradId;
    payload["audioModeValidity"] = audioModeValidity;
    payload["volumeLevel"] = volumeLevel;
    payload["Audio Volume dB"] = audioVolumeDb;
    payload["Mute"] = mute;
    payload["audioMode"] = audioMode;
    payload["recordedMessageId"] = recordedMessageId;
    payload["recordedLanguage"] = recordedLanguage;
    payload["recordedLoop"] = recordedLoop;
    payload["freeTextLanguageIn"] = freeTextLanguageIn;
    payload["freeTextLanguageOut"] = freeTextLanguageOut;
    payload["freeTextMessage"] = freeTextMessage;
    payload["freeTextLoop"] = freeTextLoop;
    payload["laserModeValidity"] = laserModeValidity;
    payload["laserMode"] = laserMode;
    payload["lightModeValidity"] = lightModeValidity;
    payload["lightPower"] = lightPower;
    payload["lightZoom"] = lightZoom;
    payload["lrfModeValidity"] = lrfModeValidity;
    payload["lrfOnOff"] = lrfOnOff;
    payload["cameraZoomValidity"] = cameraZoomValidity;
    payload["cameraZoom"] = cameraZoom;
    payload["horizontalReferenceValidity"] = horizontalReferenceValidity;
    payload["horizontalReference"] = horizontalReference;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }
    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_emission_mode_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return make_empty_packet();
}

RawPacket CmsEntity::parse_CS_LRAS_inhibition_sectors_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>& stateUpdates) const {
    // Message layout (total 42 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + Sector1(10) + Sector2(10)
    constexpr std::size_t minPacketSize = 42;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);

    const uint16_t sector1OnOff = read_u16_be(packet.data, 22);
    const float sector1Start = read_f32_be(packet.data, 24);
    const float sector1Stop = read_f32_be(packet.data, 28);

    const uint16_t sector2OnOff = read_u16_be(packet.data, 32);
    const float sector2Start = read_f32_be(packet.data, 34);
    const float sector2Stop = read_f32_be(packet.data, 38);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = lradId;
    payload["Sector 1"] = {
        {"On Off", sector1OnOff},
        {"start", sector1Start},
        {"stop", sector1Stop}
    };
    payload["Sector 2"] = {
        {"On Off", sector2OnOff},
        {"start", sector2Start},
        {"stop", sector2Stop}
    };

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;

    StateUpdate update;
    update.lradId = lradId;
    update.inhibitSector1Active = sector1OnOff;
    update.inhibitSector1Start = sector1Start;
    update.inhibitSector1Stop = sector1Stop;
    update.inhibitSector2Active = sector2OnOff;
    update.inhibitSector2Start = sector2Start;
    update.inhibitSector2Stop = sector2Stop;
    stateUpdates.push_back(update);

    if ((sector1OnOff != 0 && sector1OnOff != 1) || (sector2OnOff != 0 && sector2OnOff != 1)) {
        converted.nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_joystick_control_lrad_1_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 20 bytes): Header(16) + X(2) + Y(2)
    constexpr std::size_t minPacketSize = 20;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const int16_t xPosition = read_i16_be(packet.data, 16);
    const int16_t yPosition = read_i16_be(packet.data, 18);

    json payload;
    payload["LRAD ID"] = 1;
    payload["xPosition"] = xPosition;
    payload["yPosition"] = yPosition;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = 1;

    if (!has_known_lrad(1)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_joystick_control_lrad_2_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 20 bytes): Header(16) + X(2) + Y(2)
    constexpr std::size_t minPacketSize = 20;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const int16_t xPosition = read_i16_be(packet.data, 16);
    const int16_t yPosition = read_i16_be(packet.data, 18);

    json payload;
    payload["LRAD ID"] = 2;
    payload["xPosition"] = xPosition;
    payload["yPosition"] = yPosition;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = 2;

    if (!has_known_lrad(2)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_recording_command_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>& stateUpdates) const {
    // Message layout (total 36 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + Video source(2) + Video profile(2)
    // + Recording mode(2) + Elapsed seconds(4) + Elapsed micro seconds(4)
    constexpr std::size_t minPacketSize = 36;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t videoSource = read_u16_be(packet.data, 22);
    const uint16_t videoProfile = read_u16_be(packet.data, 24);
    const uint16_t recordingMode = read_u16_be(packet.data, 26);
    const uint32_t elapsedSeconds = read_u32_be(packet.data, 28);
    const uint32_t elapsedMicroseconds = read_u32_be(packet.data, 32);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = lradId;
    payload["videoSource"] = videoSource;
    payload["videoProfile"] = videoProfile;
    payload["recordingMode"] = recordingMode;
    payload["elapsedSeconds"] = elapsedSeconds;
    payload["elapsedMicroseconds"] = elapsedMicroseconds;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;

    StateUpdate update;
    update.lradId = lradId;
    update.recorderMode = recordingMode;
    update.recorderElapsedSeconds = elapsedSeconds;
    update.recorderElapsedMicroseconds = elapsedMicroseconds;
    stateUpdates.push_back(update);

    if (videoSource != 1 ||
        (videoProfile < 1 || videoProfile > 4) ||
        (recordingMode > 2) ||
        (elapsedSeconds > 2147483647U) ||
        (elapsedMicroseconds > 999999U)) {
        converted.nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_request_emission_mode_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    
    json payload;
    payload["Action Id"] = 1234;
    payload["LRAD ID"] = 1; 

    
    
    return make_empty_packet();
}

RawPacket CmsEntity::parse_CS_LRAS_request_engagement_capability_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 60 bytes):
    // Header(16) + ActionId(4) + CSTN(4) + CST Kinematics(36)
    constexpr std::size_t minPacketSize = 60;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint32_t cstn = read_u32_be(packet.data, 20);
    const uint32_t validitySeconds = read_u32_be(packet.data, 24);
    const uint32_t validityMicroseconds = read_u32_be(packet.data, 28);
    const uint16_t kinematicsType = read_u16_be(packet.data, 32);

    json payload;
    payload["Action Id"] = actionId;
    payload["CSTN"] = cstn;
    payload["timeOfValidity"] = {
        {"seconds", validitySeconds},
        {"microseconds", validityMicroseconds}
    };
    payload["kinematicsType"] = kinematicsType;

    json kinematics;
    switch (kinematicsType) {
        case 1: // 3D Cartesian Kinematics
            kinematics["x"] = read_f32_be(packet.data, 36);
            kinematics["y"] = read_f32_be(packet.data, 40);
            kinematics["z"] = read_f32_be(packet.data, 44);
            kinematics["vx"] = read_f32_be(packet.data, 48);
            kinematics["vy"] = read_f32_be(packet.data, 52);
            kinematics["vz"] = read_f32_be(packet.data, 56);
            break;
        case 2: // 3D Cartesian Position
            kinematics["x"] = read_f32_be(packet.data, 36);
            kinematics["y"] = read_f32_be(packet.data, 40);
            kinematics["z"] = read_f32_be(packet.data, 44);
            break;
        case 3: // 2D Cartesian Kinematics
            kinematics["x"] = read_f32_be(packet.data, 36);
            kinematics["y"] = read_f32_be(packet.data, 40);
            kinematics["vx"] = read_f32_be(packet.data, 44);
            kinematics["vy"] = read_f32_be(packet.data, 48);
            break;
        case 4: // 2D Cartesian Position
            kinematics["x"] = read_f32_be(packet.data, 36);
            kinematics["y"] = read_f32_be(packet.data, 40);
            break;
        case 5: // 2D Polar Kinematics
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            kinematics["angleOfSight"] = read_f32_be(packet.data, 40);
            kinematics["trueBearingRate"] = read_f32_be(packet.data, 44);
            kinematics["angleOfSightRate"] = read_f32_be(packet.data, 48);
            break;
        case 6: // 2D Polar Surface Kinematics
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            kinematics["range"] = read_f32_be(packet.data, 40);
            kinematics["trueBearingRate"] = read_f32_be(packet.data, 44);
            kinematics["rangeRate"] = read_f32_be(packet.data, 48);
            break;
        case 7: // 2D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            kinematics["angleOfSight"] = read_f32_be(packet.data, 40);
            break;
        case 8: // 2D Polar Surface Position
            kinematics["range"] = read_f32_be(packet.data, 36);
            kinematics["trueBearing"] = read_f32_be(packet.data, 40);
            break;
        case 9: // 1D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            break;
        case 10: // EW 1D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            kinematics["origin"] = {
                {"latitude", read_f32_be(packet.data, 40)},
                {"longitude", read_f32_be(packet.data, 44)}
            };
            break;
        case 11: // EW 2D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            kinematics["angleOfSight"] = read_f32_be(packet.data, 40);
            kinematics["origin"] = {
                {"latitude", read_f32_be(packet.data, 44)},
                {"longitude", read_f32_be(packet.data, 48)}
            };
            break;
        default:
            // Unknown type: keep base fields only.
            break;
    }

    payload["kinematics"] = kinematics;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());

    if ((cstn < 1 || cstn > 9999) ||
        (validitySeconds > 2147483647U) ||
        (validityMicroseconds > 999999U) ||
        (kinematicsType < 1 || kinematicsType > 11)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_request_full_status_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return make_empty_packet();
}

RawPacket CmsEntity::parse_CS_LRAS_request_installation_data_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 20 bytes): Header(16) + ActionId(4)
    constexpr std::size_t minPacketSize = 20;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);

    json payload;
    payload["Action Id"] = actionId;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_request_message_table_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return make_empty_packet();
}

RawPacket CmsEntity::parse_CS_LRAS_request_software_version_INS(
    const RawPacket&,
    std::vector<StateUpdate>&) const {
    return make_empty_packet();
}

RawPacket CmsEntity::parse_CS_LRAS_request_thresholds_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 28 bytes):
    // Header(16) + ActionId(4) + Volume selector(2) + Audio Volume dB(4) + Scenario(2)
    constexpr std::size_t minPacketSize = 28;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t volumeSelector = read_u16_be(packet.data, 20);
    const float audioVolumeDb = read_f32_be(packet.data, 22);
    const uint16_t scenario = read_u16_be(packet.data, 26);

    json payload;
    payload["Action Id"] = actionId;
    payload["volumeSelector"] = volumeSelector;
    payload["audioVolumeDb"] = audioVolumeDb;
    payload["scenario"] = scenario;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());

    if ((volumeSelector > 1) ||
        (scenario > 2) ||
        (volumeSelector == 1 && (audioVolumeDb < -128.0f || audioVolumeDb > 0.0f))) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_request_translation_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>& stateUpdates) const {
    // Message layout (total 794 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + FreeText(772)
    // FreeText = LanguageIn(2) + LanguageOut(2) + MessageText(768)
    constexpr std::size_t minPacketSize = 794;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t languageIn = read_u16_be(packet.data, 22);
    const uint16_t languageOut = read_u16_be(packet.data, 24);

    std::string messageText;
    messageText.reserve(768);
    for (std::size_t i = 26; i < 26 + 768; ++i) {
        const char c = static_cast<char>(packet.data[i]);
        if (c == '\0') {
            break;
        }
        messageText.push_back(c);
    }

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = lradId;
    payload["languageIn"] = languageIn;
    payload["languageOut"] = languageOut;
    payload["messageText"] = messageText;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;

    StateUpdate update;
    update.lradId = lradId;
    update.freeTextLanguageIn = languageIn;
    update.freeTextLanguageOut = languageOut;
    update.freeTextMessage = messageText;
    stateUpdates.push_back(update);

    // Spec notes: LanguageIn valid only Italian/English, LanguageOut tone not valid.
    if ((lradId != 1 && lradId != 2) ||
        (languageIn != 0 && languageIn != 1) ||
        (languageOut != 0 && languageOut != 1 && languageOut != 2)) {
        converted.nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_LRAS_video_tracking_command_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>& stateUpdates) const {
    // Message layout (total 24 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + Auto tracking(2)
    constexpr std::size_t minPacketSize = 24;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t autoTracking = read_u16_be(packet.data, 22);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = lradId;
    payload["autoTracking"] = autoTracking;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());
    converted.destinationLradId = lradId;

    StateUpdate update;
    update.lradId = lradId;
    update.videoTrackingStatus = (autoTracking == 1) ? 1 : 0;
    stateUpdates.push_back(update);

    if ((lradId != 1 && lradId != 2) || (autoTracking != 0 && autoTracking != 1)) {
        converted.nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_MULTI_health_status_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 24 bytes):
    // Header(16) + CS status(2) + DRMU status(2) + Spare(2) + CSS status(2)
    constexpr std::size_t minPacketSize = 24;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint16_t csStatus = read_u16_be(packet.data, 16);
    const uint16_t drmuStatus = read_u16_be(packet.data, 18);
    const uint16_t spare = read_u16_be(packet.data, 20);
    const uint16_t cssStatus = read_u16_be(packet.data, 22);

    json payload;
    payload["csStatus"] = csStatus;
    payload["drmuStatus"] = drmuStatus;
    payload["spare"] = spare;
    payload["cssStatus"] = cssStatus;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());

    if ((csStatus < 1 || csStatus > 3) ||
        (drmuStatus < 1 || drmuStatus > 3) ||
        (cssStatus < 1 || cssStatus > 2)) {
        converted.nackreason = 2;
    }

    return converted;
}

RawPacket CmsEntity::parse_CS_MULTI_update_cst_kinematics_INS(
    const RawPacket& packet,
    std::vector<StateUpdate>&) const {
    // Message layout (total 56 bytes):
    // Header(16) + CSTN(4) + TimeOfValidity(8) + Kinematics(28)
    // Kinematics = KinematicsType(2) + union data (up to 26 bytes)
    constexpr std::size_t minPacketSize = 56;
    if (packet.data.size() < minPacketSize) {
        return make_empty_packet();
    }

    const uint32_t cstn = read_u32_be(packet.data, 16);
    const uint32_t validitySeconds = read_u32_be(packet.data, 20);
    const uint32_t validityMicroseconds = read_u32_be(packet.data, 24);
    const uint16_t kinematicsType = read_u16_be(packet.data, 28);

    json payload;
    payload["CSTN"] = cstn;
    payload["timeOfValidity"] = {
        {"seconds", validitySeconds},
        {"microseconds", validityMicroseconds}
    };
    payload["kinematicsType"] = kinematicsType;

    json kinematics;
    switch (kinematicsType) {
        case 1: // 3D Cartesian Kinematics
            kinematics["x"] = read_f32_be(packet.data, 32);
            kinematics["y"] = read_f32_be(packet.data, 36);
            kinematics["z"] = read_f32_be(packet.data, 40);
            kinematics["vx"] = read_f32_be(packet.data, 44);
            kinematics["vy"] = read_f32_be(packet.data, 48);
            kinematics["vz"] = read_f32_be(packet.data, 52);
            break;
        case 2: // 3D Cartesian Position
            kinematics["x"] = read_f32_be(packet.data, 32);
            kinematics["y"] = read_f32_be(packet.data, 36);
            kinematics["z"] = read_f32_be(packet.data, 40);
            break;
        case 3: // 2D Cartesian Kinematics
            kinematics["x"] = read_f32_be(packet.data, 32);
            kinematics["y"] = read_f32_be(packet.data, 36);
            kinematics["vx"] = read_f32_be(packet.data, 40);
            kinematics["vy"] = read_f32_be(packet.data, 44);
            break;
        case 4: // 2D Cartesian Position
            kinematics["x"] = read_f32_be(packet.data, 32);
            kinematics["y"] = read_f32_be(packet.data, 36);
            break;
        case 5: // 2D Polar Kinematics
            kinematics["trueBearing"] = read_f32_be(packet.data, 32);
            kinematics["angleOfSight"] = read_f32_be(packet.data, 36);
            kinematics["trueBearingRate"] = read_f32_be(packet.data, 40);
            kinematics["angleOfSightRate"] = read_f32_be(packet.data, 44);
            break;
        case 6: // 2D Polar Surface Kinematics
            kinematics["trueBearing"] = read_f32_be(packet.data, 32);
            kinematics["range"] = read_f32_be(packet.data, 36);
            kinematics["trueBearingRate"] = read_f32_be(packet.data, 40);
            kinematics["rangeRate"] = read_f32_be(packet.data, 44);
            break;
        case 7: // 2D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 32);
            kinematics["angleOfSight"] = read_f32_be(packet.data, 36);
            break;
        case 8: // 2D Polar Surface Position
            kinematics["range"] = read_f32_be(packet.data, 32);
            kinematics["trueBearing"] = read_f32_be(packet.data, 36);
            break;
        case 9: // 1D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 32);
            break;
        case 10: // EW 1D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 32);
            kinematics["origin"] = {
                {"latitude", read_f32_be(packet.data, 36)},
                {"longitude", read_f32_be(packet.data, 40)}
            };
            break;
        case 11: // EW 2D Polar Position
            kinematics["trueBearing"] = read_f32_be(packet.data, 32);
            kinematics["angleOfSight"] = read_f32_be(packet.data, 36);
            kinematics["origin"] = {
                {"latitude", read_f32_be(packet.data, 40)},
                {"longitude", read_f32_be(packet.data, 44)}
            };
            break;
        default:
            break;
    }

    payload["kinematics"] = kinematics;

    const std::string jsonString = payload.dump();
    RawPacket converted;
    converted.data.assign(jsonString.begin(), jsonString.end());

    if ((cstn < 1 || cstn > 9999) ||
        (validitySeconds > 2147483647U) ||
        (validityMicroseconds > 999999U) ||
        (kinematicsType < 1 || kinematicsType > 11)) {
        converted.nackreason = 2;
    }

    return converted;
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

    uint16_t ackNackAccepted = 1; // ACK accepted, no NACK reason
    const uint16_t nackReason = static_cast<uint16_t>(dispatchEvent->nackreason);
    if (nackReason != 0) {
        ackNackAccepted = 2; // ACK with NACK reason
    }
    constexpr uint32_t payloadLength = 12;

    RawPacket ackPacket;
    ackPacket.data.reserve(HeaderSize + payloadLength);
    append_u32_be(ackPacket.data, MessageId_LRAS_CS_ack_INS);
    append_u32_be(ackPacket.data, payloadLength + HeaderSize);
    append_u32_be(ackPacket.data, 0);
    append_u32_be(ackPacket.data, 0);
    append_u32_be(ackPacket.data, *actionId);
    append_u32_be(ackPacket.data, sourceMessageId);
    append_u16_be(ackPacket.data, ackNackAccepted);
    append_u16_be(ackPacket.data, nackReason);

    try {
        boost::asio::io_context txIoContext;
        UdpSocket sender(txIoContext);
        const SendResult result = sender.send(ackPacket, LrasStatusMulticastGroup, LrasStatusMulticastPort);
        if (!result.success) {
            std::cerr << "[CMS Entity] Errore invio ACK multicast verso "
                      << LrasStatusMulticastGroup << ":" << LrasStatusMulticastPort
                      << " -> " << result.error_message << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[CMS Entity] Eccezione durante invio ACK multicast: "
                  << e.what() << std::endl;
    }
}

void CmsEntity::periodicMessages() {
    if (!eventBus_) {
        return;
    }

    if (!periodicTimer_.has_value()) {
        periodicTimer_.emplace(rxIoContext_);
    }

    periodicTimer_->expires_after(std::chrono::milliseconds(100));
    periodicTimer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            auto eventLrad1 = std::make_shared<CmsDispatchTopicPacketEvent>();
            eventLrad1->dispatchTopic = Topics::LRAS_CS_lrad_1_status_INS;
            eventLrad1->packet = make_empty_packet();
            eventBus_->publish(eventLrad1);

            auto eventLrad2 = std::make_shared<CmsDispatchTopicPacketEvent>();
            eventLrad2->dispatchTopic = Topics::LRAS_CS_lrad_2_status_INS;
            eventLrad2->packet = make_empty_packet();
            eventBus_->publish(eventLrad2);

            auto eventFullStatus = std::make_shared<CmsDispatchTopicPacketEvent>();
            eventFullStatus->dispatchTopic = Topics::LRAS_MULTI_full_status_v2_INS;
            eventFullStatus->packet = make_empty_packet();
            eventBus_->publish(eventFullStatus);

            auto eventHealthStatus = std::make_shared<CmsDispatchTopicPacketEvent>();
            eventHealthStatus->dispatchTopic = Topics::LRAS_MULTI_health_status_INS;
            eventHealthStatus->packet = make_empty_packet();
            eventBus_->publish(eventHealthStatus);            

            periodicMessages();
        }
    });
}

void CmsEntity::sendLRAS_CS_lrad_1_status_INS(const EventBus::EventPtr& event) const {
    (void)event;

    // Use default StateUpdate for LRAD 1
    const StateUpdate state = StateUpdate{};
    const RawPacket packet = build_lrad_status_packet(state, MessageId_LRAS_CS_lrad_1_status_INS);
    send_multicast_packet(packet, "LRAS_CS_lrad_1_status_INS");
}  

void CmsEntity::sendLRAS_CS_lrad_2_status_INS(const EventBus::EventPtr& event) const {
    (void)event;

    // Use default StateUpdate for LRAD 2
    const StateUpdate state = StateUpdate{};
    const RawPacket packet = build_lrad_status_packet(state, MessageId_LRAS_CS_lrad_2_status_INS);
    send_multicast_packet(packet, "LRAS_CS_lrad_2_status_INS");
}

void CmsEntity::sendLRAS_MULTI_full_status_v2_INS(const EventBus::EventPtr& event) const {
    (void)event;

    // Use default StateUpdate for both LRADs
    const StateUpdate state1 = StateUpdate{};
    const StateUpdate state2 = StateUpdate{};

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_MULTI_full_status_v2_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_MULTI_full_status_v2_INS);
    append_u32_be(packet.data, MessageLength_LRAS_MULTI_full_status_v2_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // LRAD 1 full status (44 bytes)
    append_lrad_full_status(packet.data, state1);
    // LRAD 2 full status (44 bytes)
    append_lrad_full_status(packet.data, state2);

    send_multicast_packet(packet, "LRAS_MULTI_full_status_v2_INS");
}

void CmsEntity::sendLRAS_MULTI_health_status_INS(const EventBus::EventPtr& event) const {
    (void)event;

    // Use default StateUpdate and SystemHealthUpdate for both LRADs
    const StateUpdate state1 = StateUpdate{};
    const StateUpdate state2 = StateUpdate{};
    const SystemHealthUpdate sys = SystemHealthUpdate{};

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_MULTI_health_status_INS);

    // Header (16 bytes)
    append_u32_be(packet.data, MessageId_LRAS_MULTI_health_status_INS);
    append_u32_be(packet.data, MessageLength_LRAS_MULTI_health_status_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // System-level fields (6 bytes)
    append_u16_be(packet.data, sys.lrasCondition.value_or(0));
    append_u16_be(packet.data, sys.lrasOperativeState.value_or(0));
    append_u16_be(packet.data, sys.laserDazzlerMainAuth.value_or(0));

    // LRAD 1 health (856 bytes)
    append_lrad_health_block(packet.data, state1);
    // LRAD 2 health (856 bytes)
    append_lrad_health_block(packet.data, state2);

    // LRAS Server Status + Console statuses (10 bytes)
    append_u16_be(packet.data, sys.lrasServerStatus.value_or(0));
    append_u16_be(packet.data, sys.console1Health.value_or(0));
    append_u16_be(packet.data, sys.console1ControlledLrad.value_or(0));
    append_u16_be(packet.data, sys.console2Health.value_or(0));
    append_u16_be(packet.data, sys.console2ControlledLrad.value_or(0));

    send_multicast_packet(packet, "LRAS_MULTI_health_status_INS");
}
