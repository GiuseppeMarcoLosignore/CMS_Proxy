#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

#include <array>
#include <string>

class UdpUnicastReceiver : public IReceiver {
public:
    UdpUnicastReceiver(boost::asio::io_context& io_ctx,
                       const std::string& listen_address,
                       uint16_t port);

    void set_callback(MessageCallback cb) override;
    void start() override;
    void stop() override;

private:
    void do_receive();

    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint sender_endpoint_;
    std::array<uint8_t, 4096> buffer_;
    MessageCallback callback_;
};