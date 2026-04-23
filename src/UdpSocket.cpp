#include "UdpSocket.hpp"

#include <iostream>
#include <stdexcept>

UdpSocket::UdpSocket(boost::asio::io_context& io_ctx)
    : socket_(io_ctx) {
    ensure_socket_open();
    set_ttl(1);
    set_loopback(true);
}

UdpSocket::UdpSocket(boost::asio::io_context& io_ctx,
                     const std::string& listen_address,
                     const std::string& multicast_address,
                     int port)
    : socket_(io_ctx) {
    configure_receiver(listen_address, multicast_address, port);
    set_ttl(1);
    set_loopback(true);

    std::cout << "[UDP Receiver] In ascolto su " << multicast_address << ":" << port << std::endl;
}

void UdpSocket::ensure_socket_open() {
    if (socket_.is_open()) {
        return;
    }

    boost::system::error_code ec;
    socket_.open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        throw std::runtime_error("Errore apertura socket UDP: " + ec.message());
    }
}

void UdpSocket::configure_receiver(const std::string& listen_address,
                                   const std::string& multicast_address,
                                   int port) {
    using namespace boost::asio::ip;

    const udp::endpoint listen_endpoint(make_address(listen_address), static_cast<unsigned short>(port));

    ensure_socket_open();
    socket_.set_option(udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint);
    socket_.set_option(multicast::join_group(make_address(multicast_address)));
    receive_enabled_ = true;
}

void UdpSocket::set_callback(MessageCallback cb) {
    callback_ = std::move(cb);
}

void UdpSocket::start() {
    if (receive_enabled_) {
        do_receive();
    }
}

void UdpSocket::stop() {
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);
}

SendResult UdpSocket::send(const RawPacket& packet,
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

    ensure_socket_open();
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

void UdpSocket::set_ttl(unsigned char ttl) {
    if (!socket_.is_open()) {
        return;
    }

    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::hops(ttl), ec);
}

void UdpSocket::set_loopback(bool enabled) {
    if (!socket_.is_open()) {
        return;
    }

    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::enable_loopback(enabled), ec);
}

void UdpSocket::do_receive() {
    socket_.async_receive_from(
        boost::asio::buffer(buffer_),
        sender_endpoint_,
        [this](const boost::system::error_code& ec, std::size_t bytes_received) {
            if (!ec && bytes_received > 0) {
                RawPacket packet;
                packet.data.assign(
                    buffer_.begin(),
                    buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_received)
                );

                if (callback_) {
                    PacketSourceInfo source_info;
                    source_info.protocol = TransportProtocol::Udp;
                    source_info.source_ip = sender_endpoint_.address().to_string();
                    source_info.source_port = sender_endpoint_.port();
                    callback_(packet, source_info);
                }

                do_receive();
            } else if (ec != boost::asio::error::operation_aborted) {
                std::cerr << "[UDP Error] " << ec.message() << std::endl;
            }
        }
    );
}