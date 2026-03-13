#include "TcpSender.hpp"

TcpSender::TcpSender(boost::asio::io_context& io_ctx, const std::string& host, int port)
    : io_ctx_(io_ctx), socket_(io_ctx), host_(host), port_(port) 
{
    boost::asio::ip::tcp::resolver resolver(io_ctx_);
    endpoint_ = *resolver.resolve(host_, std::to_string(port_)).begin();
    connect();
}

void TcpSender::connect() {
    boost::system::error_code ec;
    if (socket_.is_open()) socket_.close();
    
    socket_.connect(endpoint_, ec);
    if (ec) {
        std::cerr << "[TCP Sender] Errore connessione a " << host_ << ":" << port_ << ": " << ec.message() << std::endl;
    } else {
        std::cout << "[TCP Sender] Connesso al server di destinazione." << std::endl;
    }
}

void TcpSender::send(const RawPacket& packet) {
    boost::system::error_code ec;
    boost::asio::write(socket_, boost::asio::buffer(packet.data), ec);

    if (ec) {
        std::cerr << "[TCP Sender] Errore invio: " << ec.message() << ". Riconnessione..." << std::endl;
        connect(); 
        // Opzionale: riprova l'invio dopo la riconnessione
    }
}