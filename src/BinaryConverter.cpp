#include "BinaryConverter.hpp"
#include <cstring>
#include <algorithm>

namespace {

constexpr size_t HeaderSize = 4 * sizeof(uint32_t);

// Identificatori dei messaggi (estratti dalla tabella fornita)
// Valori N.A. non sono dichiarati perché non hanno un ID noto.
constexpr uint32_t MessageId_CS_LRAS_change_configuration_order_INS = 1679949825;
constexpr uint32_t MessageId_CS_LRAS_cueing_order_cancellation_INS = 1679949827;
constexpr uint32_t MessageId_CS_LRAS_cueing_order_INS = 1679949828;
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
    uint16_t messageSender;
    uint16_t messageLength;
    uint32_t timestampSec;
    uint32_t timestampUsec;
};

bool parseHeader(const RawPacket& input, ParsedHeader& out) {
    if (input.data.size() < HeaderSize) {
        return false;
    }

    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;

    std::memcpy(&word0, input.data.data() + 0 * sizeof(uint32_t), sizeof(uint32_t));
    std::memcpy(&word1, input.data.data() + 1 * sizeof(uint32_t), sizeof(uint32_t));
    std::memcpy(&word2, input.data.data() + 2 * sizeof(uint32_t), sizeof(uint32_t));
    std::memcpy(&word3, input.data.data() + 3 * sizeof(uint32_t), sizeof(uint32_t));

    out.messageId = ntohl(word0);
    uint32_t senderAndLength = ntohl(word1);
    out.timestampSec = ntohl(word2);
    out.timestampUsec = ntohl(word3);

    out.messageSender = static_cast<uint16_t>(senderAndLength >> 16);
    out.messageLength = static_cast<uint16_t>(senderAndLength & 0xFFFF);

    // Assumiamo che "Message Length" rappresenti la lunghezza del payload, senza l'header
    if (input.data.size() < HeaderSize + out.messageLength) {
        return false;
    }

    return true;
}

void writeHeaderHostOrder(RawPacket& output, const ParsedHeader& parsed) {
    // Scrive i primi 16 byte del pacchetto in ordine host.
    uint32_t hostWord0 = parsed.messageId;
    uint32_t hostWord1 = (static_cast<uint32_t>(parsed.messageSender) << 16) | parsed.messageLength;
    uint32_t hostWord2 = parsed.timestampSec;
    uint32_t hostWord3 = parsed.timestampUsec;

    std::memcpy(output.data.data() + 0 * sizeof(uint32_t), &hostWord0, sizeof(uint32_t));
    std::memcpy(output.data.data() + 1 * sizeof(uint32_t), &hostWord1, sizeof(uint32_t));
    std::memcpy(output.data.data() + 2 * sizeof(uint32_t), &hostWord2, sizeof(uint32_t));
    std::memcpy(output.data.data() + 3 * sizeof(uint32_t), &hostWord3, sizeof(uint32_t));
}

} // namespace

RawPacket BinaryConverter::convert(const RawPacket& input) {
    ParsedHeader parsed;
    if (!parseHeader(input, parsed)) {
        return {};
    }

    RawPacket output = input;
    writeHeaderHostOrder(output, parsed);

    bool conversionSuccess = false;

    // Dispatch based on message ID.
    switch (parsed.messageId) {
        case MessageId_CS_LRAS_change_configuration_order_INS:
            conversionSuccess = convert_CS_LRAS_change_configuration_order_INS(output);
            break;
        default:
            // Messaggio non supportato
            return {};
    }

    if (!conversionSuccess) {
        return {};
    }
        
    // Converti il payload a JSON e metti il JSON nel RawPacket
    json payloadJson = extractPayloadToJson(output, parsed.messageId);
    if (payloadJson.empty()) {
        return {};
    }
    
    std::string jsonStr = payloadJson.dump();
    
    // Crea un nuovo RawPacket con il JSON come contenuto
    RawPacket jsonOutput;
    jsonOutput.data.assign(jsonStr.begin(), jsonStr.end());
    return jsonOutput;
}

bool BinaryConverter::convert_CS_LRAS_change_configuration_order_INS(RawPacket& packet) const {
    // Struttura del messaggio (tutto in big-endian):
    //  - Header (16 bytes)
    //  - Action Id (4 bytes)
    //  - LRAD ID (2 bytes)
    //  - Configuration (2 bytes)
    constexpr size_t ExpectedPayloadSize = 8;

    if (packet.data.size() < HeaderSize + ExpectedPayloadSize) {
        return false;
    }

    // Action Id
    uint32_t actionId;
    std::memcpy(&actionId, packet.data.data() + HeaderSize + 0, sizeof(uint32_t));
    actionId = ntohl(actionId);
    std::memcpy(packet.data.data() + HeaderSize + 0, &actionId, sizeof(uint32_t));

    // LRAD ID
    uint16_t lradId;
    std::memcpy(&lradId, packet.data.data() + HeaderSize + 4, sizeof(uint16_t));
    lradId = ntohs(lradId);
    std::memcpy(packet.data.data() + HeaderSize + 4, &lradId, sizeof(uint16_t));

    // Configuration
    uint16_t configuration;
    std::memcpy(&configuration, packet.data.data() + HeaderSize + 6, sizeof(uint16_t));
    configuration = ntohs(configuration);
    std::memcpy(packet.data.data() + HeaderSize + 6, &configuration, sizeof(uint16_t));

    return true;
}

bool BinaryConverter::convert_CS_LRAS_status_update(RawPacket& packet) const {
    // Struttura del messaggio CS_LRAS_status_update (esempio):
    //  - Header (16 bytes)
    //  - Status Code (4 bytes)
    //  - LRAD ID (2 bytes)
    //  - Error Count (2 bytes)
    constexpr size_t ExpectedPayloadSize = 8;

    if (packet.data.size() < HeaderSize + ExpectedPayloadSize) {
        return false;
    }

    // Status Code
    uint32_t statusCode;
    std::memcpy(&statusCode, packet.data.data() + HeaderSize + 0, sizeof(uint32_t));
    statusCode = ntohl(statusCode);
    std::memcpy(packet.data.data() + HeaderSize + 0, &statusCode, sizeof(uint32_t));

    // LRAD ID
    uint16_t lradId;
    std::memcpy(&lradId, packet.data.data() + HeaderSize + 4, sizeof(uint16_t));
    lradId = ntohs(lradId);
    std::memcpy(packet.data.data() + HeaderSize + 4, &lradId, sizeof(uint16_t));

    // Error Count
    uint16_t errorCount;
    std::memcpy(&errorCount, packet.data.data() + HeaderSize + 6, sizeof(uint16_t));
    errorCount = ntohs(errorCount);
    std::memcpy(packet.data.data() + HeaderSize + 6, &errorCount, sizeof(uint16_t));

    return true;
}

json BinaryConverter::extractPayloadToJson(const RawPacket& packet, uint32_t messageId) const {
    // Dispatch basato su messageId per estrarre il JSON appropriato
    switch (messageId) {
        case MessageId_CS_LRAS_change_configuration_order_INS: {
            // Struttura: actionId (4), lradId (2), configuration (2)
            constexpr size_t ExpectedPayloadSize = 8;
            if (packet.data.size() < HeaderSize + ExpectedPayloadSize) {
                return json();
            }

            uint32_t actionId;
            uint16_t lradId;
            uint16_t configuration;

            std::memcpy(&actionId, packet.data.data() + HeaderSize + 0, sizeof(uint32_t));
            std::memcpy(&lradId, packet.data.data() + HeaderSize + 4, sizeof(uint16_t));
            std::memcpy(&configuration, packet.data.data() + HeaderSize + 6, sizeof(uint16_t));

            json payload;
            payload["type"] = "CS_LRAS_change_configuration_order_INS";
            payload["actionId"] = actionId;
            payload["lradId"] = lradId;
            payload["configuration"] = configuration;
            return payload;
        }

        default:
            // Messaggio non supportato
            return json();
    }
}
