#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TcpSocket : public IReceiver, public ISender {
public:
    explicit TcpSocket(boost::asio::io_context& io_ctx);
    TcpSocket(boost::asio::io_context& io_ctx,
              const std::string& listen_address,
              uint16_t port);

    void set_callback(MessageCallback cb) override;
    void start() override;
    void stop() override;

    SendResult send(const RawPacket& packet,
                    const std::string& target_host,
                    uint16_t target_port) override;

private:
    void configure_listener(const std::string& listen_address, uint16_t port);
    void close_outgoing_socket();
    void do_accept();
    void do_read(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                 const std::shared_ptr<std::string>& pending_data);

    boost::asio::io_context& io_ctx_;
    std::optional<boost::asio::ip::tcp::acceptor> acceptor_;
    std::vector<std::shared_ptr<boost::asio::ip::tcp::socket>> client_sockets_;
    boost::asio::ip::tcp::socket outgoing_socket_;
    MessageCallback callback_;
};