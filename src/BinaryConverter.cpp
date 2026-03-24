#include "BinaryConverter.hpp"
#include <cstring>

namespace {
    constexpr size_t HeaderSize = 16;

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
}

BinaryConverter::BinaryConverter() {
    initializeDispatcher();
}

void BinaryConverter::initializeDispatcher() {
    // Lista di inizializzazione usando la struct MessageMapping
    std::vector<MessageMapping> mappings = {
        { MessageId_CS_LRAS_change_configuration_order_INS, &BinaryConverter::handle_CS_LRAS_change_configuration_order_INS },
        
    };

    for (const auto& m : mappings) {
        dispatchTable[m.messageId] = m.handler;
    }
}

std::vector<RawPacket> BinaryConverter::convert(const RawPacket& input) {
    ParsedHeader header;
    if (!parseHeader(input, header)) return {};

    auto it = dispatchTable.find(header.messageId);
    if (it == dispatchTable.end()) return {};

    // Esegue l'handler che restituisce i messaggi convertiti
    std::vector<ConvertedMessage> convertedMessages = it->second(this, input);
    
    // Converte ogni oggetto JSON in un RawPacket separato
    std::vector<RawPacket> outputPackets;
    for (const auto& message : convertedMessages) {
        std::string s = message.payload.dump();
        RawPacket rp;
        rp.data.assign(s.begin(), s.end());
        rp.destinationLradId = message.destinationLradId;
        outputPackets.push_back(rp);
    }

    return outputPackets;
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

std::string BinaryConverter::mapMasterMode(uint8_t modeCode) {
    switch(modeCode) {
        case 0: return "REQ";
        case 1: return "RELEASE";
        case 2: return "ACCEPT";
        case 3: return "REFUSE";
        default: return "UNKNOWN";
    }
}