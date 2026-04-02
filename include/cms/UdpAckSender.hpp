#pragma once

#include "IInterfaces.hpp"
#include <boost/asio.hpp>
#include <string>

class UdpAckSender : public IAckSender {
public:
    UdpAckSender(boost::asio::io_context& io_ctx, std::string ack_target_ip, uint16_t ack_target_port);

    void send_ack(const RawPacket& ack_packet) override;

private:
    boost::asio::ip::udp::socket ack_socket_;
    boost::asio::ip::udp::endpoint ack_target_endpoint_;
    bool ack_socket_ready_;
};
