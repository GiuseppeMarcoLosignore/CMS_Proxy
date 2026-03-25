#pragma once
#include "IInterfaces.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

using json = nlohmann::json;

class BinaryConverter : public IProtocolConverter {
public:
    BinaryConverter();

    ConversionResult convert(const RawPacket& input);

private:
    struct ConvertedMessage {
        json payload;
        uint16_t destinationLradId;
    };

    using HandlerFunc = std::function<std::vector<ConvertedMessage>(BinaryConverter*, const RawPacket&)>;

    struct MessageMapping {
        uint32_t messageId;
        HandlerFunc handler;
        AckBuilderFunc ack_builder;
        bool ack_only;
    };

    std::unordered_map<uint32_t, MessageMapping> dispatchTable;

    void initializeDispatcher();

    std::vector<ConvertedMessage> handle_CS_LRAS_change_configuration_order_INS(const RawPacket& packet);
    std::vector<ConvertedMessage> handle_CS_LRAS_cueing_order_cancellation_INS(const RawPacket& packet);
    std::vector<ConvertedMessage> handle_CS_LRAS_cueing_order_INS(const RawPacket& packet);

    std::string mapMasterMode(uint8_t modeCode);
};
