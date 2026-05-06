#pragma once

#include "AppConfig.hpp"
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

class AcsEntity : public IEntity , public IActuator {
public:
    AcsEntity(const AcsConfig& config);

    void start() override;
    void stop() override;

    void setMessageCallback(std::function<void(const std::string&, const uint16_t&, const nlohmann::json&)> cb);

    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void handleOutgoingJsonEvent(const std::string& topic, const nlohmann::json& message);
    void handleConfigChanged(const std::string& topic, const nlohmann::json& message);
    void sendToTcpDestination(const RawPacket& packet, const AcsDestination& destination);
    void sendToMulticast(const RawPacket& packet);
    std::optional<AcsDestination> findDestination(uint16_t id) const;
    std::optional<AcsDestination> findDestination(const std::string& ip) const;

    void createHeader(std::string header, std::string type, std::string sender, nlohmann::json param, nlohmann::json& outPayload);



    void createAUDIO(uint16_t destinationLradId, float gain, bool mute);
    void createLAD(uint16_t destinationLradId, const std::string& mode, bool overrideMode);
    void createSEARCHLIGHT(uint16_t destinationLradId, const uint8_t& power, float focus, const std::string& mode);
    void createLRF(uint16_t destinationLradId, const std::string& mode);
    void createSHADOW(uint16_t destinationLradId, float az1, float el1, float az2, float el2);
    void createZOOM(uint16_t destinationLradId, const std::string& id, const uint8_t value);
    void createMASTER(uint16_t destinationLradId, const std::string& mode);
    void createPOSITION(uint16_t destinationLradId, float az, float el, int goTo);
    void createDELTA(uint16_t destinationLradId, float az, float el);
    void createTRACKING(uint16_t destinationLradId, bool autoTracking);

    void turnLRFon(uint16_t destinationLradId) override;
    void turnLRFoff(uint16_t destinationLradId) override;
    void turnLADon(uint16_t destinationLradId) override;
    void turnLADoff(uint16_t destinationLradId) override;
    void turnLADstrobe(uint16_t destinationLradId) override;
    void turnSearchlightOn(uint16_t destinationLradId) override;
    void turnSearchlightOff(uint16_t destinationLradId) override;
    void turnSearchlightStrobe(uint16_t destinationLradId) override;
    void setSearchlightFocus(uint16_t destinationLradId, float focus) override;
    void setSearchlightPower(uint16_t destinationLradId, const uint8_t& power) override;
    void setGain(uint16_t destinationLradId, float gain) override;
    void setMute(uint16_t destinationLradId, bool mute) override;
    void setHdZoom(uint16_t destinationLradId, const uint8_t zoomValue) override;
    void setThZoom(uint16_t destinationLradId, const uint8_t zoomValue) override;  
    void setChangeRequest(uint16_t destinationLradId, const std::string& mode) override; 

private:
    AcsConfig config_;
    std::shared_ptr<TcpSocket> tcpSocket_;
    std::shared_ptr<UdpSocket> udpSocket_;
    std::map<uint16_t, AcsDestination> destinations_;
    mutable std::mutex configMutex_;
    mutable std::mutex destinationsMutex_;
    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::jthread rxThread_;
    std::function<void(const std::string&, const uint16_t&, const nlohmann::json&)> messageCallback_;
    
};