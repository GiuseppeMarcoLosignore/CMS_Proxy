#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

class TcpUnicastReceiver : public IReceiver {
public:
    TcpUnicastReceiver(boost::asio::io_context& io_ctx,
                       const std::string& listen_address,
                       uint16_t port);

    void set_callback(MessageCallback cb) override;
    void start() override;
    void stop() override;

private:
    void do_accept();
    void do_read(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket,
                 const std::shared_ptr<std::string>& pending_data);
    void emit_packet(const std::string& payload,
                     const std::shared_ptr<boost::asio::ip::tcp::socket>& socket);

    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<std::shared_ptr<boost::asio::ip::tcp::socket>> client_sockets_;
    MessageCallback callback_;
};
