#include "UdpMulticastSender.hpp"

#include <stdexcept>

UdpMulticastSender::UdpMulticastSender(boost::asio::io_context& io_ctx)
    : socket_(io_ctx) {
    boost::system::error_code ec;
    socket_.open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        throw std::runtime_error("Errore apertura socket UDP multicast: " + ec.message());
    }

    // Impostazioni conservative di default per invio multicast locale.
    socket_.set_option(boost::asio::ip::multicast::hops(1), ec);
    ec.clear();
    socket_.set_option(boost::asio::ip::multicast::enable_loopback(true), ec);
}

SendResult UdpMulticastSender::send(const RawPacket& packet,
                                    const std::string& target_host,
                                    uint16_t target_port) {
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

    if (!address.is_multicast()) {
        result.success = false;
        result.error_value = -1;
        result.error_category = "multicast";
        result.error_message = "L'indirizzo target non e multicast";
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

void UdpMulticastSender::set_ttl(unsigned char ttl) {
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::hops(ttl), ec);
}

void UdpMulticastSender::set_loopback(bool enabled) {
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::enable_loopback(enabled), ec);
}
