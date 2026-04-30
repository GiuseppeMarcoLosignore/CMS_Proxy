#include "AcsEntity.hpp"

#include "TcpSocket.hpp"
#include "UdpSocket.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace {

constexpr const char* kAnyListenIp = "0.0.0.0";

std::string extract_message_type(const nlohmann::json& payload) {
    if (!payload.contains("header") || !payload.at("header").is_object()) {
        return {};
    }

    const auto& header = payload.at("header");
    if (!header.contains("type") || !header.at("type").is_string()) {
        return {};
    }

    return header.at("type").get<std::string>();
}

std::optional<uint16_t> extract_header(const nlohmann::json& payload) {
    const char* keys[] = { "destination_id", "target_id", "id" };
    for (const char* key : keys) {
        if (payload.contains(key) && payload.at(key).is_number_unsigned()) {
            return static_cast<uint16_t>(payload.at(key).get<uint32_t>());
        }
    }

    if (payload.contains("header") && payload.at("header").is_object()) {
        const auto& header = payload.at("header");
        for (const char* key : keys) {
            if (header.contains(key) && header.at(key).is_number_unsigned()) {
                return static_cast<uint16_t>(header.at(key).get<uint32_t>());
            }
        }
    }

    return std::nullopt;
}

std::optional<uint16_t> extract_destination_lrad_id(const nlohmann::json& payload) {
    if (!payload.contains("destinationLradId")) {
        return std::nullopt;
    }

    const auto& value = payload.at("destinationLradId");
    if (value.is_number_unsigned()) {
        return static_cast<uint16_t>(value.get<uint32_t>());
    }

    if (value.is_number_integer()) {
        const int32_t signedValue = value.get<int32_t>();
        if (signedValue >= 0) {
            return static_cast<uint16_t>(signedValue);
        }
    }

    return std::nullopt;
}

} // namespace

AcsEntity::AcsEntity(const AcsConfig& config)
    : config_(config),
    destinations_(config_.destinations),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
}

void AcsEntity::start() {

    AcsConfig startupConfig;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        startupConfig = config_;
    } //graffe per limitare la durata della lock

    std::vector<MulticastEndpoint> multicastEndpoints;
    multicastEndpoints.reserve(startupConfig.multicast_groups.size());
    
    for (const auto& group : startupConfig.multicast_groups) {
        multicastEndpoints.push_back(MulticastEndpoint{group, startupConfig.multicast_port});
    }

    udpSocket_ = std::make_shared<UdpSocket>(
        rxIoContext_,
        kAnyListenIp,
        multicastEndpoints
    );

    tcpSocket_ = std::make_shared<TcpSocket>(
        rxIoContext_,
        startupConfig.tcp_listen_ip,
        startupConfig.tcp_listen_port
    );

    udpSocket_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    tcpSocket_->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    udpSocket_->start();
    tcpSocket_->start();

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[ACS Entity] Avviata su " << startupConfig.multicast_groups.size()
              << " gruppo/i multicast, porta " << startupConfig.multicast_port << std::endl;
    std::cout << "[ACS Entity] TCP unicast in ascolto su "
              << startupConfig.tcp_listen_ip << ":" << startupConfig.tcp_listen_port << std::endl;
}

void AcsEntity::stop() {
    if (udpSocket_) {
        udpSocket_->stop();
    }
    if (tcpSocket_) {
        tcpSocket_->stop();
    }

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}

void AcsEntity::setMessageCallback(std::function<void(const std::string&, const nlohmann::json&)> cb) {
    messageCallback_ = std::move(cb);
}



void AcsEntity::handleConfigChanged(const std::string& topic, const nlohmann::json& message) {
    (void)topic;
    if (!message.contains("acs") || !message.at("acs").is_object()) {
        return;
    }

    const auto& acsJson = message.at("acs");

    std::map<uint16_t, AcsDestination> parsedDestinations;
    if (acsJson.contains("destinations") && acsJson.at("destinations").is_array()) {
        for (const auto& item : acsJson.at("destinations")) {
            if (!item.is_object()) {
                continue;
            }
            if (!item.contains("id") || !item.contains("ip_address") || !item.contains("port")) {
                continue;
            }

            AcsDestination d;
            d.id = static_cast<uint16_t>(item.at("id").get<uint32_t>());
            d.ip_address = item.at("ip_address").get<std::string>();
            d.port = static_cast<uint16_t>(item.at("port").get<uint32_t>());
            parsedDestinations[d.id] = d;
        }
    }

    if (parsedDestinations.empty()) {
        return;
    }

    std::map<uint16_t, AcsDestination> updatedDestinations;
    {
        std::lock_guard<std::mutex> lock(destinationsMutex_);
        updatedDestinations = destinations_;

        const auto lrad1 = parsedDestinations.find(1);
        if (lrad1 != parsedDestinations.end()) {
            updatedDestinations[1] = lrad1->second;
        }

        const auto lrad2 = parsedDestinations.find(2);
        if (lrad2 != parsedDestinations.end()) {
            updatedDestinations[2] = lrad2->second;
        }

        destinations_ = updatedDestinations;
    }

    {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_.destinations = updatedDestinations;
    }

    auto describeDestination = [&updatedDestinations](uint16_t id) {
        const auto it = updatedDestinations.find(id);
        if (it == updatedDestinations.end()) {
            return std::string("n/a");
        }

        return it->second.ip_address + ":" + std::to_string(it->second.port);
    };

    std::cout << "[ACS Entity] Config aggiornata: TCP unicast LRAD1 "
              << describeDestination(1)
              << ", LRAD2 "
              << describeDestination(2)
              << std::endl;
}

std::optional<AcsDestination> AcsEntity::findDestination(uint16_t id) const {
    std::lock_guard<std::mutex> lock(destinationsMutex_);
    const auto destinationIt = destinations_.find(id);
    if (destinationIt == destinations_.end()) {
        return std::nullopt;
    }

    return destinationIt->second;
}

std::optional<AcsDestination> AcsEntity::findDestination(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(destinationsMutex_);
    for (const auto& [_, destination] : destinations_) {
        if (destination.ip_address == ip) {
            return destination;
        }
    }

    return std::nullopt;
}

void AcsEntity::handleOutgoingJsonEvent(const std::string& topic, const nlohmann::json& message) {
    (void)topic;

    const uint16_t destinationLradId = static_cast<uint16_t>(message.value("destinationLradId", 0));
    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione ACS non configurata: "
                  << destinationLradId << std::endl;
        return;
    }

    if (message.is_null()) {
        std::cerr << "[ACS Entity] Payload mancante per invio outgoing" << std::endl;
        return;
    }

    const std::string payloadStr = message.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
    sendToMulticast(outPacket);
}

void AcsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
    if (!messageCallback_) {
        return;
    }

    nlohmann::json payload;
    std::string sendTopic;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        sendTopic = payload["header"];
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido: " << e.what() << std::endl;
        return;
    }


    if(payload["sender"] == "ACS") {
        const auto destination = findDestination(sourceInfo.source_ip);
        if (destination.has_value()) {
            if(destination->id == 1) {
                payload["destinationLradId"] = 1;
            }
            else if(destination->id == 2) {
                payload["destinationLradId"] = 2;
            }
        }
    }

    messageCallback_(sendTopic, payload);

}

void AcsEntity::createHeader(std::string header, std::string type, std::string sender, nlohmann::json param, nlohmann::json& outPayload) {
    outPayload["header"] = header;
    outPayload["type"] = type;
    outPayload["sender"] = sender;
    outPayload["param"] = param;
}






void AcsEntity::createMASTER(uint16_t destinationLradId, const std::string& mode) {
    nlohmann::json param;
    nlohmann::json payload;

    param["mode"] = mode;
    createHeader("MASTER", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}



void AcsEntity::createAUDIO(uint16_t destinationLradId, float gain, bool mute) {
    nlohmann::json param;
    nlohmann::json payload;

    param["gain"] = gain;
    param["mute"] = mute;
    createHeader("AUDIO", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}

void AcsEntity::createLAD(uint16_t destinationLradId, const std::string& mode, bool overrideMode) {
    nlohmann::json param;
    nlohmann::json payload;

    param["mode"] = mode;
    param["override"] = overrideMode;
    createHeader("LAD", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}

void AcsEntity::createSEARCHLIGHT(uint16_t destinationLradId, const std::string& power, float focus, const std::string& mode) {
    nlohmann::json param;
    nlohmann::json payload;

    param["power"] = power;
    param["focus"] = focus;
    param["mode"] = mode;
    createHeader("SEARCHLIGHT", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}

void AcsEntity::createLRF(uint16_t destinationLradId, const std::string& mode) {
    nlohmann::json param;
    nlohmann::json payload;

    param["mode"] = mode;
    createHeader("LRF", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}

//TODO: implementare le altre createXXX per gli altri tipi di comando previsti (es. ZOOM, SHADOW, etc.) mappando opportunamente i parametri in ingresso e quelli richiesti dall'ACS, e gestendo eventuali errori di formato o di parametri mancanti
void AcsEntity::createSHADOW(uint16_t destinationLradId, float az1, float el1, float az2, float el2) {
    nlohmann::json param;
    nlohmann::json payload;
    nlohmann::json sectors;

    if (az1 != 0 || el1 != 0) {
        nlohmann::json sector1;
        sector1["az"] = az1;
        sector1["el"] = el1;
        sectors.push_back(sector1);
    }
    if (az2 != 0 || el2 != 0) {
        nlohmann::json sector2;
        sector2["az"] = az2;
        sector2["el"] = el2;
        sectors.push_back(sector2);
    }

    param["sectors"] = sectors;
    createHeader("SHADOW", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    if (sectors.is_array() && !sectors.empty()) {
        sendToTcpDestination(outPacket, *destination);
    } else {
        std::cout << "[ACS Entity] Nessun settore di ombreggiamento attivo, non invio comando shadow" << std::endl;
    }
}

void AcsEntity::createZOOM(uint16_t destinationLradId, const std::string& id) {
    nlohmann::json param;
    nlohmann::json payload;

    param["id"] = id;
    createHeader("ZOOM", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}


void AcsEntity::createPOSITION(uint16_t destinationLradId, float az, float el, int goTo) {
    nlohmann::json param;
    nlohmann::json payload;

    param["az"] = az;
    param["el"] = el;
    param["goTo"] = goTo;
    createHeader("POSITION", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}
void AcsEntity::createDELTA(uint16_t destinationLradId, float az, float el) {
    nlohmann::json param;
    nlohmann::json payload;

    param["az"] = az;
    param["el"] = el;
    createHeader("DELTA", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}

void AcsEntity::createTRACKING(uint16_t destinationLradId, bool autoTracking) {
    nlohmann::json param;
    nlohmann::json payload;

    param["auto"] = autoTracking;
    createHeader("TRACKING", "CMD", "CMS", param, payload);

    const auto destination = findDestination(destinationLradId);
    if (!destination.has_value()) {
        std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                  << destinationLradId << std::endl;
        return;
    }

    const std::string payloadStr = payload.dump();
    RawPacket outPacket;
    outPacket.data.assign(payloadStr.begin(), payloadStr.end());
    outPacket.destinationLradId = destinationLradId;

    sendToTcpDestination(outPacket, *destination);
}


void AcsEntity::sendToTcpDestination(const RawPacket& packet, const AcsDestination& destination) {
    if (!tcpSocket_) {
        return;
    }

    const SendResult result = tcpSocket_->send(
        packet,
        destination.ip_address,
        destination.port
    );

    if (!result.success) {
        std::cerr << "[ACS Entity] Errore invio TCP JSON verso "
                  << destination.ip_address << ":" << destination.port
                  << " -> " << result.error_message << std::endl;
    }
}

void AcsEntity::sendToMulticast(const RawPacket& packet) {
    if (!udpSocket_) {
        return;
    }

    std::string txGroup;
    uint16_t txPort = 0;
    {
        std::lock_guard<std::mutex> lock(configMutex_);
        txGroup = config_.tx_multicast_group;
        txPort = config_.tx_multicast_port;
    }

    const SendResult result = udpSocket_->send(
        packet,
        txGroup,
        txPort
    );

    if (!result.success) {
        std::cerr << "[ACS Entity] Errore invio UDP multicast verso "
                  << txGroup << ":" << txPort
                  << " -> " << result.error_message << std::endl;
    }
}



