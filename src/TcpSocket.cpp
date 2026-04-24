#include "TcpSocket.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

TcpSocket::TcpSocket(boost::asio::io_context& io_ctx)
    : io_ctx_(io_ctx),
      outgoing_socket_(io_ctx) {
}

TcpSocket::TcpSocket(boost::asio::io_context& io_ctx,
                     const std::string& listen_address,
                     uint16_t port)
    : TcpSocket(io_ctx) {
    configure_listener(listen_address, port);
}

void TcpSocket::configure_listener(const std::string& listen_address, uint16_t port) {
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(listen_address, ec);
    if (ec) {
        throw std::runtime_error("Indirizzo TCP listen ACS non valido: " + listen_address);
    }

    const boost::asio::ip::tcp::endpoint endpoint(address, port);
    acceptor_.emplace(io_ctx_);
    acceptor_->open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("Errore apertura acceptor TCP ACS: " + ec.message());
    }

    acceptor_->set_option(boost::asio::socket_base::reuse_address(true), ec);
    ec.clear();
    acceptor_->bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("Errore bind acceptor TCP ACS: " + ec.message());
    }

    acceptor_->listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("Errore listen TCP ACS: " + ec.message());
    }
}

void TcpSocket::set_callback(MessageCallback cb) {
    callback_ = std::move(cb);
}

void TcpSocket::start() {
    if (acceptor_.has_value()) {
        do_accept();
    }
}

void TcpSocket::stop() {
    boost::system::error_code ec;
    if (acceptor_.has_value()) {
        acceptor_->cancel(ec);
        acceptor_->close(ec);
    }

    for (const auto& socket : client_sockets_) {
        if (!socket) {
            continue;
        }

        socket->cancel(ec);
        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket->close(ec);
    }

    client_sockets_.clear();
    {
        std::lock_guard<std::mutex> lock(outgoing_mutex_);
        close_outgoing_socket();
    }
}

SendResult TcpSocket::send(const RawPacket& packet,
                           const std::string& target_host,
                           uint16_t target_port) {
    SendResult result;

    std::lock_guard<std::mutex> lock(outgoing_mutex_);

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(io_ctx_);
    auto endpoints = resolver.resolve(target_host, std::to_string(target_port), ec);
    if (ec || endpoints.begin() == endpoints.end()) {
        result.success = false;
        result.error_value = ec ? ec.value() : -1;
        result.error_category = ec ? ec.category().name() : "resolver";
        result.error_message = ec ? ec.message() : "Nessun endpoint valido trovato";
        return result;
    }

    const bool target_changed =
        !outgoing_target_host_.has_value() ||
        !outgoing_target_port_.has_value() ||
        *outgoing_target_host_ != target_host ||
        *outgoing_target_port_ != target_port;

    if (target_changed) {
        close_outgoing_socket();
    }

    auto ensure_connected = [&]() -> bool {
        if (outgoing_socket_.is_open()) {
            return true;
        }

        ec.clear();
        outgoing_socket_.connect(*endpoints.begin(), ec);
        if (ec) {
            result.success = false;
            result.error_value = ec.value();
            result.error_category = ec.category().name();
            result.error_message = ec.message();
            close_outgoing_socket();
            return false;
        }

        outgoing_target_host_ = target_host;
        outgoing_target_port_ = target_port;
        return true;
    };

    if (!ensure_connected()) {
        return result;
    }

    ec.clear();
    boost::asio::write(outgoing_socket_, boost::asio::buffer(packet.data), ec);
    if (ec) {
        // Retry once after reconnect in case peer closed the persistent socket.
        close_outgoing_socket();

        if (!ensure_connected()) {
            return result;
        }

        ec.clear();
        boost::asio::write(outgoing_socket_, boost::asio::buffer(packet.data), ec);
        if (ec) {
            result.success = false;
            result.error_value = ec.value();
            result.error_category = ec.category().name();
            result.error_message = ec.message();
            close_outgoing_socket();
            return result;
        }
    }

    result.success = true;
    return result;
}

void TcpSocket::close_outgoing_socket() {
    outgoing_target_host_.reset();
    outgoing_target_port_.reset();

    if (!outgoing_socket_.is_open()) {
        return;
    }

    boost::system::error_code ec;
    outgoing_socket_.cancel(ec);
    outgoing_socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    outgoing_socket_.close(ec);
}

void TcpSocket::do_accept() {
    acceptor_->async_accept([this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
        if (!ec) {
            auto client_socket = std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket));
            client_sockets_.push_back(client_socket);
            auto pending_data = std::make_shared<std::string>();
            do_read(client_socket, pending_data);
        } else if (ec != boost::asio::error::operation_aborted) {
            std::cerr << "[TCP Receiver] Errore accept: " << ec.message() << std::endl;
        }

        if (acceptor_.has_value() && acceptor_->is_open()) {
            do_accept();
        }
    });
}

void TcpSocket::do_read(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
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
                        if (callback_) {
                            callback_(std::move(packet), std::move(source_info));
                        }
                    }

                    pending_data->erase(0, separator_pos + 1);
                    separator_pos = pending_data->find('\n');
                }

                do_read(socket, pending_data);
                return;
            }

            if (ec == boost::asio::error::eof) {
                if (!pending_data->empty()) {
                    PacketSourceInfo source_info;
                    source_info.protocol = TransportProtocol::Tcp;

                    boost::system::error_code endpoint_ec;
                    const auto endpoint = socket->remote_endpoint(endpoint_ec);
                    if (!endpoint_ec) {
                        source_info.source_ip = endpoint.address().to_string();
                        source_info.source_port = endpoint.port();
                    }

                    RawPacket packet;
                    packet.data.assign(pending_data->begin(), pending_data->end());
                    if (callback_) {
                        callback_(std::move(packet), std::move(source_info));
                    }
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