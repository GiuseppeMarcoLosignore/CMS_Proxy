#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

class TcpJsonSender : public ISender {
public:
    explicit TcpJsonSender(boost::asio::io_context& io_ctx);

    SendResult send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) override;

private:
    boost::asio::io_context& io_ctx_;
};
