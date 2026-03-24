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
                std::string ack_multicast_ip,
                uint16_t ack_multicast_port);
    void run();

private:
    void sendAckToMulticast(const RawPacket& ack_packet);
    void processPacket(const RawPacket& input);

    std::shared_ptr<IReceiver> receiver_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<ISender> sender_;

    boost::asio::io_context& delivery_io_ctx_;
    boost::asio::ip::udp::socket ack_socket_;
    boost::asio::ip::udp::endpoint ack_multicast_endpoint_;
    bool ack_multicast_ready_;
    std::map<uint16_t, LradDestination> lrad_config_;
};