#include "TcpSender.hpp"

TcpSender::TcpSender(boost::asio::io_context& io_ctx, const std::string& host, int port)
    : io_ctx_(io_ctx), socket_(io_ctx), host_(host), port_(port), unicast_host_("") 
{
    boost::asio::ip::tcp::resolver resolver(io_ctx_);
    endpoint_ = *resolver.resolve(host_, std::to_string(port_)).begin();
    connect();
}

void TcpSender::set_unicast_target(const std::string& unicast_host) {
    unicast_host_ = unicast_host;
    std::cout << "[TCP Sender] Unicast target configurato: " << unicast_host_ << std::endl;
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
    
    // Determina l'endpoint target
    boost::asio::ip::tcp::endpoint target_endpoint;
    if (!unicast_host_.empty()) {
        boost::asio::ip::tcp::resolver resolver(io_ctx_);
        boost::asio::ip::tcp::resolver::results_type results = 
            resolver.resolve(unicast_host_, std::to_string(port_));
        target_endpoint = *results.begin();
    } else {
        target_endpoint = endpoint_;
    }
    
    // Controlla se dobbiamo riconnettere
    bool need_reconnect = false;
    if (!socket_.is_open()) {
        need_reconnect = true;
    } else {
        try {
            auto current_endpoint = socket_.remote_endpoint();
            if (current_endpoint != target_endpoint) {
                need_reconnect = true;
                socket_.close();
            }
        } catch (const boost::system::system_error&) {
            // Socket non valido, riconnetti
            need_reconnect = true;
            socket_.close();
        }
    }
    
    // Riconnetti se necessario
    if (need_reconnect) {
        socket_.connect(target_endpoint, ec);
        if (ec) {
            std::cerr << "[TCP Sender] Errore connessione a " 
                      << (unicast_host_.empty() ? host_ : unicast_host_) 
                      << ":" << port_ << ": " << ec.message() << std::endl;
            return;
        } else {
            std::cout << "[TCP Sender] Connessione stabilita con " 
                      << (unicast_host_.empty() ? host_ : unicast_host_) 
                      << ":" << port_ << std::endl;
        }
    }
    
    // Invia i dati
    boost::asio::write(socket_, boost::asio::buffer(packet.data), ec);
    if (ec) {
        std::cerr << "[TCP Sender] Errore invio: " << ec.message() 
                  << ". Chiudo connessione..." << std::endl;
        socket_.close();
    }
}