#include "CmsEntity.hpp"

#include "AcsEntity.hpp"
#include "Topics.hpp"
#include "UdpSocket.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#

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
constexpr uint32_t MessageId_LRAS_CS_change_configuration_request_INS = 576978946;
constexpr uint32_t MessageLength_LRAS_CS_change_configuration_request_INS = 4; // LRAD ID(2) + Configuration(2)
constexpr uint32_t MessageId_LRAS_CS_emission_mode_feedback_INS = 576978955;
constexpr uint32_t MessageLength_LRAS_CS_emission_mode_feedback_INS = 32; // ActionId(4)+LRAD ID(2)+AudioEnable(2)+Levels(12)+LaserEnable(2)+LaserMinDist(4)+LightEnable(2)+LightMaxW(2)+LRFEnable(2)
constexpr uint32_t MessageId_LRAS_CS_engagement_capability_INS = 576978947;
constexpr uint32_t MessageLength_LRAS_CS_engagement_capability_INS = 80; // ActionId(4)+CSTN(4)+LRAD1(36)+LRAD2(36)
constexpr uint32_t MessageId_LRAS_CS_hw_limit_warning_INS = 576978948;
constexpr uint32_t MessageLength_LRAS_CS_hw_limit_warning_INS = 6; // LRAD ID(2) + Limit(4)
constexpr uint32_t MessageId_LRAS_CS_installation_data_INS = 576978956;
constexpr uint32_t MessageLength_LRAS_CS_installation_data_INS = 44; // ActionId(4)+LRAD1(20)+LRAD2(20)
constexpr uint32_t MessageId_LRAS_CS_message_table_INS = 576978951;
constexpr uint32_t MessageId_LRAS_CS_software_version_INS = 576978952;
constexpr uint32_t MessageLength_LRAS_CS_software_version_INS = 288; // 18 fixed strings x 16 bytes
constexpr uint32_t MessageId_LRAS_CS_thresholds_INS = 576978953;
constexpr uint32_t MessageLength_LRAS_CS_thresholds_INS = 60; // ActionId(4)+LRAD1(28)+LRAD2(28)
constexpr uint32_t MessageId_LRAS_CS_translation_INS = 576978954;
constexpr uint32_t MessageLength_LRAS_CS_translation_INS = 780; // ActionId(4)+LRAD ID(2)+Status(2)+FreeText(772)
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
constexpr uint32_t MessageId_NAVS_MULTI_gyro_fore_nav_data_10ms_INS = 425920942;
constexpr uint32_t MessageId_NAVS_MULTI_health_status_INS = 425920943;
constexpr uint32_t MessageId_NAVS_MULTI_nav_data_100ms_INS = 425920944;
constexpr uint32_t MessageId_NAVS_MULTI_ships_admin_force_time_INS = 425920947;
constexpr uint32_t MessageLength_LRAS_CS_lrad_status_INS = 32;
constexpr uint32_t MessageId_LRAS_MULTI_full_status_v2_INS = 576913411;
constexpr uint32_t MessageLength_LRAS_MULTI_full_status_v2_INS = 88; // 2 LRADs x 44 bytes each
constexpr uint32_t MessageId_LRAS_MULTI_health_status_INS = 576913410;
constexpr uint32_t MessageLength_LRAS_MULTI_health_status_INS = 1728; // 6 (sys) + 2x856 (LRADs) + 10 (consoles)
constexpr const char* LrasStatusMulticastGroup = "226.1.1.43";
constexpr uint16_t LrasStatusMulticastPort = 55010;

float normalize_0_360(float angleDeg) {
    (void)angleDeg;
    return 0.0f;
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

uint16_t derive_lrad_status() {
    return 1;
}

uint16_t derive_lrad_mode() {
    return 1;
}

uint16_t derive_cueing_status() {
    return 0;
}

uint16_t derive_video_tracking_status() {
    return 0;
}

int16_t derive_lrf_distance() {
    return static_cast<int16_t>(-1);
}

uint16_t derive_inhibition_sector_flag() {
    return 0;
}

uint16_t derive_laser_dazzler_mode() {
    return 0;
}

RawPacket build_lrad_status_packet(uint32_t messageId) {
    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_lrad_status_INS);

    append_u32_be(packet.data, messageId);
    append_u32_be(packet.data, MessageLength_LRAS_CS_lrad_status_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    append_u16_be(packet.data, derive_lrad_status());
    append_u16_be(packet.data, derive_lrad_mode());
    append_u16_be(packet.data, derive_cueing_status());
    append_u16_be(packet.data, derive_video_tracking_status());
    append_f32_be(packet.data, 0.0f);
    append_f32_be(packet.data, 0.0f);
    append_i16_be(packet.data, derive_lrf_distance());
    append_u16_be(packet.data, derive_inhibition_sector_flag());
    append_u16_be(packet.data, 0);
    append_u16_be(packet.data, 0);
    append_u16_be(packet.data, derive_laser_dazzler_mode());
    append_u16_be(packet.data, 0);
    append_u16_be(packet.data, 0);
    append_u16_be(packet.data, 0);

    return packet;
}

// Appends the 44-byte "LRAD x full status" block (Lrad_full 40-byte struct + IR camera 4 bytes)
void append_lrad_full_status(std::vector<uint8_t>& buffer) {
    // Lrad_full (40 bytes)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Audio Emitter
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Searchlight
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Laser Dazzler
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // LRF
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Remaining Lrad_full fields
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Outside Lrad_full but within LRAD x full status block (4 bytes)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
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
void append_lrad_health_block(std::vector<uint8_t>& buffer) {
    // Pre-AudioMode fields (10 bytes)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);

    // Audio Mode struct (792 bytes)
    // Volume Mode (10 bytes)
    append_u16_be(buffer, 0);
    append_f32_be(buffer, 0.0f);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Recorded Message-Tone (8 bytes)
    append_u32_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    // Free Text (774 bytes): Language in(2) + Language out(2) + text(768) + Loop(2)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    {
        // message text: fixed 768-byte field, zero-padded
        constexpr std::size_t textSize = 768;
        buffer.insert(buffer.end(), textSize, 0x00);
    }
    append_u16_be(buffer, 0);

    // Searchlight Condition + Mode (6 bytes)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);   // Light Power
    append_u16_be(buffer, 0);   // Light Zoom

    // Laser Dazzler, LRF, Camera (10 bytes)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);

    // IMU, Recorder (10 bytes + 8 bytes time limit)
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u16_be(buffer, 0);
    append_u32_be(buffer, 0);
    append_u32_be(buffer, 0);

    // Horizontal Reference (2 bytes)
    append_u16_be(buffer, 0);

    // Inhibit Sector 1 (10 bytes)
    append_u16_be(buffer, 0);
    append_f32_be(buffer, 0.0f);
    append_f32_be(buffer, 0.0f);

    // Inhibit Sector 2 (10 bytes)
    append_u16_be(buffer, 0);
    append_f32_be(buffer, 0.0f);
    append_f32_be(buffer, 0.0f);
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

std::string bytes_to_hex(const std::vector<uint8_t>& data, std::size_t offset) {
    std::ostringstream stream;
    stream << std::hex;
    for (std::size_t i = offset; i < data.size(); ++i) {
        stream.width(2);
        stream.fill('0');
        stream << static_cast<int>(data[i]);
    }
    return stream.str();
}

json make_empty_payload() {
    return json();
}

// Helper function: assume LRAD 1 and 2 are always known
bool has_known_lrad(uint16_t lradId) {
    return (lradId == 1 || lradId == 2);
}

} // namespace

CmsEntity::CmsEntity(const CmsConfig& config)
    : config_(config),
      rxIoContext_(),
    rxWorkGuard_(std::nullopt),
    periodicTimer_(std::nullopt),
    udpSocket_(nullptr) {
}

void CmsEntity::subscribeTopics() {
    // EventBus removed: publication is now handled via messageCallback_.
    // Nothing to subscribe to in this entity.
}

void CmsEntity::start() {
    if (!subscribed_.exchange(true)) {
        subscribeTopics();
    }

    std::vector<MulticastEndpoint> multicastEndpoints;
    multicastEndpoints.reserve(config_.multicast_groups.size());
    for (const auto& group : config_.multicast_groups) {
        multicastEndpoints.push_back(MulticastEndpoint{group, config_.multicast_port});
    }

    udpSocket_ = std::make_shared<UdpSocket>(
        rxIoContext_,
        "0.0.0.0", //useless, receiver will bind to the multicast group address directly
        multicastEndpoints
    );

    udpSocket_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    udpSocket_->start();

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    boost::asio::post(rxIoContext_, [this]() {
        periodicMessages(); // Commentare per test.
    });

    running_.store(true);
     

    std::cout << "[CMS Entity] Avviata su " << config_.multicast_groups.size()
              << " gruppo/i multicast, porta " << config_.multicast_port << std::endl;
}

void CmsEntity::stop() {
    running_.store(false);

    if (udpSocket_) {
        udpSocket_->stop();
    }

    if (periodicTimer_.has_value()) {
        periodicTimer_->cancel();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}


void CmsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!messageCallback_) {
        return;
    }

    const uint32_t sourceMessageId = extract_message_id_from_header(packet);
    std::string publishTopic;
    nlohmann::json publishMessage;
    if (!convertIncomingPacket(packet, publishTopic, publishMessage)) {
        std::cerr << "[CMS Entity] Messaggio ignorato: source_id=" << sourceMessageId << std::endl;
        return;
    }

    messageCallback_(publishTopic, publishMessage);
    
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

bool CmsEntity::convertIncomingPacket(const RawPacket& packet,
                                      std::string& outTopic,
                                      nlohmann::json& outMessage) const {
    using ParserFn = json (CmsEntity::*)(const RawPacket&, uint16_t&, uint16_t&) const;

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
        { MessageId_CS_MULTI_update_cst_kinematics_INS, { &CmsEntity::parse_CS_MULTI_update_cst_kinematics_INS, Topics::CS_MULTI_update_cst_kinematics_INS } },
        { MessageId_NAVS_MULTI_gyro_fore_nav_data_10ms_INS, { &CmsEntity::parse_NAVS_MULTI_gyro_fore_nav_data_10ms_INS, Topics::NAVS_MULTI_gyro_fore_nav_data_10ms_INS } },
        { MessageId_NAVS_MULTI_health_status_INS, { &CmsEntity::parse_NAVS_MULTI_health_status_INS, Topics::NAVS_MULTI_health_status_INS } },
        { MessageId_NAVS_MULTI_nav_data_100ms_INS, { &CmsEntity::parse_NAVS_MULTI_nav_data_100ms_INS, Topics::NAVS_MULTI_nav_data_100ms_INS } },
        { MessageId_NAVS_MULTI_ships_admin_force_time_INS, { &CmsEntity::parse_NAVS_MULTI_ships_admin_force_time_INS, Topics::NAVS_MULTI_ships_admin_force_time_INS } }
    };

    ParsedHeader header;
    if (!parseHeader(packet, header)) {
        return false;
    }

    uint16_t destinationLradId = 0;
    uint16_t nackreason = 0;
    nlohmann::json payload;

    switch (header.messageId) {
        case MessageId_CS_LRAS_change_configuration_order_INS:
            payload = parse_CS_LRAS_change_configuration_order_INS(packet, destinationLradId, nackreason);
            outTopic = Topics::CS_LRAS_change_configuration_order_INS;
            break;
        case MessageId_CS_LRAS_cueing_order_cancellation_INS:
            payload = parse_CS_LRAS_cueing_order_cancellation_INS(packet, destinationLradId, nackreason);
            outTopic = Topics::CS_LRAS_cueing_order_cancellation_INS;
            break;
        case MessageId_CS_LRAS_cueing_order_INS:
            payload = parse_CS_LRAS_cueing_order_INS(packet, destinationLradId, nackreason);
            outTopic = Topics::CS_LRAS_cueing_order_INS;
            break;
        case MessageId_CS_LRAS_emission_control_INS:
            payload = parse_CS_LRAS_emission_control_INS(packet, destinationLradId, nackreason);
            outTopic = Topics::CS_LRAS_emission_control_INS;
            break;
        default:
            {
                const auto bindingIt = additionalParserBindings.find(header.messageId);
                if (bindingIt != additionalParserBindings.end()) {
                    payload = (this->*(bindingIt->second.parser))(packet, destinationLradId, nackreason);
                    outTopic = bindingIt->second.topic;
                }
            }
            break;
    }

    if (payload.is_null()) {
        return false;
    }

    payload["destinationLradId"] = destinationLradId;
    payload["nackreason"] = nackreason;
    outMessage = std::move(payload);
    return true;
}

json CmsEntity::parse_CS_LRAS_change_configuration_order_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t offset = 16;
    constexpr std::size_t blockSize = 8;

    if (offset + blockSize > packet.data.size()) {
        return make_empty_payload();
    }

    const uint16_t actionId = read_u16_be(packet.data, offset);
    const uint16_t lradId = read_u16_be(packet.data, offset + 4);
    const uint16_t rawConfig = read_u16_be(packet.data, offset + 6);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = lradId;
    payload["Configuration"] = rawConfig;


    return payload;
}

json CmsEntity::parse_CS_LRAS_cueing_order_cancellation_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t offset = 16;
    constexpr std::size_t blockSize = 6;
    if (offset + blockSize > packet.data.size()) {
        return make_empty_payload();
    }

    const uint32_t actionId = read_u32_be(packet.data, offset);
    const uint16_t lradId = read_u16_be(packet.data, offset + 4);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = std::to_string(lradId);

    destinationLradId = lradId;

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }
    // Assume LRAD is operativefor LRAD 1 and 2
    return payload;
}

json CmsEntity::parse_CS_LRAS_cueing_order_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t minPayloadSize = 22;
    if (packet.data.size() < HeaderSize + minPayloadSize) {
        return make_empty_payload();
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
        // CueingMath removed: keep default azimuth/elevation values.
        azimuthDeg = 0.0f;
        elevationDeg = 0.0f;
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

    destinationLradId = lradId;

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }
    // Assume LRAD is operative for LRAD 1 and 2
    return payload;
}

json CmsEntity::parse_CS_LRAS_emission_control_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    if (packet.data.size() < 838) {
        return make_empty_payload();
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
    payload["LRAD ID"] = lradId;
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
    payload["Laser Mode"] = laserMode;
    payload["lightModeValidity"] = lightModeValidity;
    payload["Light Power"] = lightPower;
    payload["Light Zoom"] = lightZoom;
    payload["lrfModeValidity"] = lrfModeValidity;
    payload["LRF On Off"] = lrfOnOff;
    payload["cameraZoomValidity"] = cameraZoomValidity;
    payload["Camera Zoom"] = cameraZoom;
    payload["horizontalReferenceValidity"] = horizontalReferenceValidity;
    payload["Horizontal Reference"] = horizontalReference;

    destinationLradId = lradId;

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }
    return payload;
}

json CmsEntity::parse_CS_LRAS_emission_mode_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    (void)packet;
    (void)destinationLradId;
    (void)nackreason;
    return json::object();
}

json CmsEntity::parse_CS_LRAS_inhibition_sectors_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 42 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + Sector1(10) + Sector2(10)
    constexpr std::size_t minPacketSize = 42;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
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

    destinationLradId = lradId;

    if ((sector1OnOff != 0 && sector1OnOff != 1) || (sector2OnOff != 0 && sector2OnOff != 1)) {
        nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_joystick_control_lrad_1_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 20 bytes): Header(16) + X(2) + Y(2)
    constexpr std::size_t minPacketSize = 20;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    const int16_t xPosition = read_i16_be(packet.data, 16);
    const int16_t yPosition = read_i16_be(packet.data, 18);

    json payload;
    payload["LRAD ID"] = 1;
    payload["xPosition"] = xPosition;
    payload["yPosition"] = yPosition;

    destinationLradId = 1;

    if (!has_known_lrad(1)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_joystick_control_lrad_2_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 20 bytes): Header(16) + X(2) + Y(2)
    constexpr std::size_t minPacketSize = 20;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    const int16_t xPosition = read_i16_be(packet.data, 16);
    const int16_t yPosition = read_i16_be(packet.data, 18);

    json payload;
    payload["LRAD ID"] = 2;
    payload["xPosition"] = xPosition;
    payload["yPosition"] = yPosition;

    destinationLradId = 2;

    if (!has_known_lrad(2)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_recording_command_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 36 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + Video source(2) + Video profile(2)
    // + Recording mode(2) + Elapsed seconds(4) + Elapsed micro seconds(4)
    constexpr std::size_t minPacketSize = 36;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
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
    payload["Video source"] = videoSource;
    payload["Video Profile"] = videoProfile;
    payload["Recording mode"] = recordingMode;
    payload["Elapsed seconds"] = elapsedSeconds;
    payload["Elapsed micro seconds"] = elapsedMicroseconds;

    destinationLradId = lradId;

    if (videoSource != 1 ||
        (videoProfile < 1 || videoProfile > 4) ||
        (recordingMode > 2) ||
        (elapsedSeconds > 2147483647U) ||
        (elapsedMicroseconds > 999999U)) {
        nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_request_emission_mode_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    (void)packet;
    (void)nackreason;

    json payload;
    payload["Action Id"] = 1234;
    payload["LRAD ID"] = 1; 

    destinationLradId = 1;
    return payload;
}

json CmsEntity::parse_CS_LRAS_request_engagement_capability_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 60 bytes):
    // Header(16) + ActionId(4) + CSTN(4) + CST Kinematics(36)
    constexpr std::size_t minPacketSize = 60;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

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

    if ((cstn < 1 || cstn > 9999) ||
        (validitySeconds > 2147483647U) ||
        (validityMicroseconds > 999999U) ||
        (kinematicsType < 1 || kinematicsType > 11)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_request_full_status_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    (void)packet;
    (void)destinationLradId;
    (void)nackreason;
    return json::object();
}

json CmsEntity::parse_CS_LRAS_request_installation_data_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 20 bytes): Header(16) + ActionId(4)
    constexpr std::size_t minPacketSize = 20;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;
    (void)nackreason;

    const uint32_t actionId = read_u32_be(packet.data, 16);

    json payload;
    payload["Action Id"] = actionId;

    return payload;
}

json CmsEntity::parse_CS_LRAS_request_message_table_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    (void)packet;
    (void)destinationLradId;
    (void)nackreason;
    return json::object();
}

json CmsEntity::parse_CS_LRAS_request_software_version_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    (void)packet;
    (void)destinationLradId;
    (void)nackreason;
    return json::object();
}

json CmsEntity::parse_CS_LRAS_request_thresholds_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 28 bytes):
    // Header(16) + ActionId(4) + Volume selector(2) + Audio Volume dB(4) + Scenario(2)
    constexpr std::size_t minPacketSize = 28;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t volumeSelector = read_u16_be(packet.data, 20);
    const float audioVolumeDb = read_f32_be(packet.data, 22);
    const uint16_t scenario = read_u16_be(packet.data, 26);

    json payload;
    payload["Action Id"] = actionId;
    payload["volumeSelector"] = volumeSelector;
    payload["audioVolumeDb"] = audioVolumeDb;
    payload["scenario"] = scenario;

    if ((volumeSelector > 1) ||
        (scenario > 2) ||
        (volumeSelector == 1 && (audioVolumeDb < -128.0f || audioVolumeDb > 0.0f))) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_request_translation_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 794 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + FreeText(772)
    // FreeText = LanguageIn(2) + LanguageOut(2) + MessageText(768)
    constexpr std::size_t minPacketSize = 794;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
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

    destinationLradId = lradId;

    // Spec notes: LanguageIn valid only Italian/English, LanguageOut tone not valid.
    if ((lradId != 1 && lradId != 2) ||
        (languageIn != 0 && languageIn != 1) ||
        (languageOut != 0 && languageOut != 1 && languageOut != 2)) {
        nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_LRAS_video_tracking_command_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 24 bytes):
    // Header(16) + ActionId(4) + LRAD ID(2) + Auto tracking(2)
    constexpr std::size_t minPacketSize = 24;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t autoTracking = read_u16_be(packet.data, 22);

    json payload;
    payload["Action Id"] = actionId;
    payload["LRAD ID"] = lradId;
    payload["autoTracking"] = autoTracking;

    destinationLradId = lradId;

    if ((lradId != 1 && lradId != 2) || (autoTracking != 0 && autoTracking != 1)) {
        nackreason = 2;
    }

    if (!has_known_lrad(lradId)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_MULTI_health_status_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 24 bytes):
    // Header(16) + CS status(2) + DRMU status(2) + Spare(2) + CSS status(2)
    constexpr std::size_t minPacketSize = 24;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

    const uint16_t csStatus = read_u16_be(packet.data, 16);
    const uint16_t drmuStatus = read_u16_be(packet.data, 18);
    const uint16_t spare = read_u16_be(packet.data, 20);
    const uint16_t cssStatus = read_u16_be(packet.data, 22);

    json payload;
    payload["csStatus"] = csStatus;
    payload["drmuStatus"] = drmuStatus;
    payload["spare"] = spare;
    payload["cssStatus"] = cssStatus;

    if ((csStatus < 1 || csStatus > 3) ||
        (drmuStatus < 1 || drmuStatus > 3) ||
        (cssStatus < 1 || cssStatus > 2)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_CS_MULTI_update_cst_kinematics_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    // Message layout (total 56 bytes):
    // Header(16) + CSTN(4) + TimeOfValidity(8) + Kinematics(28)
    // Kinematics = KinematicsType(2) + union data (up to 26 bytes)
    constexpr std::size_t minPacketSize = 56;
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

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

    if ((cstn < 1 || cstn > 9999) ||
        (validitySeconds > 2147483647U) ||
        (validityMicroseconds > 999999U) ||
        (kinematicsType < 1 || kinematicsType > 11)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_NAVS_MULTI_gyro_fore_nav_data_10ms_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t minPacketSize = 66; // Header(16) + payload(50)
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

    const uint16_t sensorReference = read_u16_be(packet.data, 16);
    const uint16_t gyroReadinessState = read_u16_be(packet.data, 18);
    const uint16_t gyroAvailability = read_u16_be(packet.data, 20);
    const uint32_t timeSeconds = read_u32_be(packet.data, 22);
    const uint32_t timeMicroseconds = read_u32_be(packet.data, 26);

    const float heading = read_f32_be(packet.data, 30);
    const float relativeRoll = read_f32_be(packet.data, 34);
    const float absolutePitch = read_f32_be(packet.data, 38);
    const float headingRate = read_f32_be(packet.data, 42);
    const float relativeRollRate = read_f32_be(packet.data, 46);
    const float absolutePitchRate = read_f32_be(packet.data, 50);
    const float northVelocity = read_f32_be(packet.data, 54);
    const float eastVelocity = read_f32_be(packet.data, 58);
    const float verticalVelocity = read_f32_be(packet.data, 62);

    json payload;
    payload["sensor_reference"] = sensorReference;
    payload["gyro_readiness_state"] = gyroReadinessState;
    payload["gyro_availability"] = gyroAvailability;
    payload["time_of_validity"] = {
        {"seconds", timeSeconds},
        {"microseconds", timeMicroseconds}
    };
    payload["ship_attitude"] = {
        {"heading_rad", heading},
        {"relative_roll_rad", relativeRoll},
        {"absolute_pitch_rad", absolutePitch},
        {"heading_rate_rad_s", headingRate},
        {"relative_roll_rate_rad_s", relativeRollRate},
        {"absolute_pitch_rate_rad_s", absolutePitchRate},
        {"north_velocity_m_s", northVelocity},
        {"east_velocity_m_s", eastVelocity},
        {"vertical_velocity_m_s", verticalVelocity}
    };

    if ((sensorReference > 1) ||
        (gyroReadinessState > 2) ||
        (gyroAvailability > 2)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_NAVS_MULTI_health_status_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t minPacketSize = 20; // Header(16) + Readiness State(2) + Availability(2)
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

    const uint16_t readinessState = read_u16_be(packet.data, 16);
    const uint16_t availability = read_u16_be(packet.data, 18);

    json payload;
    payload["readiness_state"] = readinessState;
    payload["availability"] = availability;

    if ((readinessState > 1) ||
        (availability > 2)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_NAVS_MULTI_nav_data_100ms_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t minPacketSize = 116; // Header(16) + payload(100)
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

    const uint32_t timeSeconds = read_u32_be(packet.data, 16);
    const uint32_t timeMicroseconds = read_u32_be(packet.data, 20);

    // Ship position
    const float latitude = read_f32_be(packet.data, 24);
    const float longitude = read_f32_be(packet.data, 28);
    const float logSpeed = read_f32_be(packet.data, 32);
    const float courseMadeGood = read_f32_be(packet.data, 36);
    const float speedOverGround = read_f32_be(packet.data, 40);
    const float set = read_f32_be(packet.data, 44);
    const float drift = read_f32_be(packet.data, 48);
    const float latitudeAccuracy = read_f32_be(packet.data, 52);
    const float longitudeAccuracy = read_f32_be(packet.data, 56);

    // Ship attitude
    const float heading = read_f32_be(packet.data, 60);
    const float relativeRoll = read_f32_be(packet.data, 64);
    const float absolutePitch = read_f32_be(packet.data, 68);
    const float headingRate = read_f32_be(packet.data, 72);
    const float relativeRollRate = read_f32_be(packet.data, 76);
    const float absolutePitchRate = read_f32_be(packet.data, 80);
    const float northVelocity = read_f32_be(packet.data, 84);
    const float eastVelocity = read_f32_be(packet.data, 88);
    const float verticalVelocity = read_f32_be(packet.data, 92);
    const float shipHeave = read_f32_be(packet.data, 96);
    const float waterDepth = read_f32_be(packet.data, 100);

    // Validity enums
    const uint16_t attitudeVelocitiesValidity = read_u16_be(packet.data, 104);
    const uint16_t headingValidity = read_u16_be(packet.data, 106);
    const uint16_t courseSpeedValidity = read_u16_be(packet.data, 108);
    const uint16_t positionValidity = read_u16_be(packet.data, 110);
    const uint16_t setDriftValidity = read_u16_be(packet.data, 112);
    const uint16_t waterDepthValidity = read_u16_be(packet.data, 114);

    json payload;
    payload["time_of_validity"] = {
        {"seconds", timeSeconds},
        {"microseconds", timeMicroseconds}
    };
    payload["ship_position"] = {
        {"latitude_deg", latitude},
        {"longitude_deg", longitude},
        {"log_speed_m_s", logSpeed},
        {"course_made_good_rad", courseMadeGood},
        {"speed_over_ground_m_s", speedOverGround},
        {"set_rad", set},
        {"drift_m_s", drift},
        {"latitude_accuracy_nm", latitudeAccuracy},
        {"longitude_accuracy_nm", longitudeAccuracy}
    };
    payload["ship_attitude"] = {
        {"heading_rad", heading},
        {"relative_roll_rad", relativeRoll},
        {"absolute_pitch_rad", absolutePitch},
        {"heading_rate_rad_s", headingRate},
        {"relative_roll_rate_rad_s", relativeRollRate},
        {"absolute_pitch_rate_rad_s", absolutePitchRate},
        {"north_velocity_m_s", northVelocity},
        {"east_velocity_m_s", eastVelocity},
        {"vertical_velocity_m_s", verticalVelocity},
        {"ship_heave_m", shipHeave},
        {"water_depth_m", waterDepth}
    };
    payload["validity"] = {
        {"attitude_velocities", attitudeVelocitiesValidity},
        {"heading", headingValidity},
        {"course_speed", courseSpeedValidity},
        {"position", positionValidity},
        {"set_drift", setDriftValidity},
        {"water_depth", waterDepthValidity}
    };

    // Validate all enum fields (0-2 range)
    if ((attitudeVelocitiesValidity > 2) ||
        (headingValidity > 2) ||
        (courseSpeedValidity > 2) ||
        (positionValidity > 2) ||
        (setDriftValidity > 2) ||
        (waterDepthValidity > 2)) {
        nackreason = 2;
    }

    return payload;
}

json CmsEntity::parse_NAVS_MULTI_ships_admin_force_time_INS(
    const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const {
    constexpr std::size_t minPacketSize = 62; // Header(16) + payload(46)
    if (packet.data.size() < minPacketSize) {
        return make_empty_payload();
    }

    (void)destinationLradId;

    // Helper lambda to read 2-byte ASCII string
    auto read_ascii_2 = [&packet](std::size_t offset) -> std::string {
        if (offset + 2 > packet.data.size()) {
            return "";
        }
        return std::string(
            reinterpret_cast<const char*>(packet.data.data() + offset),
            2
        );
    };

    // Force Time (18 bytes)
    const std::string forceCentury = read_ascii_2(16);
    const std::string forceYear = read_ascii_2(18);
    const std::string forceMonth = read_ascii_2(20);
    const std::string forceDay = read_ascii_2(22);
    const std::string forceHour = read_ascii_2(24);
    const std::string forceMinute = read_ascii_2(26);
    const std::string forceSecond = read_ascii_2(28);
    const std::string forceHundredth = read_ascii_2(30);
    const std::string forceTimeZone = read_ascii_2(32);

    // Admin Time (18 bytes)
    const std::string adminCentury = read_ascii_2(34);
    const std::string adminYear = read_ascii_2(36);
    const std::string adminMonth = read_ascii_2(38);
    const std::string adminDay = read_ascii_2(40);
    const std::string adminHour = read_ascii_2(42);
    const std::string adminMinute = read_ascii_2(44);
    const std::string adminSecond = read_ascii_2(46);
    const std::string adminHundredth = read_ascii_2(48);
    const std::string adminTimeZone = read_ascii_2(50);

    // Time of measurement
    const uint32_t measurementSeconds = read_u32_be(packet.data, 52);
    const uint32_t measurementMicroseconds = read_u32_be(packet.data, 56);

    // Time source enum
    const uint16_t timeSource = read_u16_be(packet.data, 60);

    json payload;
    payload["force_time"] = {
        {"century", forceCentury},
        {"year", forceYear},
        {"month", forceMonth},
        {"day", forceDay},
        {"hour", forceHour},
        {"minute", forceMinute},
        {"second", forceSecond},
        {"hundredth", forceHundredth},
        {"time_zone", forceTimeZone}
    };
    payload["admin_time"] = {
        {"century", adminCentury},
        {"year", adminYear},
        {"month", adminMonth},
        {"day", adminDay},
        {"hour", adminHour},
        {"minute", adminMinute},
        {"second", adminSecond},
        {"hundredth", adminHundredth},
        {"time_zone", adminTimeZone}
    };
    payload["time_of_measurement"] = {
        {"seconds", measurementSeconds},
        {"microseconds", measurementMicroseconds}
    };
    payload["time_source"] = timeSource;

    if (timeSource > 1) {
        nackreason = 2;
    }

    return payload;
}

void CmsEntity::sendLRAS_CS_ack_INS(const std::string& topic, uint16_t nackreason, const nlohmann::json& message) const {
    const uint32_t sourceMessageId = source_message_id_from_topic(topic);
    if (sourceMessageId == 0) {
        std::cerr << "[CMS Entity] Impossibile determinare source_message_id per ACK: topic="
                  << topic << std::endl;
        return;
    }

    const json& payload = message;
    if (payload.is_null()) {
        std::cerr << "[CMS Entity] Payload mancante per ACK LRAS_CS_ack_INS"
                  << std::endl;
        return;
    }

    const auto actionId = extract_action_id(payload);
    if (!actionId.has_value()) {
        std::cerr << "[CMS Entity] Action Id mancante nel payload per ACK LRAS_CS_ack_INS"
                  << std::endl;
        return;
    }

    uint16_t ackNackAccepted = 1; // ACK accepted, no NACK reason
    const uint16_t nackReason = nackreason;
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

    sendMulticastPacket(ackPacket, "ACK multicast");
}

void CmsEntity::periodicMessages() {
    if (!messageCallback_) {
        return;
    }

    if (!periodicTimer_.has_value()) {
        periodicTimer_.emplace(rxIoContext_);
    }

    periodicTimer_->expires_after(std::chrono::milliseconds(100));
    periodicTimer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            messageCallback_(Topics::LRAS_CS_lrad_1_status_INS, nlohmann::json::object());
            messageCallback_(Topics::LRAS_CS_lrad_2_status_INS, nlohmann::json::object());
            messageCallback_(Topics::LRAS_MULTI_full_status_v2_INS, nlohmann::json::object());
            messageCallback_(Topics::LRAS_MULTI_health_status_INS, nlohmann::json::object());

            periodicMessages();
        }
    });
}

void CmsEntity::setMessageCallback(std::function<void(const std::string&, const nlohmann::json&)> cb) {
    messageCallback_ = std::move(cb);
}

void CmsEntity::sendLRAS_CS_change_configuration_request_INS(const std::string& topic,
                                                             const nlohmann::json& message,
                                                             uint16_t lradId,
                                                             uint16_t configuration) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_change_configuration_request_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_change_configuration_request_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_change_configuration_request_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    append_u16_be(packet.data, lradId);

    append_u16_be(packet.data, configuration);

    sendMulticastPacket(packet, "LRAS_CS_change_configuration_request_INS");
}

void CmsEntity::sendLRAS_CS_emission_mode_feedback_INS(const std::string& topic,
                                                       const nlohmann::json& message,
                                                       uint16_t lradId,
                                                       uint16_t audioEnable,
                                                       float audioLevel1,
                                                       float audioLevel2,
                                                       float audioLevel3,
                                                       uint16_t laserEnable,
                                                       uint32_t laserMinDistance,
                                                       uint16_t lightEnable,
                                                       uint16_t lightMaxW,
                                                       uint16_t lrfEnable) const {
    (void)topic;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_emission_mode_feedback_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_emission_mode_feedback_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_emission_mode_feedback_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // Action Id
    const uint32_t actionId = static_cast<uint32_t>(message.value("action_id", 0));
    append_u32_be(packet.data, actionId);

    // LRAD ID: 1 = LRAD 1 Port, 2 = LRAD 2 Starboard
    append_u16_be(packet.data, lradId);

    // Audio Enable: 0 = Disable, 1 = Enable
    append_u16_be(packet.data, audioEnable);

    // Audio Volume levels (3 x float, dB, [-128..0])
    append_f32_be(packet.data, audioLevel1);
    append_f32_be(packet.data, audioLevel2);
    append_f32_be(packet.data, audioLevel3);

    // Laser Enable: 0 = Disable, 1 = Enable
    append_u16_be(packet.data, laserEnable);

    // Laser Min Distance [100..6000] m
    append_u32_be(packet.data, laserMinDistance);

    // Light Enable: 0 = Disable, 1 = Enable
    append_u16_be(packet.data, lightEnable);

    // Light Max W: 0 = Off, 1 = 35W, 2 = 45W, 3 = 85W
    append_u16_be(packet.data, lightMaxW);

    // Laser Range Finder Enable: 0 = Disable, 1 = Enable
    append_u16_be(packet.data, lrfEnable);

    sendMulticastPacket(packet, "LRAS_CS_emission_mode_feedback_INS");
}

void CmsEntity::sendLRAS_CS_engagement_capability_INS(const std::string& topic,
                                                      const nlohmann::json& message,
                                                      uint32_t cstn,
                                                      uint16_t engagementPossible1,
                                                      uint16_t searchlightCapability1,
                                                      uint16_t warningCapability1,
                                                      float warningMinDb1,
                                                      float warningMaxDb1,
                                                      uint16_t dissuasionCapability1,
                                                      float dissuasionMinDb1,
                                                      float dissuasionMaxDb1,
                                                      uint16_t persuasionCapability1,
                                                      float persuasionMinDb1,
                                                      float persuasionMaxDb1,
                                                      uint16_t laserDazzlerCapability1,
                                                      uint16_t engagementPossible2,
                                                      uint16_t searchlightCapability2,
                                                      uint16_t warningCapability2,
                                                      float warningMinDb2,
                                                      float warningMaxDb2,
                                                      uint16_t dissuasionCapability2,
                                                      float dissuasionMinDb2,
                                                      float dissuasionMaxDb2,
                                                      uint16_t persuasionCapability2,
                                                      float persuasionMinDb2,
                                                      float persuasionMaxDb2,
                                                      uint16_t laserDazzlerCapability2) const {
    (void)topic;
    (void)message;

    // Helper lambda: appends one 36-byte LRAD capability block
    auto append_lrad_capability = [this](std::vector<uint8_t>& buf,
                                         uint16_t engagementPossible,
                                         uint16_t searchlightCapability,
                                         uint16_t warningCapability,
                                         float warningMinDb,
                                         float warningMaxDb,
                                         uint16_t dissuasionCapability,
                                         float dissuasionMinDb,
                                         float dissuasionMaxDb,
                                         uint16_t persuasionCapability,
                                         float persuasionMinDb,
                                         float persuasionMaxDb,
                                         uint16_t laserDazzlerCapability) {
        // Engagement possible: 0=unknown,1=not attainable,2=LRAD not available,3=CST in blind arc,4=engagement possible
        append_u16_be(buf, engagementPossible);

        // Searchlight capability: 0=FALSE, 1=TRUE
        append_u16_be(buf, searchlightCapability);

        // Warning capability (10 bytes): capability(2) + min db(4) + max db(4)
        append_u16_be(buf, warningCapability);
        append_f32_be(buf, warningMinDb);
        append_f32_be(buf, warningMaxDb);

        // Dissuasion capability (10 bytes): capability(2) + min db(4) + max db(4)
        append_u16_be(buf, dissuasionCapability);
        append_f32_be(buf, dissuasionMinDb);
        append_f32_be(buf, dissuasionMaxDb);

        // Persuasion capability (10 bytes): capability(2) + min db(4) + max db(4)
        append_u16_be(buf, persuasionCapability);
        append_f32_be(buf, persuasionMinDb);
        append_f32_be(buf, persuasionMaxDb);

        // Laser dazzler capability: 0=FALSE, 1=TRUE
        append_u16_be(buf, laserDazzlerCapability);
    };

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_engagement_capability_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_engagement_capability_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_engagement_capability_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // Action Id
    const uint32_t actionId = static_cast<uint32_t>(message.value("action_id", 0));
    append_u32_be(packet.data, actionId);

    // CSTN - Combat System Track Number [1..9999]
    append_u32_be(packet.data, cstn);

    // LRAD 1 capability block (36 bytes)
    append_lrad_capability(packet.data,
                           engagementPossible1,
                           searchlightCapability1,
                           warningCapability1,
                           warningMinDb1,
                           warningMaxDb1,
                           dissuasionCapability1,
                           dissuasionMinDb1,
                           dissuasionMaxDb1,
                           persuasionCapability1,
                           persuasionMinDb1,
                           persuasionMaxDb1,
                           laserDazzlerCapability1);

    // LRAD 2 capability block (36 bytes)
    append_lrad_capability(packet.data,
                           engagementPossible2,
                           searchlightCapability2,
                           warningCapability2,
                           warningMinDb2,
                           warningMaxDb2,
                           dissuasionCapability2,
                           dissuasionMinDb2,
                           dissuasionMaxDb2,
                           persuasionCapability2,
                           persuasionMinDb2,
                           persuasionMaxDb2,
                           laserDazzlerCapability2);

    sendMulticastPacket(packet, "LRAS_CS_engagement_capability_INS");
}

void CmsEntity::sendLRAS_CS_hw_limit_warning_INS(const std::string& topic,
                                                 const nlohmann::json& message,
                                                 uint16_t lradId,
                                                 int32_t limit) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_hw_limit_warning_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_hw_limit_warning_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_hw_limit_warning_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    append_u16_be(packet.data, lradId);

    // Limit: bordo al quale ci si sta' avvicinando [-180..180] deg
    append_u32_be(packet.data, static_cast<uint32_t>(limit));

    sendMulticastPacket(packet, "LRAS_CS_hw_limit_warning_INS");
}

void CmsEntity::sendLRAS_CS_installation_data_INS(const std::string& topic,
                                                  const nlohmann::json& message,
                                                  float arcStart1,
                                                  float arcStop1,
                                                  float x1,
                                                  float y1,
                                                  float z1,
                                                  float arcStart2,
                                                  float arcStop2,
                                                  float x2,
                                                  float y2,
                                                  float z2) const {
    (void)topic;
    (void)message;

    // Helper lambda: appends one 20-byte LRAD installation data block
    auto append_lrad_inst_data = [this](std::vector<uint8_t>& buf,
                                        float arcStart,
                                        float arcStop,
                                        float x,
                                        float y,
                                        float z) {
        // HW active arc: start and stop [-180..180] deg
        append_f32_be(buf, arcStart);
        append_f32_be(buf, arcStop);

        // Pos_crp: X, Y [-400000..400000] m; Z [-5000..40000] m
        append_f32_be(buf, x);
        append_f32_be(buf, y);
        append_f32_be(buf, z);
    };

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_installation_data_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_installation_data_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_installation_data_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // Action Id
    const uint32_t actionId = static_cast<uint32_t>(message.value("action_id", 0));
    append_u32_be(packet.data, actionId);

    // LRAD 1 installation data (20 bytes)
    append_lrad_inst_data(packet.data, arcStart1, arcStop1, x1, y1, z1);

    // LRAD 2 installation data (20 bytes)
    append_lrad_inst_data(packet.data, arcStart2, arcStop2, x2, y2, z2);

    sendMulticastPacket(packet, "LRAS_CS_installation_data_INS");
}

void CmsEntity::sendLRAS_CS_message_table_INS(const std::string& topic,
                                              const nlohmann::json& message,
                                              uint16_t totalMessagesNumber,
                                              uint16_t messageNumber,
                                              uint16_t dbItemsNumber,
                                              uint32_t messageId,
                                              const std::string& summaryText,
                                              uint16_t numberOfLanguages,
                                              uint32_t recordId,
                                              uint16_t language,
                                              uint8_t associatedAudio,
                                              const std::string& messageText) const {
    (void)topic;
    (void)message;

    const uint32_t payloadLength = 6u + 4u + 32u + 2u +
                                   static_cast<uint32_t>(numberOfLanguages) * (4u + 2u + 1u + 768u);

    RawPacket packet;
    packet.data.reserve(HeaderSize + payloadLength);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_message_table_INS);
    append_u32_be(packet.data, payloadLength + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // Frame-level fields
    append_u16_be(packet.data, totalMessagesNumber);
    append_u16_be(packet.data, messageNumber);
    append_u16_be(packet.data, dbItemsNumber);

    // DB item
    append_u32_be(packet.data, messageId);

    std::vector<uint8_t> summary(32, 0);
    const std::size_t summaryLen = std::min<std::size_t>(summaryText.size(), summary.size());
    std::memcpy(summary.data(), summaryText.data(), summaryLen);
    packet.data.insert(packet.data.end(), summary.begin(), summary.end());

    append_u16_be(packet.data, numberOfLanguages);

    // Text block (1 language placeholder)
    append_u32_be(packet.data, recordId);
    append_u16_be(packet.data, language);
    packet.data.push_back(associatedAudio);

    std::vector<uint8_t> text(768, 0);
    const std::size_t textLen = std::min<std::size_t>(messageText.size(), text.size());
    std::memcpy(text.data(), messageText.data(), textLen);
    packet.data.insert(packet.data.end(), text.begin(), text.end());

    sendMulticastPacket(packet, "LRAS_CS_message_table_INS");
}

void CmsEntity::sendLRAS_CS_software_version_INS(const std::string& topic,
                                                 const nlohmann::json& message,
                                                 const std::string& lrasServerSwName,
                                                 const std::string& lrasServerSwVersion,
                                                 const std::string& lrad1MasterSwName,
                                                 const std::string& lrad1MasterSwVersion,
                                                 const std::string& lrad1SlaveSwName,
                                                 const std::string& lrad1SlaveSwVersion,
                                                 const std::string& lrad1TrackingSwName,
                                                 const std::string& lrad1TrackingSwVersion,
                                                 const std::string& lrad2MasterSwName,
                                                 const std::string& lrad2MasterSwVersion,
                                                 const std::string& lrad2SlaveSwName,
                                                 const std::string& lrad2SlaveSwVersion,
                                                 const std::string& lrad2TrackingSwName,
                                                 const std::string& lrad2TrackingSwVersion,
                                                 const std::string& console1SwName,
                                                 const std::string& console1SwVersion,
                                                 const std::string& console2SwName,
                                                 const std::string& console2SwVersion) const {
    (void)topic;
    (void)message;

    auto append_fixed_string_16 = [](std::vector<uint8_t>& buffer, const std::string& value) {
        std::vector<uint8_t> field(16, 0);
        const std::size_t copyLen = (value.size() < field.size()) ? value.size() : field.size();
        std::memcpy(field.data(), value.data(), copyLen);
        buffer.insert(buffer.end(), field.begin(), field.end());
    };

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_software_version_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_software_version_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_software_version_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // LRAS Server (2 x 16 bytes)
    append_fixed_string_16(packet.data, lrasServerSwName);
    append_fixed_string_16(packet.data, lrasServerSwVersion);

    // LRAD 1 / CPU Master
    append_fixed_string_16(packet.data, lrad1MasterSwName);
    append_fixed_string_16(packet.data, lrad1MasterSwVersion);
    // LRAD 1 / CPU Slave
    append_fixed_string_16(packet.data, lrad1SlaveSwName);
    append_fixed_string_16(packet.data, lrad1SlaveSwVersion);
    // LRAD 1 / CPU Tracking
    append_fixed_string_16(packet.data, lrad1TrackingSwName);
    append_fixed_string_16(packet.data, lrad1TrackingSwVersion);

    // LRAD 2 / CPU Master
    append_fixed_string_16(packet.data, lrad2MasterSwName);
    append_fixed_string_16(packet.data, lrad2MasterSwVersion);
    // LRAD 2 / CPU Slave
    append_fixed_string_16(packet.data, lrad2SlaveSwName);
    append_fixed_string_16(packet.data, lrad2SlaveSwVersion);
    // LRAD 2 / CPU Tracking
    append_fixed_string_16(packet.data, lrad2TrackingSwName);
    append_fixed_string_16(packet.data, lrad2TrackingSwVersion);

    // LRAS Console 1
    append_fixed_string_16(packet.data, console1SwName);
    append_fixed_string_16(packet.data, console1SwVersion);

    // LRAS Console 2
    append_fixed_string_16(packet.data, console2SwName);
    append_fixed_string_16(packet.data, console2SwVersion);

    sendMulticastPacket(packet, "LRAS_CS_software_version_INS");
}

void CmsEntity::sendLRAS_CS_thresholds_INS(const std::string& topic,
                                           const nlohmann::json& message,
                                           uint32_t warningDistance1,
                                           uint32_t dissuasionDistance1,
                                           uint32_t persuasionDistance1,
                                           uint32_t nohdDistance1,
                                           uint32_t acousticDamageDistance1,
                                           uint32_t maxDazzlerDistance1,
                                           uint32_t maxLightDistance1,
                                           uint32_t warningDistance2,
                                           uint32_t dissuasionDistance2,
                                           uint32_t persuasionDistance2,
                                           uint32_t nohdDistance2,
                                           uint32_t acousticDamageDistance2,
                                           uint32_t maxDazzlerDistance2,
                                           uint32_t maxLightDistance2) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_thresholds_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_thresholds_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_thresholds_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // Action Id
    const uint32_t actionId = static_cast<uint32_t>(message.value("action_id", 0));
    append_u32_be(packet.data, actionId);

    // LRAD 1 thresholds (28 bytes)
    append_u32_be(packet.data, warningDistance1);
    append_u32_be(packet.data, dissuasionDistance1);
    append_u32_be(packet.data, persuasionDistance1);
    append_u32_be(packet.data, nohdDistance1);
    append_u32_be(packet.data, acousticDamageDistance1);
    append_u32_be(packet.data, maxDazzlerDistance1);
    append_u32_be(packet.data, maxLightDistance1);

    // LRAD 2 thresholds (28 bytes)
    append_u32_be(packet.data, warningDistance2);
    append_u32_be(packet.data, dissuasionDistance2);
    append_u32_be(packet.data, persuasionDistance2);
    append_u32_be(packet.data, nohdDistance2);
    append_u32_be(packet.data, acousticDamageDistance2);
    append_u32_be(packet.data, maxDazzlerDistance2);
    append_u32_be(packet.data, maxLightDistance2);

    sendMulticastPacket(packet, "LRAS_CS_thresholds_INS");
}

void CmsEntity::sendLRAS_CS_translation_INS(const std::string& topic,
                                            const nlohmann::json& message,
                                            uint16_t lradId,
                                            uint16_t status,
                                            uint16_t languageIn,
                                            uint16_t languageOut,
                                            const std::string& messageText) const {
    (void)topic;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_translation_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_CS_translation_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_translation_INS + HeaderSize);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // Action Id
    const uint32_t actionId = static_cast<uint32_t>(message.value("action_id", 0));
    append_u32_be(packet.data, actionId);

    // LRAD ID: 1 = LRAD 1 Port, 2 = LRAD 2 Starboard
    append_u16_be(packet.data, lradId);

    // Status: 1=Wait, 2=Ok, 3=Refused, 4=Timeout, 5=Unsolicited
    append_u16_be(packet.data, status);

    // Free text block (valid when status is Ok or Unsolicited)
    // Language in: 0=Italian, 1=English, 2=Arabic-Egypt, 99=Tone
    append_u16_be(packet.data, languageIn);

    // Language out: 0=Italian, 1=English, 2=Arabic-Egypt, 99=Tone
    append_u16_be(packet.data, languageOut);
    std::vector<uint8_t> text(768, 0);
    const std::size_t textLen = std::min<std::size_t>(messageText.size(), text.size());
    std::memcpy(text.data(), messageText.data(), textLen);
    packet.data.insert(packet.data.end(), text.begin(), text.end());

    sendMulticastPacket(packet, "LRAS_CS_translation_INS");
}

void CmsEntity::sendLRAS_CS_lrad_1_status_INS(const std::string& topic,
                                              const nlohmann::json& message,
                                              uint16_t lradStatus,
                                              uint16_t lradMode,
                                              uint16_t cueingStatus,
                                              uint16_t videoTrackingStatus,
                                              float azimuth,
                                              float elevation,
                                              int16_t lrfDistance,
                                              uint16_t inhibitionSectorFlag,
                                              uint16_t warningStep,
                                              uint16_t dissuasionStep,
                                              uint16_t laserDazzlerMode,
                                              uint16_t persuasionStep,
                                              uint16_t laserPulseLength,
                                              uint16_t lightPower) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_lrad_status_INS);

    append_u32_be(packet.data, MessageId_LRAS_CS_lrad_1_status_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_lrad_status_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    append_u16_be(packet.data, lradStatus);
    append_u16_be(packet.data, lradMode);
    append_u16_be(packet.data, cueingStatus);
    append_u16_be(packet.data, videoTrackingStatus);
    append_f32_be(packet.data, azimuth);
    append_f32_be(packet.data, elevation);
    append_i16_be(packet.data, lrfDistance);
    append_u16_be(packet.data, inhibitionSectorFlag);
    append_u16_be(packet.data, warningStep);
    append_u16_be(packet.data, dissuasionStep);
    append_u16_be(packet.data, laserDazzlerMode);
    append_u16_be(packet.data, persuasionStep);
    append_u16_be(packet.data, laserPulseLength);
    append_u16_be(packet.data, lightPower);

    sendMulticastPacket(packet, "LRAS_CS_lrad_1_status_INS");
}  

void CmsEntity::sendLRAS_CS_lrad_2_status_INS(const std::string& topic,
                                              const nlohmann::json& message,
                                              uint16_t lradStatus,
                                              uint16_t lradMode,
                                              uint16_t cueingStatus,
                                              uint16_t videoTrackingStatus,
                                              float azimuth,
                                              float elevation,
                                              int16_t lrfDistance,
                                              uint16_t inhibitionSectorFlag,
                                              uint16_t warningStep,
                                              uint16_t dissuasionStep,
                                              uint16_t laserDazzlerMode,
                                              uint16_t persuasionStep,
                                              uint16_t laserPulseLength,
                                              uint16_t lightPower) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_CS_lrad_status_INS);

    append_u32_be(packet.data, MessageId_LRAS_CS_lrad_2_status_INS);
    append_u32_be(packet.data, MessageLength_LRAS_CS_lrad_status_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    append_u16_be(packet.data, lradStatus);
    append_u16_be(packet.data, lradMode);
    append_u16_be(packet.data, cueingStatus);
    append_u16_be(packet.data, videoTrackingStatus);
    append_f32_be(packet.data, azimuth);
    append_f32_be(packet.data, elevation);
    append_i16_be(packet.data, lrfDistance);
    append_u16_be(packet.data, inhibitionSectorFlag);
    append_u16_be(packet.data, warningStep);
    append_u16_be(packet.data, dissuasionStep);
    append_u16_be(packet.data, laserDazzlerMode);
    append_u16_be(packet.data, persuasionStep);
    append_u16_be(packet.data, laserPulseLength);
    append_u16_be(packet.data, lightPower);

    sendMulticastPacket(packet, "LRAS_CS_lrad_2_status_INS");
}

void CmsEntity::sendLRAS_MULTI_full_status_v2_INS(const std::string& topic,
                                                  const nlohmann::json& message,
                                                  const std::vector<uint8_t>& lrad1FullStatusBlock,
                                                  const std::vector<uint8_t>& lrad2FullStatusBlock) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_MULTI_full_status_v2_INS);

    // Header
    append_u32_be(packet.data, MessageId_LRAS_MULTI_full_status_v2_INS);
    append_u32_be(packet.data, MessageLength_LRAS_MULTI_full_status_v2_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // LRAD 1 full status (44 bytes)
    const std::size_t lrad1FullCopySize = (lrad1FullStatusBlock.size() < 44u) ? lrad1FullStatusBlock.size() : 44u;
    packet.data.insert(packet.data.end(), lrad1FullStatusBlock.begin(), lrad1FullStatusBlock.begin() + lrad1FullCopySize);
    if (lrad1FullCopySize < 44u) {
        packet.data.insert(packet.data.end(), 44u - lrad1FullCopySize, 0u);
    }

    // LRAD 2 full status (44 bytes)
    const std::size_t lrad2FullCopySize = (lrad2FullStatusBlock.size() < 44u) ? lrad2FullStatusBlock.size() : 44u;
    packet.data.insert(packet.data.end(), lrad2FullStatusBlock.begin(), lrad2FullStatusBlock.begin() + lrad2FullCopySize);
    if (lrad2FullCopySize < 44u) {
        packet.data.insert(packet.data.end(), 44u - lrad2FullCopySize, 0u);
    }

    sendMulticastPacket(packet, "LRAS_MULTI_full_status_v2_INS");
}

void CmsEntity::sendLRAS_MULTI_health_status_INS(const std::string& topic,
                                                 const nlohmann::json& message,
                                                 uint16_t systemCondition,
                                                 uint16_t systemOperativeState,
                                                 uint16_t systemTemperature,
                                                 const std::vector<uint8_t>& lrad1HealthBlock,
                                                 const std::vector<uint8_t>& lrad2HealthBlock,
                                                 uint16_t serverStatus,
                                                 uint16_t console1Status,
                                                 uint16_t console2Status,
                                                 uint16_t console3Status,
                                                 uint16_t console4Status) const {
    (void)topic;
    (void)message;

    RawPacket packet;
    packet.data.reserve(HeaderSize + MessageLength_LRAS_MULTI_health_status_INS);

    // Header (16 bytes)
    append_u32_be(packet.data, MessageId_LRAS_MULTI_health_status_INS);
    append_u32_be(packet.data, MessageLength_LRAS_MULTI_health_status_INS);
    append_u32_be(packet.data, 0);
    append_u32_be(packet.data, 0);

    // System-level fields (6 bytes)
    append_u16_be(packet.data, systemCondition);
    append_u16_be(packet.data, systemOperativeState);
    append_u16_be(packet.data, systemTemperature);

    // LRAD 1 health (856 bytes)
    const std::size_t lrad1HealthCopySize = (lrad1HealthBlock.size() < 856u) ? lrad1HealthBlock.size() : 856u;
    packet.data.insert(packet.data.end(), lrad1HealthBlock.begin(), lrad1HealthBlock.begin() + lrad1HealthCopySize);
    if (lrad1HealthCopySize < 856u) {
        packet.data.insert(packet.data.end(), 856u - lrad1HealthCopySize, 0u);
    }

    // LRAD 2 health (856 bytes)
    const std::size_t lrad2HealthCopySize = (lrad2HealthBlock.size() < 856u) ? lrad2HealthBlock.size() : 856u;
    packet.data.insert(packet.data.end(), lrad2HealthBlock.begin(), lrad2HealthBlock.begin() + lrad2HealthCopySize);
    if (lrad2HealthCopySize < 856u) {
        packet.data.insert(packet.data.end(), 856u - lrad2HealthCopySize, 0u);
    }

    // LRAS Server Status + Console statuses (10 bytes)
    append_u16_be(packet.data, serverStatus);
    append_u16_be(packet.data, console1Status);
    append_u16_be(packet.data, console2Status);
    append_u16_be(packet.data, console3Status);
    append_u16_be(packet.data, console4Status);

    sendMulticastPacket(packet, "LRAS_MULTI_health_status_INS");
}

void CmsEntity::sendMulticastPacket(const RawPacket& packet, const char* messageName) const {
    if (!udpSocket_) {
        std::cerr << "[CMS Entity] Socket UDP non inizializzato per invio "
                  << messageName << std::endl;
        return;
    }

    const SendResult result = udpSocket_->send(packet, LrasStatusMulticastGroup, LrasStatusMulticastPort);
    if (!result.success) {
        std::cerr << "[CMS Entity] Errore invio " << messageName << " verso "
                  << LrasStatusMulticastGroup << ":" << LrasStatusMulticastPort
                  << " -> " << result.error_message << std::endl;
    }
}






