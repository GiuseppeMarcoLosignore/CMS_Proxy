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

class AcsEntity : public IEntity {
public:
    AcsEntity(const AcsConfig& config,
              std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;


    void subscribeTopics();
    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void handleOutgoingJsonEvent(const std::string& topic, const nlohmann::json& message);
    void handleConfigChanged(const std::string& topic, const nlohmann::json& message);
    void sendToTcpDestination(const RawPacket& packet, const AcsDestination& destination);
    void sendToMulticast(const RawPacket& packet);
    std::optional<AcsDestination> findDestination(uint16_t id) const;
    std::optional<AcsDestination> findDestination(const std::string& ip) const;

    void createHeader(std::string header, std::string type, std::string sender, nlohmann::json param, nlohmann::json& outPayload);



    void createAUDIO(const nlohmann::json& message);
    void createLAD(const nlohmann::json& message);
    void createSEARCHLIGHT(const nlohmann::json& message);
    void createLRF(const nlohmann::json& message);
    void createSHADOW(const nlohmann::json& message);
    void createZOOM(const nlohmann::json& message);
    void createMASTER(const nlohmann::json& message);
    void createPOSITION(const nlohmann::json& message);
    void createDELTA(const nlohmann::json& message);
    void createTRACKING(const nlohmann::json& message);

private:
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