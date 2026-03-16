#include "BinaryConverter.hpp"
#include <cstring>
#include <algorithm>

namespace {

constexpr size_t HeaderSize = 4 * sizeof(uint32_t);

// Identificatore del messaggio CS_LRAS_change_configuration_order_INS.
// Cambiare questo valore secondo lo standard del protocollo.
constexpr uint32_t MessageId_CS_LRAS_change_configuration_order_INS = 1;

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

    // Dispatch based on message ID.
    if (parsed.messageId == MessageId_CS_LRAS_change_configuration_order_INS) {
        if (!convert_CS_LRAS_change_configuration_order_INS(output)) {
            return {};
        }
    }

    return output;
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
