#pragma once
#include "IInterfaces.hpp"
#include <boost/asio.hpp>
#include <string>
#include <iostream>

class TcpSender : public ISender {
public:
    TcpSender(boost::asio::io_context& io_ctx, const std::string& host, int port);
    
    // Implementazione del contratto ISender
    void send(const RawPacket& packet) override;
    
    // Imposta l'indirizzo unicast di destinazione
    void set_unicast_target(const std::string& unicast_host);

private:
    void connect();

    boost::asio::io_context& io_ctx_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::endpoint endpoint_;
    std::string host_;
    int port_;
    std::string unicast_host_;
};