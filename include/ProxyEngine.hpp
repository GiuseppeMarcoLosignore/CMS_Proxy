#pragma once
#include "AppConfig.hpp"
#include "IInterfaces.hpp"
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class ProxyEngine {
public:
    ProxyEngine(std::shared_ptr<IReceiver> r, 
                std::shared_ptr<IProtocolConverter> c, 
                std::shared_ptr<ISender> s,
                boost::asio::io_context& delivery_io_ctx,
                std::map<uint16_t, LradDestination> lrad_config,
                std::string ack_target_ip,
                uint16_t ack_target_port);
    void run();

private:
    void sendAck(const RawPacket& ack_packet, const PacketSourceInfo& source_info);
    void processPacket(const RawPacket& input, const PacketSourceInfo& source_info);

    std::shared_ptr<IReceiver> receiver_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<ISender> sender_;

    boost::asio::io_context& delivery_io_ctx_;
    boost::asio::ip::udp::socket ack_socket_;
    std::string ack_target_ip_;
    uint16_t ack_target_port_;
    bool ack_socket_ready_;
    std::map<uint16_t, LradDestination> lrad_config_;
};