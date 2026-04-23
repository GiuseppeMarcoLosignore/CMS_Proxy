#include "AcsEntity.hpp"

#include "TcpUnicastReceiver.hpp"
#include "UdpMulticastReceiver.hpp"
#include "cms/CmsEntity.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace {

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



std::optional<StateUpdate> parse_state_update(const nlohmann::json& payload) {
    const nlohmann::json* source = nullptr;
    if (payload.contains("state_update") && payload.at("state_update").is_object()) {
        source = &payload.at("state_update");
    } else if (payload.contains("state") && payload.at("state").is_object()) {
        source = &payload.at("state");
    }

    if (!source) {
        return std::nullopt;
    }

    StateUpdate update;
    if (source->contains("system_mode") && source->at("system_mode").is_string()) {
        update.systemMode = source->at("system_mode").get<std::string>();
    }
    if (source->contains("lrad_id") && source->at("lrad_id").is_number_unsigned()) {
        update.lradId = static_cast<uint16_t>(source->at("lrad_id").get<uint32_t>());
    }
    if (source->contains("cueing_status") && source->at("cueing_status").is_string()) {
        update.cueingStatus = source->at("cueing_status").get<std::string>();
    }
    if (source->contains("configuration") && source->at("configuration").is_string()) {
        update.configuration = source->at("configuration").get<std::string>();
    }
    if (source->contains("online") && source->at("online").is_boolean()) {
        update.online = source->at("online").get<bool>();
    }
    if (source->contains("engaged") && source->at("engaged").is_boolean()) {
        update.engaged = source->at("engaged").get<bool>();
    }
    if (source->contains("audio_enabled") && source->at("audio_enabled").is_boolean()) {
        update.audioEnabled = source->at("audio_enabled").get<bool>();
    }
    if (source->contains("lad_enabled") && source->at("lad_enabled").is_boolean()) {
        update.ladEnabled = source->at("lad_enabled").get<bool>();
    }
    if (source->contains("lrf_enabled") && source->at("lrf_enabled").is_boolean()) {
        update.lrfEnabled = source->at("lrf_enabled").get<bool>();
    }

    return update;
}

} // namespace

AcsEntity::AcsEntity(const AcsConfig& config,
                                         std::shared_ptr<EventBus> eventBus,
                             std::shared_ptr<ISender> tcpSender,
                             std::shared_ptr<ISender> multicastSender)
    : config_(config),
      eventBus_(std::move(eventBus)),
    tcpSender_(std::move(tcpSender)),
    multicastSender_(std::move(multicastSender)),
    destinations_(config_.destinations),
      rxIoContext_(),
      rxWorkGuard_(std::nullopt) {
}

void AcsEntity::start() {
    subscribeTopics();

    auto multicast_receiver = std::make_shared<UdpMulticastReceiver>(
        rxIoContext_,
        config_.listen_ip,
        config_.multicast_group,
        config_.multicast_port
    );

    auto tcp_receiver = std::make_shared<TcpUnicastReceiver>(
        rxIoContext_,
        config_.tcp_listen_ip,
        config_.tcp_listen_port
    );

    multicast_receiver->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    tcp_receiver->set_callback([this](const RawPacket& packet, const PacketSourceInfo& sourceInfo) {
        onPacketReceived(packet, sourceInfo);
    });

    receivers_.clear();
    receivers_.push_back(multicast_receiver);
    receivers_.push_back(tcp_receiver);
    for (const auto& receiver : receivers_) {
        receiver->start();
    }

    rxWorkGuard_.emplace(rxIoContext_.get_executor());
    rxThread_ = std::jthread([this]() {
        rxIoContext_.run();
    });

    std::cout << "[ACS Entity] Avviata su "
              << config_.multicast_group << ":" << config_.multicast_port
              << " (iface " << config_.listen_ip << ")" << std::endl;
    std::cout << "[ACS Entity] TCP unicast in ascolto su "
              << config_.tcp_listen_ip << ":" << config_.tcp_listen_port << std::endl;
}

void AcsEntity::stop() {
    for (const auto& receiver : receivers_) {
        receiver->stop();
    }
    receivers_.clear();

    if (rxWorkGuard_.has_value()) {
        rxWorkGuard_->reset();
    }

    rxIoContext_.stop();
}

void AcsEntity::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    eventBus_->subscribe(Topics::AcsAlive, [this](const EventBus::EventPtr& event) {
        parseALIVE(event);
    });    


    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const EventBus::EventPtr& event) {
        createMASTER(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_emission_control_INS, [this](const EventBus::EventPtr& event) {
        createSEARCHLIGHT(event);
        createAUDIO(event);
        createLAD(event);
        createLRF(event);
        createZOOM(event);
        createLRF(event);
    });   

    eventBus_->subscribe(Topics::CS_LRAS_inhibition_sectors_INS, [this](const EventBus::EventPtr& event) {
        createSHADOW(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_joystick_control_lrad_1_INS, [this](const EventBus::EventPtr& event) {
        createDELTA(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_joystick_control_lrad_2_INS, [this](const EventBus::EventPtr& event) {
        createDELTA(event);
    });
}

void AcsEntity::handleOutgoingJsonEvent(const EventBus::EventPtr& event) {
    const auto outgoing = std::dynamic_pointer_cast<const AcsOutgoingJsonEvent>(event);
    if (!outgoing) {
        return;
    }

    const auto destinationIt = destinations_.find(outgoing->destinationId);
    if (destinationIt == destinations_.end()) {
        std::cerr << "[ACS Entity] Destinazione ACS non configurata: "
                  << outgoing->destinationId << std::endl;
        return;
    }

    sendToTcpDestination(outgoing->packet, destinationIt->second);
    sendToMulticast(outgoing->packet);
}

void AcsEntity::handleStateUpdateEvent(const EventBus::EventPtr& event) {
    const auto stateEvent = std::dynamic_pointer_cast<const AcsStateUpdateEvent>(event);
    if (!stateEvent) {
        return;
    }
    // State updates are no longer persisted; entity simply forwards them via EventBus
}

void AcsEntity::onPacketReceived(const RawPacket& packet, const PacketSourceInfo&) {
    if (!eventBus_) {
        return;
    }

    nlohmann::json payload;
    std::string sendTopic;
    try {
        payload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        sendTopic = payload["sender"];
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido: " << e.what() << std::endl;
        return;
    }

    const auto destinationId = extract_header(payload);
    if (destinationId.has_value()) {
        auto outgoingEvent = std::make_shared<AcsOutgoingJsonEvent>();
        outgoingEvent->Topic = sendTopic;
        outgoingEvent->packet = packet;
        outgoingEvent->payload = payload;
        outgoingEvent->destinationId = *destinationId;
        eventBus_->publish(outgoingEvent);
    }

    const auto stateUpdate = parse_state_update(payload);
    if (stateUpdate.has_value()) {
        auto stateEvent = std::make_shared<AcsStateUpdateEvent>();
        stateEvent->updates.push_back(*stateUpdate);
        eventBus_->publish(stateEvent);
    }
}

void AcsEntity::createHeader(std::string header, std::string type, std::string sender, nlohmann::json param, nlohmann::json& outPayload) {
    outPayload["header"] = header;
    outPayload["type"] = type;
    outPayload["sender"] = sender;
    outPayload["param"] = param;
}

void AcsEntity::parseALIVE(const EventBus::EventPtr& event) {
 if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (!inputPayload.contains("param") || !inputPayload.at("param").is_object()) {
            std::cerr << "[ACS Entity] Campo 'param' mancante o non valido per ALIVE: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        const auto& input = inputPayload.at("param");
        if (!input.contains("name") || !input.at("name").is_string()) {
            std::cerr << "[ACS Entity] Campo 'name' mancante o non valido per ALIVE: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        std::string ip;
        if (input.contains("ip") && input.at("ip").is_string()) {
            ip = input.at("ip").get<std::string>();
        } else if (input.contains("ipAddress") && input.at("ipAddress").is_string()) {
            ip = input.at("ipAddress").get<std::string>();
        }

        if (ip.empty()) {
            std::cerr << "[ACS Entity] Campo 'ip' mancante o non valido per ALIVE: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        const std::string sideName = input.at("name").get<std::string>();
        uint16_t lradId = 0;
        if (sideName == "PORT") {
            lradId = 1;
        } else if (sideName == "STARBOARD") {
            lradId = 2;
        } else {
            std::cerr << "[ACS Entity] Valore 'name' non supportato per ALIVE: "
                      << sideName << std::endl;
            return;
        }

        const char* configPath = "config/network_config.json";
        std::ifstream configInput(configPath);
        if (!configInput.is_open()) {
            std::cerr << "[ACS Entity] Impossibile aprire il file di configurazione: "
                      << configPath << std::endl;
            return;
        }

        nlohmann::json configJson;
        configInput >> configJson;

        if (!configJson.contains("acs") || !configJson.at("acs").is_object() ||
            !configJson.at("acs").contains("destinations") || !configJson.at("acs").at("destinations").is_array()) {
            std::cerr << "[ACS Entity] Struttura 'acs.destinations' non valida in "
                      << configPath << std::endl;
            return;
        }

        bool updated = false;
        for (auto& destination : configJson["acs"]["destinations"]) {
            if (!destination.is_object() || !destination.contains("id") || !destination.at("id").is_number_unsigned()) {
                continue;
            }

            const auto idValue = static_cast<uint16_t>(destination.at("id").get<uint32_t>());
            if (idValue == lradId) {
                destination["name"] = sideName;
                destination["ip"] = ip;
                updated = true;
                break;
            }
        }

        if (!updated) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << lradId << std::endl;
            return;
        }

        std::ofstream configOutput(configPath, std::ios::trunc);
        if (!configOutput.is_open()) {
            std::cerr << "[ACS Entity] Impossibile scrivere il file di configurazione: "
                      << configPath << std::endl;
            return;
        }
        configOutput << configJson.dump(2);

        const auto destinationIt = destinations_.find(lradId);
        if (destinationIt != destinations_.end()) {
            destinationIt->second.ip_address = ip;
        }

        std::cout << "[ACS Entity] ALIVE aggiornato: " << sideName
                  << " -> " << ip << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] Errore parsing/salvataggio ALIVE: " << e.what() << std::endl;
        return;
    }
}


void AcsEntity::createMASTER(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;
    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());

        std::string mode = "";


        inputPayload["Configuration"] == "0" ? param["mode"] = "RELEASE": param["mode"] = "REQ";
        createHeader("MASTER", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per MASTER: " << e.what() << std::endl;
        return;
    }
}



void AcsEntity::createAUDIO(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("Audio Volume dB") && 
            inputPayload.contains("Mute") ) {
            const float& inputParam = inputPayload.at("Audio Volume dB");
            const auto& inputMute = inputPayload.at("Mute");

            param["gain"] = inputParam/2;
            param["mute"] = inputMute == 1 ? true : false;
           
        }
        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per AUDIO: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("AUDIO", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per AUDIO: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createLAD(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("Laser Mode") ) {
            const float& inputParam = inputPayload.at("Laser Mode");

            param["mode"] = inputParam == 0 ? "OFF" : inputParam == 1 ? "ON" : "STROBE";
            param["override"] = false; //feature non prevista per LAD, sempre false


        }
        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per AUDIO: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("LAD", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per LAD: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createSEARCHLIGHT(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("Light Power") && inputPayload.contains("Light Zoom")) {
            const float& inputParam = inputPayload.at("Light Power");
            const float& inputParam2 = inputPayload.at("Light Zoom");

            param["power"] = inputParam == 1 ? "35W" : inputParam == 2 ? "45W" : inputParam == 3 ? "85W" : "35W";
            param["focus"] = inputParam2;
            param["mode"] = inputParam == 0 ? "OFF" : "ON"; 


        }
        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per AUDIO: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("SEARCHLIGHT", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per SEARCHLIGHT: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createLRF(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("LRF on off") ) {
            const float& inputParam = inputPayload.at("LRF on off");

            param["mode"] = inputParam == 0 ? "OFF" : "ON" ;
            param["value"] = -1; //TODO : mappare opportunamente il parametro di potenza del LRF, se previsto 


        }
        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per AUDIO: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("LRF", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per LRF: " << e.what() << std::endl;
        return;
    }
}

//TODO: implementare le altre createXXX per gli altri tipi di comando previsti (es. ZOOM, SHADOW, etc.) mappando opportunamente i parametri in ingresso e quelli richiesti dall'ACS, e gestendo eventuali errori di formato o di parametri mancanti
void AcsEntity::createSHADOW(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    nlohmann::json sectors;
    bool hasSectors = false;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("Sector 1") ) {
            const auto& inputParam = inputPayload.at("Sector 1");
            if(inputParam["On Off"] == 1) {
                hasSectors = true;
                nlohmann::json sector;
                sector["target"] = "AZ";
                sector["start"] = inputParam["start"];
                sector["stop"] = inputParam["stop"];
                sectors.push_back(sector);
            }
        }
        if (inputPayload.contains("Sector 2") ) {
            const auto& inputParam = inputPayload.at("Sector 2");
            if(inputParam["On Off"] == 1) {
                hasSectors = true;
                nlohmann::json sector;
                sector["target"] = "AZ";
                sector["start"] = inputParam["start"];
                sector["stop"] = inputParam["stop"];
                sectors.push_back(sector);
            }
        }

        param["sectors"] = sectors;
        

        createHeader("LRF", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        if (hasSectors) {
            sendToTcpDestination(outPacket, destinationIt->second);
        } else {
            std::cout << "[ACS Entity] Nessun settore di ombreggiamento attivo, non invio comando shadow" << std::endl;
        }
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per LRF: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createZOOM(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("Camera Zoom") ) {
            const auto& inputParam = inputPayload.at("Camera Zoom");

            param["id"] = "HD"; 
            param["value"] = inputParam*0.3; //conversione da step in trentesimi da centesimi

        }
        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per AUDIO: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("ZOOM", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per ZOOM: " << e.what() << std::endl;
        return;
    }
}


void AcsEntity::createPOSITION(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("az") && inputPayload.contains("el") ) {

            param["az"] = inputPayload.at("az");
            param["el"] = inputPayload.at("el");
            param["goTo"] = 2;

        }

        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per POSITION: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("POSITION", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per POSITION: " << e.what() << std::endl;
        return;
    }
}
void AcsEntity::createDELTA(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;
    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());

        if (inputPayload.contains("X position") && inputPayload.contains("Y position")) {
            const auto& X = inputPayload.at("X position");
            const auto& Y = inputPayload.at("Y position");
            
            param["az"] = X*0.1; //TODO: verificare se è necessario scalare i comandi di delta (es. 0.1) per adattarli alla sensibilità richiesta dall'ACS, e se questa scala è la stessa per azimuth e elevazione
            param["el"] = Y*0.1;
        }
        //TODO:gestire anche il caso del cueing status (es. "cueing_status": "ON") per determinare se inviare comandi di delta o di posizione assoluta
        

        createHeader("DELTA", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per DELTA: " << e.what() << std::endl;
        return;
    }
}

void AcsEntity::createTRACKING(const EventBus::EventPtr& event) {
    if (!eventBus_) {
        return;
    }

    const auto dispatchEvent = std::dynamic_pointer_cast<const CmsDispatchTopicPacketEvent>(event);
    if (!dispatchEvent) {
        return;
    }

    const RawPacket& packet = dispatchEvent->packet;

    nlohmann::json inputPayload;
    nlohmann::json param;
    nlohmann::json payload;

    try {
        inputPayload = nlohmann::json::parse(packet.data.begin(), packet.data.end());
        if (inputPayload.contains("Auto tracking") ) {
            const auto& inputParam = inputPayload.at("Auto tracking");

            param["auto"] = inputParam == 0 ? false : true; 
            

        }
        else {
            std::cerr << "[ACS Entity] Parametri mancanti o di tipo errato per AUDIO: "
                      << inputPayload.dump() << std::endl;
            return;
        }

        createHeader("TRACKING", "CMD", "CMS", param, payload);

        const auto destinationIt = destinations_.find(packet.destinationLradId);
        if (destinationIt == destinations_.end()) {
            std::cerr << "[ACS Entity] Destinazione non configurata per LRAD ID: "
                      << packet.destinationLradId << std::endl;
            return;
        }

        const std::string payloadStr = payload.dump();
        RawPacket outPacket;
        outPacket.data.assign(payloadStr.begin(), payloadStr.end());
        outPacket.destinationLradId = packet.destinationLradId;

        sendToTcpDestination(outPacket, destinationIt->second);
        //sendToMulticast(outPacket);
    } catch (const std::exception& e) {
        std::cerr << "[ACS Entity] JSON non valido per TRACKING: " << e.what() << std::endl;
        return;
    }
}


void AcsEntity::sendToTcpDestination(const RawPacket& packet, const AcsDestination& destination) {
    if (!tcpSender_) {
        return;
    }

    const SendResult result = tcpSender_->send(
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
    if (!multicastSender_) {
        return;
    }

    const SendResult result = multicastSender_->send(
        packet,
        config_.tx_multicast_group,
        config_.tx_multicast_port
    );

    if (!result.success) {
        std::cerr << "[ACS Entity] Errore invio UDP multicast verso "
                  << config_.tx_multicast_group << ":" << config_.tx_multicast_port
                  << " -> " << result.error_message << std::endl;
    }
}