#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

class UdpMulticastSender : public ISender {
public:
    explicit UdpMulticastSender(boost::asio::io_context& io_ctx);

    SendResult send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) override;

    void set_ttl(unsigned char ttl);
    void set_loopback(bool enabled);

private:
    boost::asio::ip::udp::socket socket_;
};
