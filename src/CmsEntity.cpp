#include "CmsEntity.hpp"

#include "CmsEvents.hpp"
#include "UdpMulticastReceiver.hpp"

#include <cstring>
#include <iostream>

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

} // namespace

CmsEntity::CmsEntity(const CmsConfig& config,
                     std::shared_ptr<IProtocolConverter> converter,
                     std::shared_ptr<EventBus> eventBus)
    : config_(config),
      converter_(std::move(converter)),
      eventBus_(std::move(eventBus)),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
}

void CmsEntity::start() {
    receiver_ = std::make_shared<UdpMulticastReceiver>(
        rxIoContext_,
        config_.listen_ip,
        config_.multicast_group,
        config_.multicast_port
    );

    receiver_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    receiver_->start();

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[CMS Entity] Avviata su "
              << config_.multicast_group << ":" << config_.multicast_port << std::endl;
}

void CmsEntity::stop() {
    if (receiver_) {
        receiver_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}

void CmsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!converter_ || !eventBus_) {
        return;
    }

    const uint32_t sourceMessageId = extract_message_id_from_header(packet);
    ConversionResult result = converter_->convert(packet, SystemStateSnapshot{});

    if (result.packets.empty() && !result.ack_only) {
        std::cerr << "[CMS Entity] Messaggio ignorato: source_id=" << sourceMessageId << std::endl;
        return;
    }

    if (result.ack_only) {
        SendResult sendResult;
        sendResult.success = true;

        auto ackEvent = std::make_shared<CmsAckPacketEvent>();
        ackEvent->ackPacket = result.ack_builder(0, sourceMessageId, sendResult);
        eventBus_->publish(ackEvent);
    }

    for (const auto& packetToSend : result.packets) {
        auto outgoingEvent = std::make_shared<CmsOutgoingPacketEvent>();
        outgoingEvent->packet = packetToSend;
        outgoingEvent->ackBuilder = result.ack_builder;
        outgoingEvent->sourceMessageId = sourceMessageId;
        eventBus_->publish(outgoingEvent);
    }

    if (!result.state_updates.empty()) {
        auto stateEvent = std::make_shared<CmsStateUpdateEvent>();
        stateEvent->updates = result.state_updates;
        eventBus_->publish(stateEvent);
    }
}
