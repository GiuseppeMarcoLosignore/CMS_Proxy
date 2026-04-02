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
                     std::shared_ptr<EventBus> eventBus,
                     std::shared_ptr<ISender> unicast_relay_sender)
    : config_(config),
      converter_(std::move(converter)),
      eventBus_(std::move(eventBus)),
      unicast_relay_sender_(std::move(unicast_relay_sender)),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
    // Popolare mappa relay_config da config.unicast_relays
    for (const auto& relay : config_.unicast_relays) {
        relay_config_[relay.name] = relay;
    }
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

    // Sottoscrivere agli eventi remoti da relayare
    for (const auto& relay_pair : relay_config_) {
        subscribeToRelay(relay_pair.first, relay_pair.second);
    }

    if (config_.periodic_health_status.enabled) {
        periodicHealthTimer_.emplace(rxIoContext_);
        schedulePeriodicHealthStatusTick();
    }

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[CMS Entity] Avviata su "
              << config_.multicast_group << ":" << config_.multicast_port << std::endl;
    if (!relay_config_.empty()) {
        std::cout << "[CMS Entity] Relay unicast attivi: " << relay_config_.size() << std::endl;
    }
    if (config_.periodic_health_status.enabled) {
        std::cout << "[CMS Entity] Periodic CS_MULTI_health_status_INS attivo: interval_ms="
                  << config_.periodic_health_status.interval_ms << std::endl;
    }
}

void CmsEntity::stop() {
    if (receiver_) {
        receiver_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    if (periodicHealthTimer_.has_value()) {
        boost::system::error_code ec;
        periodicHealthTimer_->cancel();
    }

    rxIoContext_.stop();
}

void CmsEntity::schedulePeriodicHealthStatusTick() {
    if (!periodicHealthTimer_.has_value()) {
        return;
    }

    periodicHealthTimer_->expires_after(std::chrono::milliseconds(config_.periodic_health_status.interval_ms));
    periodicHealthTimer_->async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }

        publishPeriodicHealthStatusTick();
        schedulePeriodicHealthStatusTick();
    });
}

void CmsEntity::publishPeriodicHealthStatusTick() {
    if (!eventBus_) {
        return;
    }

    auto tickEvent = std::make_shared<CmsPeriodicMessageTickEvent>();
    eventBus_->publish(tickEvent);
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

void CmsEntity::subscribeToRelay(const std::string& /*relay_name*/, const CmsUnicastRelayConfig& /*relay_cfg*/) {
    if (!eventBus_) {
        return;
    }

    // TODO: Implementare il relay unicast per topic remoti
    // Per adesso questa funzionalità è disabilitata in attesa di una soluzione
    // ai vincoli di type conversion di MSVC con std::function e lambda/bind
    
    // Quando implementato, questo dovrebbe registrar un callback che:
    // 1. Ascolta su relay_name (topic remoto)
    // 2. Converte l'evento a RawPacket
    // 3. Invia tramite unicast_relay_sender_ a relay_cfg.destination_ip:destination_port
}

void CmsEntity::relayCallback(const std::shared_ptr<IEvent>& /*event*/) {
    // Placeholder - da implementare
}

void CmsEntity::onRemoteEventReceived(const std::string& relay_name,
                                     const CmsUnicastRelayConfig& relay_cfg,
                                     const std::shared_ptr<IEvent>& event) {
    if (!unicast_relay_sender_) {
        return;
    }

    // Converti l'evento a RawPacket per il relay
    // Questo è un template generico che può essere esteso per gestire diversi tipi di event
    RawPacket relay_packet{};
    
    // TODO: Implementare la serializzazione dell'evento a RawPacket
    // in base al tipo specifico di evento (AcsOutgoingPacketEvent, NavOutgoingPacketEvent, etc.)
    
    try {
        unicast_relay_sender_->send(
            relay_packet,
            relay_cfg.destination_ip,
            relay_cfg.destination_port
        );
        std::cout << "[CMS Entity] Relay unicast inviato a " 
                  << relay_cfg.destination_ip << ":" << relay_cfg.destination_port
                  << " (topic: " << relay_name << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[CMS Entity] Errore relay unicast: " << e.what() << std::endl;
    }
}
