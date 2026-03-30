#include "ProxyEngine.hpp"
#include <boost/asio/error.hpp>
#include <boost/asio/ip/address.hpp>
#include <algorithm>
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

void append_u16_be(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void append_u32_be(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void append_u64_be(std::vector<uint8_t>& out, uint64_t value) {
    out.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void append_fixed_string(std::vector<uint8_t>& out, const std::string& value, std::size_t max_len) {
    const std::size_t len = std::min(value.size(), max_len);
    out.insert(out.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(len));
    out.insert(out.end(), max_len - len, static_cast<uint8_t>(0));
}

SendResult make_udp_send_result(const boost::system::error_code& ec) {
    SendResult result;
    result.success = !ec;
    result.error_value = ec.value();
    result.error_category = ec.category().name();
    result.error_message = ec.message();
    return result;
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

void ProxyEngine::configurePeriodicMessages(const std::vector<PeriodicMulticastMessageConfig>& periodic_messages,
                                            const std::vector<SourceProfileConfig>& source_profiles) {
    periodic_tasks_.clear();
    source_profile_bind_ip_.clear();

    for (const auto& profile : source_profiles) {
        source_profile_bind_ip_[profile.name] = profile.bind_ip;
    }

    for (const auto& cfg : periodic_messages) {
        if (!cfg.enabled) {
            continue;
        }
        PeriodicTask task;
        task.config = cfg;
        task.timer = std::make_unique<boost::asio::steady_timer>(delivery_io_ctx_);
        periodic_tasks_.push_back(std::move(task));
    }

    std::cout << "[Engine] Messaggi periodici configurati: " << periodic_tasks_.size() << std::endl;
}

void ProxyEngine::startPeriodicMessages() {
    if (periodic_tasks_.empty()) {
        std::cout << "[Engine] Nessun messaggio periodico abilitato in configurazione." << std::endl;
        return;
    }

    initializePeriodicUdpSockets();
    periodic_enabled_ = true;

    for (std::size_t i = 0; i < periodic_tasks_.size(); ++i) {
        schedulePeriodicTask(i);
    }
}

void ProxyEngine::stopPeriodicMessages() {
    periodic_enabled_ = false;
    for (auto& task : periodic_tasks_) {
        if (task.timer) {
            task.timer->cancel();
        }
    }
}

void ProxyEngine::initializePeriodicUdpSockets() {
    periodic_udp_sockets_.clear();

    boost::system::error_code ec;
    auto default_socket = std::make_unique<boost::asio::ip::udp::socket>(delivery_io_ctx_);
    default_socket->open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        std::cerr << "[Engine] Errore apertura socket UDP periodico default: " << ec.message() << std::endl;
    }
    periodic_udp_sockets_[""] = std::move(default_socket);

    for (const auto& profile : source_profile_bind_ip_) {
        auto sock = std::make_unique<boost::asio::ip::udp::socket>(delivery_io_ctx_);
        ec.clear();
        sock->open(boost::asio::ip::udp::v4(), ec);
        if (ec) {
            std::cerr << "[Engine] Errore apertura socket UDP per profile '" << profile.first
                      << "': " << ec.message() << std::endl;
            continue;
        }

        ec.clear();
        const auto bind_address = boost::asio::ip::make_address(profile.second, ec);
        if (ec) {
            std::cerr << "[Engine] bind_ip non valido per profile '" << profile.first
                      << "': " << ec.message() << std::endl;
            continue;
        }

        ec.clear();
        sock->bind(boost::asio::ip::udp::endpoint(bind_address, 0), ec);
        if (ec) {
            std::cerr << "[Engine] Errore bind socket UDP per profile '" << profile.first
                      << "': " << ec.message() << std::endl;
            continue;
        }

        periodic_udp_sockets_[profile.first] = std::move(sock);
    }
}

void ProxyEngine::schedulePeriodicTask(std::size_t index) {
    if (!periodic_enabled_ || index >= periodic_tasks_.size()) {
        return;
    }

    auto& task = periodic_tasks_[index];
    task.timer->expires_after(std::chrono::milliseconds(task.config.interval_ms));
    task.timer->async_wait([this, index](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            std::cerr << "[Engine] Errore timer periodico: " << ec.message() << std::endl;
            return;
        }

        onPeriodicTaskTick(index);
        if (periodic_enabled_) {
            schedulePeriodicTask(index);
        }
    });
}

void ProxyEngine::onPeriodicTaskTick(std::size_t index) {
    if (index >= periodic_tasks_.size()) {
        return;
    }

    const auto& task = periodic_tasks_[index];
    if (!system_state_) {
        return;
    }

    const SystemStateSnapshot snapshot = system_state_->getSnapshot();
    const std::vector<uint8_t> payload = buildPeriodicPayload(task.config.message_id, snapshot);
    const SendResult send_result = sendPeriodicDatagram(payload, task.config);

    if (!send_result.success) {
        std::cerr << "[Engine] Invio periodico fallito (message_id=" << task.config.message_id
                  << ", protocol=" << (task.config.protocol == PeriodicTransport::UdpMulticast ? "udp_multicast" : "tcp_unicast")
                  << ", endpoint=" << task.config.destination_ip << ":" << task.config.destination_port
                  << ") -> " << send_result.error_message << std::endl;
    }
}

std::vector<uint8_t> ProxyEngine::buildPeriodicPayload(uint32_t message_id, const SystemStateSnapshot& snapshot) const {
    std::vector<uint8_t> payload;

    // Header comune: message_id + timestamp(ms)
    append_u32_be(payload, message_id);
    append_u64_be(payload, snapshot.timestampMs);

    switch (message_id) {
        case 1: {
            // HEARTBEAT v1: [u32 msgId][u64 ts]
            break;
        }
        case 2: {
            // SYSTEM_SUMMARY v1: [header][u8 mode[16]][u16 lrad_count][repeating: u16 lrad_id + u8 flags]
            append_fixed_string(payload, snapshot.systemMode, 16);
            append_u16_be(payload, static_cast<uint16_t>(snapshot.lradStates.size()));
            for (const auto& entry : snapshot.lradStates) {
                append_u16_be(payload, entry.first);

                uint8_t flags = 0;
                const StateUpdate& state = entry.second;
                if (state.online.value_or(false)) {
                    flags |= 0x01;
                }
                if (state.engaged.value_or(false)) {
                    flags |= 0x02;
                }
                if (state.audioEnabled.value_or(false)) {
                    flags |= 0x04;
                }
                payload.push_back(flags);
            }
            break;
        }
        default: {
            // Fallback compatibile: [header][u8 mode[16]]
            append_fixed_string(payload, snapshot.systemMode, 16);
            break;
        }
    }

    return payload;
}

SendResult ProxyEngine::sendPeriodicDatagram(const std::vector<uint8_t>& payload,
                                             const PeriodicMulticastMessageConfig& config) {
    if (config.protocol == PeriodicTransport::TcpUnicast) {
        RawPacket packet;
        packet.data = payload;
        return sender_->send(packet, config.destination_ip, config.destination_port);
    }

    auto socket_it = periodic_udp_sockets_.find(config.source_profile);
    if (socket_it == periodic_udp_sockets_.end()) {
        socket_it = periodic_udp_sockets_.find("");
    }
    if (socket_it == periodic_udp_sockets_.end() || !socket_it->second) {
        SendResult result;
        result.success = false;
        result.error_category = "engine";
        result.error_message = "Socket UDP periodico non disponibile";
        return result;
    }

    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(config.destination_ip, ec);
    if (ec) {
        return make_udp_send_result(ec);
    }

    boost::asio::ip::udp::endpoint destination(address, config.destination_port);
    socket_it->second->send_to(boost::asio::buffer(payload), destination, 0, ec);
    return make_udp_send_result(ec);
}
