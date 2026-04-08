#include "TcpUnicastReceiver.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

TcpUnicastReceiver::TcpUnicastReceiver(boost::asio::io_context& io_ctx,
                                       const std::string& listen_address,
                                       uint16_t port)
    : acceptor_(io_ctx) {
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(listen_address, ec);
    if (ec) {
        throw std::runtime_error("Indirizzo TCP listen ACS non valido: " + listen_address);
    }

    const boost::asio::ip::tcp::endpoint endpoint(address, port);
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("Errore apertura acceptor TCP ACS: " + ec.message());
    }

    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    ec.clear();
    acceptor_.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("Errore bind acceptor TCP ACS: " + ec.message());
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("Errore listen TCP ACS: " + ec.message());
    }
}

void TcpUnicastReceiver::set_callback(MessageCallback cb) {
    callback_ = std::move(cb);
}

void TcpUnicastReceiver::start() {
    do_accept();
}

void TcpUnicastReceiver::stop() {
    boost::system::error_code ec;
    acceptor_.cancel(ec);
    acceptor_.close(ec);

    for (const auto& socket : client_sockets_) {
        if (!socket) {
            continue;
        }

        socket->cancel(ec);
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket->close(ec);
    }

    client_sockets_.clear();
}

void TcpUnicastReceiver::do_accept() {
    acceptor_.async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
        if (!ec) {
            auto client_socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));
            client_sockets_.push_back(client_socket);
            auto pending_data = std::make_shared<std::string>();
            do_read(client_socket, pending_data);
        } else if (ec != boost::asio::error::operation_aborted) {
            std::cerr << "[TCP Receiver] Errore accept: " << ec.message() << std::endl;
        }

        if (acceptor_.is_open()) {
            do_accept();
        }
    });
}

void TcpUnicastReceiver::do_read(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                                 const std::shared_ptr<std::string>& pending_data) {
    auto buffer = std::make_shared<std::array<char, 4096>>();

    socket->async_read_some(
        boost::asio::buffer(*buffer),
        [this, socket, buffer, pending_data](const boost::system::error_code& ec, std::size_t bytes_received) {
            if (!ec && bytes_received > 0) {
                pending_data->append(buffer->data(), bytes_received);

                std::size_t separator_pos = pending_data->find('\n');
                while (separator_pos != std::string::npos) {
                    std::string payload = pending_data->substr(0, separator_pos);
                    if (!payload.empty() && payload.back() == '\r') {
                        payload.pop_back();
                    }

                    if (!payload.empty()) {
                        emit_packet(payload, socket);
                    }

                    pending_data->erase(0, separator_pos + 1);
                    separator_pos = pending_data->find('\n');
                }

                do_read(socket, pending_data);
                return;
            }

            if (ec == boost::asio::error::eof) {
                if (!pending_data->empty()) {
                    emit_packet(*pending_data, socket);
                }
            } else if (ec != boost::asio::error::operation_aborted) {
                std::cerr << "[TCP Receiver] Errore read: " << ec.message() << std::endl;
            }

            client_sockets_.erase(
                std::remove(client_sockets_.begin(), client_sockets_.end(), socket),
                client_sockets_.end()
            );
        }
    );
}

void TcpUnicastReceiver::emit_packet(const std::string& payload,
                                     const std::shared_ptr<boost::asio::ip::tcp::socket>& socket) {
    if (!callback_) {
        return;
    }

    PacketSourceInfo source_info;
    source_info.protocol = TransportProtocol::Tcp;

    boost::system::error_code endpoint_ec;
    const auto endpoint = socket->remote_endpoint(endpoint_ec);
    if (!endpoint_ec) {
        source_info.source_ip = endpoint.address().to_string();
        source_info.source_port = endpoint.port();
    }

    RawPacket packet;
    packet.data.assign(payload.begin(), payload.end());
    callback_(packet, source_info);
}
