#include "TcpJsonSender.hpp"

TcpJsonSender::TcpJsonSender(boost::asio::io_context& io_ctx)
    : io_ctx_(io_ctx) {
}

SendResult TcpJsonSender::send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) {
    SendResult result;

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

    boost::asio::ip::tcp::socket socket(io_ctx_);
    boost::asio::connect(socket, endpoints, ec);
    if (ec) {
        result.success = false;
        result.error_value = ec.value();
        result.error_category = ec.category().name();
        result.error_message = ec.message();
        return result;
    }

    boost::asio::write(socket, boost::asio::buffer(packet.data), ec);
    if (ec) {
        result.success = false;
        result.error_value = ec.value();
        result.error_category = ec.category().name();
        result.error_message = ec.message();
        return result;
    }

    result.success = true;
    return result;
}
