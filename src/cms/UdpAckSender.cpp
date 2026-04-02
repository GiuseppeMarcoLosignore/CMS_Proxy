#include "UdpAckSender.hpp"

#include <iostream>

UdpAckSender::UdpAckSender(boost::asio::io_context& io_ctx, std::string ack_target_ip, uint16_t ack_target_port)
    : ack_socket_(io_ctx),
      ack_target_endpoint_(),
      ack_socket_ready_(false) {
    if (ack_target_ip.empty() || ack_target_port == 0) {
        std::cerr << "[AckSender] Endpoint ACK non configurato (ip/porta)." << std::endl;
        return;
    }

    boost::system::error_code ec;
    const auto target_address = boost::asio::ip::make_address(ack_target_ip, ec);
    if (ec) {
        std::cerr << "[AckSender] IP target ACK non valido: " << ack_target_ip
                  << " -> " << ec.message() << std::endl;
        return;
    }
    ack_target_endpoint_ = boost::asio::ip::udp::endpoint(target_address, ack_target_port);

    ec.clear();
    ack_socket_.open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        std::cerr << "[AckSender] Errore apertura socket UDP ACK: " << ec.message() << std::endl;
        return;
    }

    ack_socket_ready_ = true;
    std::cout << "[AckSender] Invio ACK UDP attivo su target "
              << ack_target_endpoint_.address().to_string() << ":" << ack_target_endpoint_.port() << std::endl;
}

void UdpAckSender::send_ack(const RawPacket& ack_packet) {
    if (!ack_socket_ready_) {
        std::cerr << "[AckSender] Socket ACK non pronto, ACK non inviato." << std::endl;
        return;
    }

    boost::system::error_code ec;
    ack_socket_.send_to(boost::asio::buffer(ack_packet.data), ack_target_endpoint_, 0, ec);
    if (ec) {
        std::cerr << "[AckSender] Errore invio ACK UDP: " << ec.message() << std::endl;
    }
}
