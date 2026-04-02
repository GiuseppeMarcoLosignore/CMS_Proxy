#include "UdpUnicastReceiver.hpp"

#include <iostream>

UdpUnicastReceiver::UdpUnicastReceiver(boost::asio::io_context& io_ctx,
                                       const std::string& listen_address,
                                       uint16_t port)
    : socket_(io_ctx) {
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(listen_address, ec);
    if (ec) {
        throw std::runtime_error("Indirizzo ACS listen non valido: " + listen_address);
    }

    boost::asio::ip::udp::endpoint listen_endpoint(address, port);
    socket_.open(listen_endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("Errore apertura socket UDP ACS: " + ec.message());
    }

    socket_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    ec.clear();
    socket_.bind(listen_endpoint, ec);
    if (ec) {
        throw std::runtime_error("Errore bind socket UDP ACS: " + ec.message());
    }
}

void UdpUnicastReceiver::set_callback(MessageCallback cb) {
    callback_ = std::move(cb);
}

void UdpUnicastReceiver::start() {
    do_receive();
}

void UdpUnicastReceiver::stop() {
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);
}

void UdpUnicastReceiver::do_receive() {
    socket_.async_receive_from(
        boost::asio::buffer(buffer_),
        sender_endpoint_,
        [this](const boost::system::error_code& ec, std::size_t bytes_received) {
            if (!ec && bytes_received > 0) {
                RawPacket packet;
                packet.data.assign(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_received));

                if (callback_) {
                    PacketSourceInfo source_info;
                    source_info.protocol = TransportProtocol::Udp;
                    source_info.source_ip = sender_endpoint_.address().to_string();
                    source_info.source_port = sender_endpoint_.port();
                    callback_(packet, source_info);
                }
            } else if (ec != boost::asio::error::operation_aborted) {
                std::cerr << "[AcsReceiver] Errore ricezione UDP: " << ec.message() << std::endl;
            }

            if (socket_.is_open()) {
                do_receive();
            }
        }
    );
}