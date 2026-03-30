#pragma once
#include "AppConfig.hpp"
#include "IInterfaces.hpp"
#include "SystemState.hpp"
#include <boost/asio.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ProxyEngine {
public:
    ProxyEngine(std::shared_ptr<IReceiver> r, 
                std::shared_ptr<IProtocolConverter> c, 
                std::shared_ptr<ISender> s,
                std::shared_ptr<IAckSender> ack_sender,
                std::shared_ptr<SystemState> system_state,
                boost::asio::io_context& delivery_io_ctx,
                std::map<uint16_t, LradDestination> lrad_config);
    void run();
    void configurePeriodicMessages(const std::vector<PeriodicMulticastMessageConfig>& periodic_messages,
                                   const std::vector<SourceProfileConfig>& source_profiles);
    void startPeriodicMessages();
    void stopPeriodicMessages();

private:
    struct PeriodicTask {
        PeriodicMulticastMessageConfig config;
        std::unique_ptr<boost::asio::steady_timer> timer;
    };

    void processPacket(const RawPacket& input);
    void initializePeriodicUdpSockets();
    void schedulePeriodicTask(std::size_t index);
    void onPeriodicTaskTick(std::size_t index);
    std::vector<uint8_t> buildPeriodicPayload(uint32_t message_id, const SystemStateSnapshot& snapshot) const;
    SendResult sendPeriodicDatagram(const std::vector<uint8_t>& payload, const PeriodicMulticastMessageConfig& config);

    std::shared_ptr<IReceiver> receiver_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<ISender> sender_;
    std::shared_ptr<IAckSender> ack_sender_;
    std::shared_ptr<SystemState> system_state_;

    boost::asio::io_context& delivery_io_ctx_;
    std::map<uint16_t, LradDestination> lrad_config_;

    std::vector<PeriodicTask> periodic_tasks_;
    std::unordered_map<std::string, std::string> source_profile_bind_ip_;
    std::unordered_map<std::string, std::unique_ptr<boost::asio::ip::udp::socket>> periodic_udp_sockets_;
    bool periodic_enabled_ = false;
};