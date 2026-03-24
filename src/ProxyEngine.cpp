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
constexpr std::size_t HeaderSize = 16;
constexpr std::size_t AckMessageSize = 28;
constexpr uint32_t MessageId_LRAS_CS_ack_INS = 576879045;
constexpr uint16_t AckAccepted = 1;
constexpr uint16_t NackNotExecuted = 2;
constexpr const char* AckMulticastIp = "239.0.0.50";
constexpr uint16_t AckMulticastPort = 12346;

uint32_t read_u32_be(const std::vector<uint8_t>& data, std::size_t offset) {
    if (data.size() < offset + sizeof(uint32_t)) {
        return 0;
    }

    uint32_t net_value = 0;
    std::memcpy(&net_value, data.data() + offset, sizeof(uint32_t));
    return ntohl(net_value);
}

uint32_t extract_action_id_from_payload(const RawPacket& packet) {
    try {
        const auto j = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (!j.contains("param")) {
            return 0;
        }
        const auto& param = j.at("param");
        if (!param.contains("action_id")) {
            return 0;
        }
        return param.at("action_id").get<uint32_t>();
    } catch (...) {
        return 0;
    }
}

uint16_t map_nack_reason(const SendResult& send_result) {
    using namespace boost::asio;

    if (send_result.success) {
        return 0;
    }

    const auto ec = boost::system::error_code(send_result.error_value, boost::system::system_category());
    if (ec == error::invalid_argument || ec == error::bad_descriptor ||
        send_result.error_category == "resolver" || send_result.error_category == "engine") {
        return 2; // Parametri errati
    }
    if (ec == error::already_started || ec == error::in_progress || ec == error::operation_aborted) {
        return 3; // Stato del sistema errato
    }
    if (ec == error::timed_out || ec == error::try_again || ec == error::would_block || ec == error::not_connected) {
        return 4; // Sistema non pronto
    }
    if (ec == error::connection_refused || ec == error::connection_reset || ec == error::host_unreachable ||
        ec == error::network_unreachable || ec == error::network_down || ec == error::broken_pipe ||
        ec == error::eof) {
        return 5; // Sistema non utilizzabile
    }

    return 0; // No statement
}

RawPacket build_ack_packet(uint32_t action_id, uint32_t source_message_id, const SendResult& send_result) {
    std::vector<uint8_t> bytes(AckMessageSize, 0);

    const uint16_t ack_nack = send_result.success ? AckAccepted : NackNotExecuted;
    const uint16_t nack_reason = send_result.success ? 0 : map_nack_reason(send_result);

    const uint32_t header_word0 = htonl(MessageId_LRAS_CS_ack_INS);
    const uint32_t header_word1 = htonl(static_cast<uint32_t>(AckMessageSize - HeaderSize));
    const uint32_t header_word2 = 0;
    const uint32_t header_word3 = 0;

    std::memcpy(bytes.data() + 0, &header_word0, sizeof(uint32_t));
    std::memcpy(bytes.data() + 4, &header_word1, sizeof(uint32_t));
    std::memcpy(bytes.data() + 8, &header_word2, sizeof(uint32_t));
    std::memcpy(bytes.data() + 12, &header_word3, sizeof(uint32_t));

    const uint32_t action_id_net = htonl(action_id);
    const uint32_t source_message_id_net = htonl(source_message_id);
    const uint16_t ack_nack_net = htons(ack_nack);
    const uint16_t nack_reason_net = htons(nack_reason);

    std::memcpy(bytes.data() + 16, &action_id_net, sizeof(uint32_t));
    std::memcpy(bytes.data() + 20, &source_message_id_net, sizeof(uint32_t));
    std::memcpy(bytes.data() + 24, &ack_nack_net, sizeof(uint16_t));
    std::memcpy(bytes.data() + 26, &nack_reason_net, sizeof(uint16_t));

    RawPacket ack_packet;
    ack_packet.data = std::move(bytes);
    return ack_packet;
}

void log_ack_packet(const RawPacket& packet, uint32_t action_id, uint32_t source_message_id, const SendResult& send_result) {
    const uint16_t ack_nack = send_result.success ? AckAccepted : NackNotExecuted;
    const uint16_t nack_reason = send_result.success ? 0 : map_nack_reason(send_result);

    std::cout << "[Engine][ACK] msg_id=" << MessageId_LRAS_CS_ack_INS
              << " action_id=" << action_id
              << " source_message_id=" << source_message_id
              << " ack_nack=" << ack_nack
              << " nack_reason=" << nack_reason
              << " ec=" << send_result.error_value
              << " (" << send_result.error_category << ")"
              << std::endl;

    std::cout << "[Engine][ACK] raw_hex=";
    for (uint8_t b : packet.data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    std::cout << std::dec << std::setfill(' ') << std::endl;
}
}

ProxyEngine::ProxyEngine(std::shared_ptr<IReceiver> r, 
                         std::shared_ptr<IProtocolConverter> c, 
                 std::shared_ptr<ISender> s,
                 boost::asio::io_context& delivery_io_ctx)
    : receiver_(r),
      converter_(c),
      sender_(s),
    delivery_io_ctx_(delivery_io_ctx),
    ack_socket_(delivery_io_ctx_),
      ack_multicast_ready_(false)
{
    lrad_config_ = getNetworkConfig();
    boost::system::error_code ec;
    auto ack_address = boost::asio::ip::make_address(AckMulticastIp, ec);
    if (ec) {
        std::cerr << "[Engine][ACK] Indirizzo multicast non valido: " << AckMulticastIp
                  << " -> " << ec.message() << std::endl;
    } else {
        ack_multicast_endpoint_ = boost::asio::ip::udp::endpoint(ack_address, AckMulticastPort);

        ack_socket_.open(boost::asio::ip::udp::v4(), ec);
        if (ec) {
            std::cerr << "[Engine][ACK] Errore apertura socket UDP ACK: " << ec.message() << std::endl;
        } else {
            ack_socket_.set_option(boost::asio::ip::multicast::enable_loopback(true), ec);
            if (ec) {
                std::cerr << "[Engine][ACK] Errore configurazione loopback multicast: " << ec.message() << std::endl;
            }

            ack_socket_.set_option(boost::asio::ip::multicast::hops(1), ec);
            if (ec) {
                std::cerr << "[Engine][ACK] Errore configurazione TTL multicast: " << ec.message() << std::endl;
            }

            ack_multicast_ready_ = true;
            std::cout << "[Engine][ACK] Invio ACK multicast attivo su "
                      << AckMulticastIp << ":" << AckMulticastPort << std::endl;
        }
    }

    // Colleghiamo la logica: quando arriva un pacchetto...
    receiver_->set_callback([this](const RawPacket& input) {
        boost::asio::post(delivery_io_ctx_, [this, input]() {
            processPacket(input);
        });
    });

}

void ProxyEngine::processPacket(const RawPacket& input) {
    const uint32_t source_message_id = read_u32_be(input.data, 0);

    // 1. Convertiamo il pacchetto (Big-Endian -> Logica interna)
    std::vector<RawPacket> output = converter_->convert(input);
    // 2. Se la conversione ha prodotto dati validi, inviamo tramite TCP
    if (!output.empty()) {
        for (const auto& packetToSend : output) {
            auto destinationIt = lrad_config_.find(packetToSend.destinationLradId);
            if (destinationIt != lrad_config_.end()) {
                const SendResult send_result = sender_->send(
                    packetToSend,
                    destinationIt->second.ip_address,
                    destinationIt->second.port
                );

                const uint32_t action_id = extract_action_id_from_payload(packetToSend);
                const RawPacket ack_packet = build_ack_packet(action_id, source_message_id, send_result);
                log_ack_packet(ack_packet, action_id, source_message_id, send_result);
                sendAckToMulticast(ack_packet);
            } else {
                std::cerr << "[Engine] LRAD ID non configurato: " << packetToSend.destinationLradId << std::endl;

                SendResult send_result;
                send_result.success = false;
                send_result.error_value = static_cast<int>(boost::asio::error::invalid_argument);
                send_result.error_category = "engine";
                send_result.error_message = "LRAD ID non configurato";

                const uint32_t action_id = extract_action_id_from_payload(packetToSend);
                const RawPacket ack_packet = build_ack_packet(action_id, source_message_id, send_result);
                log_ack_packet(ack_packet, action_id, source_message_id, send_result);
                sendAckToMulticast(ack_packet);
            }
        }
    } else {
        std::cerr << "[Engine] Messaggio ignorato (messageId non supportato o payload malformato): source_id="
                  << source_message_id << std::endl;
    }
}

void ProxyEngine::sendAckToMulticast(const RawPacket& ack_packet) {
    if (!ack_multicast_ready_) {
        std::cerr << "[Engine][ACK] Socket multicast non pronto, ACK non inviato." << std::endl;
        return;
    }

    boost::system::error_code ec;
    ack_socket_.send_to(boost::asio::buffer(ack_packet.data), ack_multicast_endpoint_, 0, ec);
    if (ec) {
        std::cerr << "[Engine][ACK] Errore invio multicast ACK: " << ec.message() << std::endl;
    }
}

void ProxyEngine::run() {
    if (receiver_) {
        std::cout << "[Engine] Avvio del ciclo di ricezione..." << std::endl;
        receiver_->start();
    }
}

std::map<uint16_t, LradDestination> ProxyEngine::getNetworkConfig() {
    // Configurazione statica per esempio
    std::map<uint16_t, LradDestination> config;
    config[1] = {1, "127.0.0.1", 9000}; // LRAD ID 1 -> Port/CC
    config[2] = {2, "127.0.0.1", 9000}; // LRAD ID 2 -> Port/ACS
    return config;
}