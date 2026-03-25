#pragma once
#include "AppConfig.hpp"
#include "IInterfaces.hpp"
#include <boost/asio.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

class ProxyEngine {
public:
    ProxyEngine(std::shared_ptr<IReceiver> r, 
                std::shared_ptr<IProtocolConverter> c, 
                std::shared_ptr<ISender> s,
                std::shared_ptr<IAckSender> ack_sender,
                boost::asio::io_context& delivery_io_ctx,
                std::map<uint16_t, LradDestination> lrad_config);
    void run();

private:
    void processPacket(const RawPacket& input);

    std::shared_ptr<IReceiver> receiver_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<ISender> sender_;
    std::shared_ptr<IAckSender> ack_sender_;

    boost::asio::io_context& delivery_io_ctx_;
    std::map<uint16_t, LradDestination> lrad_config_;
};