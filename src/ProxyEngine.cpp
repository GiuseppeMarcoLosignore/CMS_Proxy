#include "ProxyEngine.hpp"
#include <boost/asio/error.hpp>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <nlohmann/json.hpp>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

namespace {

uint32_t read_u32_be(const std::vector<uint8_t>& data, std::size_t offset) {
    if (data.size() < offset + sizeof(uint32_t)) {
        return 0;
    }
    uint32_t net_value = 0;
    std::memcpy(&net_value, data.data() + offset, sizeof(uint32_t));
    return ntohl(net_value);
}

uint32_t extract_message_id_from_header(const RawPacket& packet) {
    return read_u32_be(packet.data, 0);
}

uint32_t extract_action_id_from_payload(const RawPacket& packet) {
    try {
        const auto j = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (!j.contains("param")) return 0;
        const auto& param = j.at("param");
        if (!param.contains("action_id")) return 0;
        return param.at("action_id").get<uint32_t>();
    } catch (...) {
        return 0;
    }
}

void log_ack_packet(const RawPacket& packet, uint32_t action_id, uint32_t source_message_id) {
    std::cout << "[Engine][ACK] source_message_id=" << source_message_id
              << " action_id=" << action_id
              << " raw_hex=";
    for (uint8_t b : packet.data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;
}
}

ProxyEngine::ProxyEngine(std::shared_ptr<IReceiver> r, 
                         std::shared_ptr<IProtocolConverter> c, 
                         std::shared_ptr<ISender> s,
                         std::shared_ptr<IAckSender> ack_sender,
                         std::shared_ptr<SystemState> system_state,
                         boost::asio::io_context& delivery_io_ctx,
                         std::map<uint16_t, LradDestination> lrad_config)
    : receiver_(r),
      converter_(c),
      sender_(s),
      ack_sender_(ack_sender),
    system_state_(system_state),
      delivery_io_ctx_(delivery_io_ctx)
{
    lrad_config_ = std::move(lrad_config);

    // Colleghiamo la logica: quando arriva un pacchetto...
    receiver_->set_callback([this](const RawPacket& input, const PacketSourceInfo&) {
        boost::asio::post(delivery_io_ctx_, [this, input]() {
            processPacket(input);
        });
    });

}

void ProxyEngine::processPacket(const RawPacket& input) {
    const uint32_t source_message_id = extract_message_id_from_header(input);

    const SystemStateSnapshot snapshot = system_state_ ? system_state_->getSnapshot() : SystemStateSnapshot{};

    ConversionResult result = converter_->convert(input, snapshot);

    if (result.packets.empty()) {
        std::cerr << "[Engine] Messaggio ignorato (messageId non supportato o payload malformato): source_id="
                  << source_message_id << std::endl;
        return;
    }

    if (result.ack_only) {
        SendResult send_result;
        send_result.success = true;
        const RawPacket ack_packet = result.ack_builder(0, source_message_id, send_result);
        log_ack_packet(ack_packet, 0, source_message_id);
        if (ack_sender_) ack_sender_->send_ack(ack_packet);
        return;
    }

    bool allSendsSucceeded = true;
    for (const auto& packetToSend : result.packets) {
        SendResult send_result;
        auto destinationIt = lrad_config_.find(packetToSend.destinationLradId);
        if (destinationIt != lrad_config_.end()) {
            send_result = sender_->send(
                packetToSend,
                destinationIt->second.ip_address,
                destinationIt->second.port
            );
        } else {
            std::cerr << "[Engine] LRAD ID non configurato: " << packetToSend.destinationLradId << std::endl;
            send_result.success       = false;
            send_result.error_value   = static_cast<int>(boost::asio::error::invalid_argument);
            send_result.error_category = "engine";
            send_result.error_message  = "LRAD ID non configurato";
        }

        allSendsSucceeded = allSendsSucceeded && send_result.success;

        const uint32_t action_id  = extract_action_id_from_payload(packetToSend);
        const RawPacket ack_packet = result.ack_builder(action_id, source_message_id, send_result);
        log_ack_packet(ack_packet, action_id, source_message_id);
        if (ack_sender_) ack_sender_->send_ack(ack_packet);
    }

    if (allSendsSucceeded && system_state_ && !result.state_updates.empty()) {
        system_state_->applyBatch(result.state_updates);
    }
}

void ProxyEngine::run() {
    if (receiver_) {
        std::cout << "[Engine] Avvio del ciclo di ricezione..." << std::endl;
        receiver_->start();
    }
}
