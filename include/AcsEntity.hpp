#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IInterfaces.hpp"
#include "Topics.hpp"

#include <nlohmann/json.hpp>
#include <boost/asio.hpp>

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <thread>
#include <mutex>

class TcpSocket;
class UdpSocket;

struct AcsOutgoingJsonEvent : public IEvent {
    std::string Topic;
    RawPacket packet;
    nlohmann::json payload;
    uint16_t destinationId = 0;
    int nackreason = 0;
    const std::string& topic() const override { return Topic; }
};

class AcsEntity : public IEntity {
public:
    AcsEntity(const AcsConfig& config,
              std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    void subscribeTopics();
    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void handleOutgoingJsonEvent(const EventBus::EventPtr& event);
    void handleConfigChanged(const EventBus::EventPtr& event);
    void sendToTcpDestination(const RawPacket& packet, const AcsDestination& destination);
    void sendToMulticast(const RawPacket& packet);
    std::optional<AcsDestination> findDestination(uint16_t id) const;

    void createHeader(std::string header, std::string type, std::string sender, nlohmann::json param, nlohmann::json& outPayload);

    void parseALIVE(const EventBus::EventPtr& event);
    void parseDIAGNOSTIC(const EventBus::EventPtr& event);
    void parseERROR(const EventBus::EventPtr& event);
    void parseAUDIO(const EventBus::EventPtr& event);
    void parseLAD(const EventBus::EventPtr& event);
    void parseSEARCHLIGHT(const EventBus::EventPtr& event);
    void parseLRF(const EventBus::EventPtr& event);
    void parseSHADOW(const EventBus::EventPtr& event);
    void parseZOOM(const EventBus::EventPtr& event);
    void parseMASTER(const EventBus::EventPtr& event);
    void parseCONTEXT(const EventBus::EventPtr& event);
    void parsePOSITION(const EventBus::EventPtr& event);
    void parseTRACKING(const EventBus::EventPtr& event);
    void parseIMU(const EventBus::EventPtr& event);
    void parseHOURS(const EventBus::EventPtr& event);

    void createAUDIO(const EventBus::EventPtr& event);
    void createLAD(const EventBus::EventPtr& event);
    void createSEARCHLIGHT(const EventBus::EventPtr& event);
    void createLRF(const EventBus::EventPtr& event);
    void createSHADOW(const EventBus::EventPtr& event);
    void createZOOM(const EventBus::EventPtr& event);
    void createMASTER(const EventBus::EventPtr& event);
    void createPOSITION(const EventBus::EventPtr& event);
    void createDELTA(const EventBus::EventPtr& event);
    void createTRACKING(const EventBus::EventPtr& event);


    AcsConfig config_;
    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<TcpSocket> tcpSocket_;
    std::shared_ptr<UdpSocket> udpSocket_;
    std::map<uint16_t, AcsDestination> destinations_;
    mutable std::mutex configMutex_;
    mutable std::mutex destinationsMutex_;
    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::jthread rxThread_;
};