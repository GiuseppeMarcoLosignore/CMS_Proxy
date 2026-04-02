#include "UdpJsonSender.hpp"

#include <iostream>

UdpJsonSender::UdpJsonSender(boost::asio::io_context& io_ctx)
    : socket_(io_ctx) {
    boost::system::error_code ec;
    socket_.open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        throw std::runtime_error("Errore apertura socket UDP JSON: " + ec.message());
    }
}

SendResult UdpJsonSender::send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) {
    SendResult result;

    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(target_host, ec);
    if (ec) {
        result.success = false;
        result.error_value = ec.value();
        result.error_category = ec.category().name();
        result.error_message = ec.message();
        return result;
    }

    boost::asio::ip::udp::endpoint endpoint(address, target_port);
    socket_.send_to(boost::asio::buffer(packet.data), endpoint, 0, ec);
    if (ec) {
        result.success = false;
        result.error_value = ec.value();
        result.error_category = ec.category().name();
        result.error_message = ec.message();
        return result;
    }

    result.success = true;
    return result;
}