#include "UdpMulticastReceiver.hpp"
#include <iostream>

UdpMulticastReceiver::UdpMulticastReceiver(boost::asio::io_context& io_ctx, 
                                           const std::string& listen_address, 
                                           const std::string& multicast_address, 
                                           int port)
    : socket_(io_ctx) 
{
    using namespace boost::asio::ip;
    udp::endpoint listen_endpoint(make_address(listen_address), port);

    socket_.open(listen_endpoint.protocol());
    socket_.set_option(udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint);

    // Join al gruppo multicast
    socket_.set_option(multicast::join_group(make_address(multicast_address)));
    
    std::cout << "[UDP Receiver] In ascolto su " << multicast_address << ":" << port << std::endl;
}

void UdpMulticastReceiver::set_callback(MessageCallback cb) {
    callback_ = cb;
}

void UdpMulticastReceiver::start() {
    do_receive();
}

void UdpMulticastReceiver::stop() {
    socket_.close();
}

void UdpMulticastReceiver::do_receive() {
    socket_.async_receive_from(
        boost::asio::buffer(buffer_), sender_endpoint_,
        [this](boost::system::error_code ec, std::size_t bytes_recvd) {
            if (!ec) {
                if (callback_) {
                    std::vector<uint8_t> data(buffer_.begin(), buffer_.begin() + bytes_recvd);
                    callback_(RawPacket(std::move(data)));
                }
                do_receive(); // Continua ad ascoltare
            } else if (ec != boost::asio::error::operation_aborted) {
                std::cerr << "[UDP Error] " << ec.message() << std::endl;
            }
        });
}