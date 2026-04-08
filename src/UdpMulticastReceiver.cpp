#include "UdpMulticastReceiver.hpp"

#include <iostream>

UdpMulticastReceiver::UdpMulticastReceiver(boost::asio::io_context& io_ctx,
                                           const std::string& listen_address,
                                           const std::string& multicast_address,
                                           int port)
    : socket_(io_ctx) {
    using namespace boost::asio::ip;

    const udp::endpoint listen_endpoint(make_address(listen_address), static_cast<unsigned short>(port));

    socket_.open(listen_endpoint.protocol());
    socket_.set_option(udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint);
    socket_.set_option(multicast::join_group(make_address(multicast_address)));

    std::cout << "[UDP Receiver] In ascolto su " << multicast_address << ":" << port << std::endl;
}

void UdpMulticastReceiver::set_callback(MessageCallback cb) {
    callback_ = std::move(cb);
}

void UdpMulticastReceiver::start() {
    do_receive();
}

void UdpMulticastReceiver::stop() {
    boost::system::error_code ec;
    socket_.cancel(ec);
    socket_.close(ec);
}

void UdpMulticastReceiver::do_receive() {
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
