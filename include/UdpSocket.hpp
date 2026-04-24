#pragma once

#include "IInterfaces.hpp"

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct MulticastEndpoint {
    std::string ip;
    uint16_t port = 0;
};

class UdpSocket : public IReceiver, public ISender {
public:
    explicit UdpSocket(boost::asio::io_context& io_ctx);
    UdpSocket(boost::asio::io_context& io_ctx,
              const std::string& listen_address,
              const std::string& multicast_address,
              int port);
    UdpSocket(boost::asio::io_context& io_ctx,
              const std::string& listen_address,
              const std::vector<std::string>& multicast_addresses,
              int port);
    UdpSocket(boost::asio::io_context& io_ctx,
              const std::string& listen_address,
              const std::vector<MulticastEndpoint>& multicast_endpoints);

    void set_callback(MessageCallback cb) override;
    void start() override;
    void stop() override;

    SendResult send(const RawPacket& packet,
                    const std::string& target_host,
                    uint16_t target_port) override;

    void set_ttl(unsigned char ttl);
    void set_loopback(bool enabled);

private:
    void ensure_socket_open();
    void configure_receiver(const std::string& listen_address,
                            const std::string& multicast_address,
                            int port);
    void configure_receiver(const std::string& listen_address,
                            const std::vector<std::string>& multicast_addresses,
                            int port);
    void configure_receiver(const std::string& listen_address,
                            const std::vector<MulticastEndpoint>& multicast_endpoints);
    void do_receive();

    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint sender_endpoint_;
    std::array<uint8_t, 4096> buffer_;
    MessageCallback callback_;
    bool receive_enabled_ = false;
};