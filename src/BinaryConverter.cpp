#include "BinaryConverter.hpp"
#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>
#include <cmath>
#include <cstring>

namespace {
    constexpr size_t HeaderSize = 16;
    constexpr float Pi = 3.14159265358979323846f;

    constexpr uint32_t MessageId_CS_LRAS_change_configuration_order_INS = 1679949825;
    constexpr uint32_t MessageId_CS_LRAS_cueing_order_cancellation_INS = 1679949826;
    constexpr uint32_t MessageId_CS_LRAS_cueing_order_INS = 1679949827;
    constexpr uint32_t MessageId_CS_LRAS_emission_control_INS = 1679949829;
    constexpr uint32_t MessageId_CS_LRAS_emission_mode_INS = 1679949830;
    constexpr uint32_t MessageId_CS_LRAS_inhibition_sectors_INS = 1679949831;
    constexpr uint32_t MessageId_CS_LRAS_joystick_control_lrad_1_INS = 1679949832;
    constexpr uint32_t MessageId_CS_LRAS_joystick_control_lrad_2_INS = 1679949833;
    constexpr uint32_t MessageId_CS_LRAS_recording_command_INS = 1679949834;
    constexpr uint32_t MessageId_CS_LRAS_request_emission_mode_INS = 1679949835;
    constexpr uint32_t MessageId_CS_LRAS_request_engagement_capability_INS = 1679949836;
    constexpr uint32_t MessageId_CS_LRAS_request_full_status_INS = 1679949837;
    constexpr uint32_t MessageId_CS_LRAS_request_installation_data_INS = 1679949838;
    constexpr uint32_t MessageId_CS_LRAS_request_message_table_INS = 1679949839;
    constexpr uint32_t MessageId_CS_LRAS_request_software_version_INS = 1679949840;
    constexpr uint32_t MessageId_CS_LRAS_request_thresholds_INS = 1679949841;
    constexpr uint32_t MessageId_CS_LRAS_request_translation_INS = 1679949842;
    constexpr uint32_t MessageId_CS_LRAS_request_translation_v2_INS = 1679949843; // se si trattava di un messaggio diverso
    constexpr uint32_t MessageId_CS_LRAS_video_tracking_command_INS = 1679949844;
    constexpr uint32_t MessageId_CS_MULTI_health_status_INS = 1684229565;
    constexpr uint32_t MessageId_CS_MULTI_update_cst_kinematics_INS = 1684229566;
    constexpr uint32_t MessageId_LRAS_CS_ack_INS = 576879045;
    constexpr uint32_t MessageId_LRAS_CS_change_configuration_request_INS = 576879046;
    constexpr uint32_t MessageId_LRAS_CS_emission_mode_feedback_INS = 576879047;
    constexpr uint32_t MessageId_LRAS_CS_engagement_capability_INS = 576879048;
    constexpr uint32_t MessageId_LRAS_CS_hw_limit_warning_INS = 576879049;
    constexpr uint32_t MessageId_LRAS_CS_installation_data_INS = 576879050;
    constexpr uint32_t MessageId_LRAS_CS_lrad_1_status_INS = 576879051;
    constexpr uint32_t MessageId_LRAS_CS_lrad_2_status_INS = 576879052;
    constexpr uint32_t MessageId_LRAS_CS_message_table_INS = 576879053;
    constexpr uint32_t MessageId_LRAS_CS_software_version_INS = 576879054;
    constexpr uint32_t MessageId_LRAS_CS_thresholds_INS = 576879055;
    constexpr uint32_t MessageId_LRAS_CS_translation_INS = 576879056;
    constexpr uint32_t MessageId_LRAS_CS_video_ir_lrad_1_INS = 576879057;
    constexpr uint32_t MessageId_LRAS_CS_video_ir_lrad_2_INS = 576879058;
    constexpr uint32_t MessageId_LRAS_MULTI_full_status_v2_INS = 576913411;
    constexpr uint32_t MessageId_LRAS_MULTI_health_status_INS = 576913410;
    constexpr uint32_t MessageId_NAVS_MULTI_gyro_fore_nav_data_10ms_INS = 425290942;
    constexpr uint32_t MessageId_NAVS_MULTI_health_status_INS = 425290943;
    constexpr uint32_t MessageId_NAVS_MULTI_nav_data_100ms_INS = 425290944;
    constexpr uint32_t MessageId_NAVS_MULTI_ships_admin_force_nav_INS = 425290947;

    struct ParsedHeader {
        uint32_t messageId;
        uint16_t messageLength;
        // ... altri campi se necessari
    };

    bool parseHeader(const RawPacket& input, ParsedHeader& out) {
        if (input.data.size() < HeaderSize) return false;
        uint32_t id;
        std::memcpy(&id, input.data.data(), 4);
        out.messageId = ntohl(id);
        
        uint32_t word1;
        std::memcpy(&word1, input.data.data() + 4, 4);
        out.messageLength = ntohl(word1) & 0xFFFF;
        
        return input.data.size() >= (HeaderSize + out.messageLength);
    }

    uint16_t read_u16_be(const std::vector<uint8_t>& data, size_t offset) {
        uint16_t value = 0;
        std::memcpy(&value, data.data() + offset, sizeof(uint16_t));
        return ntohs(value);
    }

    uint32_t read_u32_be(const std::vector<uint8_t>& data, size_t offset) {
        uint32_t value = 0;
        std::memcpy(&value, data.data() + offset, sizeof(uint32_t));
        return ntohl(value);
    }

    float read_f32_be(const std::vector<uint8_t>& data, size_t offset) {
        uint32_t raw = read_u32_be(data, offset);
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(float));
        return value;
    }

    float rad_to_deg(float radians) {
        return radians * (180.0f / Pi);
    }

    // --- ACK build helpers ---
    constexpr std::size_t AckHeaderSize    = 16;
    constexpr std::size_t AckMessageSize   = 28;
    constexpr uint32_t    MsgId_LRAS_CS_ack_INS = 576879045;
    constexpr uint16_t    AckAccepted      = 1;
    constexpr uint16_t    NackNotExecuted  = 2;

    uint16_t map_nack_reason(const SendResult& sr) {
        using namespace boost::asio;
        if (sr.success) return 0;
        const auto ec = boost::system::error_code(sr.error_value, boost::system::system_category());
        if (ec == error::invalid_argument || ec == error::bad_descriptor ||
            sr.error_category == "resolver" || sr.error_category == "engine") return 2;
        if (ec == error::already_started || ec == error::in_progress || ec == error::operation_aborted) return 3;
        if (ec == error::timed_out || ec == error::try_again || ec == error::would_block || ec == error::not_connected) return 4;
        if (ec == error::connection_refused || ec == error::connection_reset || ec == error::host_unreachable ||
            ec == error::network_unreachable || ec == error::network_down || ec == error::broken_pipe ||
            ec == error::eof) return 5;
        return 0;
    }

    RawPacket build_ack_packet(uint32_t action_id, uint32_t source_message_id, const SendResult& sr) {
        std::vector<uint8_t> bytes(AckMessageSize, 0);
        const uint16_t ack_nack    = sr.success ? AckAccepted : NackNotExecuted;
        const uint16_t nack_reason = sr.success ? 0 : map_nack_reason(sr);

        const uint32_t w0 = htonl(MsgId_LRAS_CS_ack_INS);
        const uint32_t w1 = htonl(static_cast<uint32_t>(AckMessageSize - AckHeaderSize));
        const uint32_t w2 = 0;
        const uint32_t w3 = 0;
        std::memcpy(bytes.data() +  0, &w0, 4);
        std::memcpy(bytes.data() +  4, &w1, 4);
        std::memcpy(bytes.data() +  8, &w2, 4);
        std::memcpy(bytes.data() + 12, &w3, 4);

        const uint32_t aid_net  = htonl(action_id);
        const uint32_t smid_net = htonl(source_message_id);
        const uint16_t an_net   = htons(ack_nack);
        const uint16_t nr_net   = htons(nack_reason);
        std::memcpy(bytes.data() + 16, &aid_net,  4);
        std::memcpy(bytes.data() + 20, &smid_net, 4);
        std::memcpy(bytes.data() + 24, &an_net,   2);
        std::memcpy(bytes.data() + 26, &nr_net,   2);

        RawPacket pkt;
        pkt.data = std::move(bytes);
        return pkt;
    }
}

BinaryConverter::BinaryConverter() {
    initializeDispatcher();
}

void BinaryConverter::initializeDispatcher() {
    AckBuilderFunc standard_ack = [](uint32_t action_id, uint32_t source_message_id, const SendResult& sr) {
        return build_ack_packet(action_id, source_message_id, sr);
    };

    std::vector<MessageMapping> mappings = {
        { MessageId_CS_LRAS_change_configuration_order_INS,   &BinaryConverter::handle_CS_LRAS_change_configuration_order_INS,   standard_ack, false },
        { MessageId_CS_LRAS_cueing_order_cancellation_INS,    &BinaryConverter::handle_CS_LRAS_cueing_order_cancellation_INS,    standard_ack, true  },
        { MessageId_CS_LRAS_cueing_order_INS,                 &BinaryConverter::handle_CS_LRAS_cueing_order_INS,                 standard_ack, false },
    };

    for (const auto& m : mappings) {
        dispatchTable[m.messageId] = m;
    }
}

ConversionResult BinaryConverter::convert(const RawPacket& input) {
    ParsedHeader header;
    if (!parseHeader(input, header)) return {};

    auto it = dispatchTable.find(header.messageId);
    if (it == dispatchTable.end()) return {};

    const MessageMapping& mapping = it->second;
    std::vector<ConvertedMessage> convertedMessages = mapping.handler(this, input);

    ConversionResult result;
    result.ack_only    = mapping.ack_only;
    result.ack_builder = mapping.ack_builder;

    for (const auto& message : convertedMessages) {
        std::string s = message.payload.dump();
        RawPacket rp;
        rp.data.assign(s.begin(), s.end());
        rp.destinationLradId = message.destinationLradId;
        result.packets.push_back(rp);
    }

    return result;
}

std::vector<BinaryConverter::ConvertedMessage> BinaryConverter::handle_CS_LRAS_change_configuration_order_INS(const RawPacket& packet){
    std::vector<ConvertedMessage> results;
    
    // Iniziamo dal byte 16 (saltiamo l'header gestito altrove)
    size_t offset = 16; 
    const size_t BlockSize = 8; // ActionId(4) + LradId(2) + Configuration(2)

    while (offset + BlockSize <= packet.data.size()) {
        // 1. Estrazione Action ID (Offset 16)
        uint32_t actionId;
        std::memcpy(&actionId, &packet.data[offset], 4);
        actionId = ntohl(actionId);

        // 2. Estrazione LRAD ID (Offset 20)
        uint16_t lradId;
        std::memcpy(&lradId, &packet.data[offset + 4], 2);
        lradId = ntohs(lradId);

        // 3. Estrazione Configuration (Offset 22)
        uint16_t rawConfig;
        std::memcpy(&rawConfig, &packet.data[offset + 6], 2);
        rawConfig = ntohs(rawConfig);

        // --- COSTRUZIONE JSON CON STRUTTURA ORIGINALE ---
        json j;
        j["header"] = "MASTER";
        j["type"]   = "CMD"; // Valore di default per questo tipo di messaggio
        
        
        j["sender"] = "CMS"; // Valore di default per questo tipo di messaggio

        // Logica richiesta: 0 -> RELEASE, 1 -> ACCEPT
        j["param"] = {
            {"mode", (rawConfig == 0) ? "RELEASE" : "REQ"}
        };

        ConvertedMessage message;
        message.payload = j;
        message.destinationLradId = lradId;
        results.push_back(message);

        // Avanzamento al prossimo blocco di 8 byte
        offset += BlockSize;
    }

    return results;
}

std::vector<BinaryConverter::ConvertedMessage> BinaryConverter::handle_CS_LRAS_cueing_order_cancellation_INS(const RawPacket& packet) {
    std::vector<ConvertedMessage> results;

    // Header: 16 bytes | ActionId: 4 bytes | LradId: 2 bytes
    constexpr size_t offset   = 16;
    constexpr size_t BlockSize = 6; // ActionId(4) + LradId(2)

    if (offset + BlockSize > packet.data.size()) return results;

    // 1. Action ID (Pos 16, 4 byte, UInt)
    uint32_t actionId;
    std::memcpy(&actionId, &packet.data[offset], 4);
    actionId = ntohl(actionId);

    // 2. LRAD ID (Pos 20, 2 byte, Enum: 1=LRAD1, 2=LRAD2)
    uint16_t lradId;
    std::memcpy(&lradId, &packet.data[offset + 4], 2);
    lradId = ntohs(lradId);

    // Costruzione JSON TRACKING - cancellazione ordine -> mode = READY, target vuoto
    json j;
    j["header"] = "TRCK";
    j["type"]   = "CMD";
    j["sender"] = "CMS";
    j["param"]  = {
        {"mode",   "READY"},
        {"target", json::array()}
    };

    ConvertedMessage message;
    message.payload          = j;
    message.destinationLradId = lradId;
    results.push_back(message);

    return results;
}

std::vector<BinaryConverter::ConvertedMessage> BinaryConverter::handle_CS_LRAS_cueing_order_INS(const RawPacket& packet) {
    std::vector<ConvertedMessage> results;

    // Campo minimo letto fino a kinematics type (offset 36 + 2 byte).
    constexpr size_t MinPayloadSize = 22;
    if (packet.data.size() < HeaderSize + MinPayloadSize) return results;

    const uint32_t actionId = read_u32_be(packet.data, 16);
    const uint16_t lradId = read_u16_be(packet.data, 20);
    const uint16_t cueingType = read_u16_be(packet.data, 22);
    const uint32_t cstn = read_u32_be(packet.data, 24);
    const uint16_t kinematicsType = read_u16_be(packet.data, 36);

    // Coordinate nelle strutture cartesiane (offset assoluti da specifica allegata).
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool hasCartesianCoordinates = false;

    switch (kinematicsType) {
        case 1: // 3D Cartesian Kinematics
        case 2: // 3D Cartesian Position
            if (packet.data.size() >= 52) {
                x = read_f32_be(packet.data, 40);
                y = read_f32_be(packet.data, 44);
                z = read_f32_be(packet.data, 48);
                hasCartesianCoordinates = true;
            }
            break;
        case 3: // 2D Cartesian Kinematics
        case 4: // 2D Cartesian Position
            if (packet.data.size() >= 48) {
                x = read_f32_be(packet.data, 40);
                y = read_f32_be(packet.data, 44);
                z = 0.0f;
                hasCartesianCoordinates = true;
            }
            break;
        default:
            // Per i tipi non cartesiani gli angoli restano di default finche non arrivano dettagli aggiuntivi.
            break;
    }

    float azimuthDeg = 0.0f;
    float elevationDeg = 0.0f;
    if (hasCartesianCoordinates) {
        azimuthDeg = rad_to_deg(std::atan2(y, x));
        const float horizontalDistance = std::sqrt((x * x) + (y * y));
        elevationDeg = rad_to_deg(std::atan2(z, horizontalDistance));
    }

    json j;
    j["header"] = "TRCK";
    j["type"] = "CMD";
    j["sender"] = "CMS";
    j["param"] = {
        {"action_id", actionId},
        {"cueing_type", cueingType},
        {"cstn", cstn},
        {"kinematics_type", kinematicsType},
        {"azimuth", azimuthDeg},
        {"elevation", elevationDeg}
    };

    ConvertedMessage message;
    message.payload = j;
    message.destinationLradId = lradId;
    results.push_back(message);

    return results;
}

std::string BinaryConverter::mapMasterMode(uint8_t modeCode) {
    switch(modeCode) {
        case 0: return "REQ";
        case 1: return "RELEASE";
        case 2: return "ACCEPT";
        case 3: return "REFUSE";
        default: return "UNKNOWN";
    }
}