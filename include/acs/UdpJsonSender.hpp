#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

class UdpJsonSender : public ISender {
public:
    explicit UdpJsonSender(boost::asio::io_context& io_ctx);

    SendResult send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) override;

private:
    boost::asio::ip::udp::socket socket_;
};