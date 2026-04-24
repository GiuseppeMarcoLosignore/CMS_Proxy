#include "NavsEntity.hpp"

#include "Topics.hpp"
#include "UdpSocket.hpp"
#include "CmsEntity.hpp"

#include <iostream>
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
constexpr const char* kAnyListenIp = "0.0.0.0";

uint32_t read_u32_be(const std::vector<uint8_t>& data, std::size_t offset) {
    if (data.size() < offset + sizeof(uint32_t)) {
        return 0;
    }

    uint32_t net_value = 0;
    std::memcpy(&net_value, data.data() + offset, sizeof(uint32_t));
    return ntohl(net_value);
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

} // namespace

NavsEntity::NavsEntity(const NavsConfig& config,
                       std::shared_ptr<EventBus> eventBus)
    : config_(config),
      eventBus_(std::move(eventBus)),
      topicByMessageId_(),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
    for (const auto& binding : config_.topic_bindings) {
        topicByMessageId_[binding.message_id] = binding.topic;
    }
}

void NavsEntity::start() {
    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    running_.store(true);

    if (!config_.enabled) {
        std::cout << "[NAVS Entity] Disabilitata da configurazione" << std::endl;
        return;
    }

    std::vector<MulticastEndpoint> multicastEndpoints;
    multicastEndpoints.reserve(config_.multicast_groups.size());
    for (const auto& group : config_.multicast_groups) {
        multicastEndpoints.push_back(MulticastEndpoint{group, config_.multicast_port});
    }

    receiver_ = std::make_shared<UdpSocket>(
        rxIoContext_,
        kAnyListenIp,
        multicastEndpoints
    );

    receiver_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    receiver_->start();

    std::cout << "[NAVS Entity] Avviata su " << config_.multicast_groups.size()
              << " gruppo/i multicast, porta " << config_.multicast_port << std::endl;
}

void NavsEntity::stop() {
    running_.store(false);

    if (receiver_) {
        receiver_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}

void NavsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
    if (!eventBus_) {
        return;
    }

    ParsedHeader header;
    if (!parseHeader(packet, header)) {
        std::cerr << "[NAVS Entity] Header non valido o pacchetto incompleto" << std::endl;
        return;
    }

    json payload;
    payload["message_id"] = header.messageId;
    payload["message_length"] = header.messageLength;
    payload["source"] = {
        {"ip", sourceInfo.source_ip},
        {"port", sourceInfo.source_port}
    };
    payload["payload_hex"] = bytes_to_hex(packet.data, HeaderSize);

    const std::string payloadString = payload.dump();

    RawPacket parsedPacket;
    parsedPacket.data.assign(payloadString.begin(), payloadString.end());

    auto dispatchTopicEvent = std::make_shared<CmsDispatchTopicPacketEvent>();
    dispatchTopicEvent->dispatchTopic = resolveTopic(header.messageId);
    dispatchTopicEvent->packet = parsedPacket;
    eventBus_->publish(dispatchTopicEvent);
}

bool NavsEntity::parseHeader(const RawPacket& packet, ParsedHeader& out) const {
    if (packet.data.size() < HeaderSize) {
        return false;
    }

    out.messageId = read_u32_be(packet.data, 0);
    out.messageLength = static_cast<uint16_t>(read_u32_be(packet.data, 4) & 0xFFFF);
    return packet.data.size() >= HeaderSize + out.messageLength;
}

std::string NavsEntity::resolveTopic(uint32_t messageId) const {
    const auto bindingIt = topicByMessageId_.find(messageId);
    if (bindingIt != topicByMessageId_.end()) {
        return bindingIt->second;
    }

    return "navs.unknown";
}
