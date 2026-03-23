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

void TcpSender::send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) {
    boost::system::error_code ec;
    
    // 1. Determina l'endpoint target usando i parametri passati
    boost::asio::ip::tcp::endpoint target_endpoint;
    try {
        boost::asio::ip::tcp::resolver resolver(io_ctx_);
        // Risolve l'host e la porta passati come argomenti
        boost::asio::ip::tcp::resolver::results_type results = 
            resolver.resolve(target_host, std::to_string(target_port));
        target_endpoint = *results.begin();
    } catch (const boost::system::system_error& e) {
        std::cerr << "[TCP Sender] Errore risoluzione host " << target_host << ": " << e.what() << std::endl;
        return;
    }
    
    // 2. Controlla se dobbiamo riconnettere
    bool need_reconnect = false;
    if (!socket_.is_open()) {
        need_reconnect = true;
    } else {
        try {
            // Se il socket è aperto, verifichiamo se è già connesso alla destinazione richiesta
            auto current_endpoint = socket_.remote_endpoint();
            if (current_endpoint != target_endpoint) {
                std::cout << "[TCP Sender] Cambio destinazione rilevato. Riconnessione..." << std::endl;
                need_reconnect = true;
                socket_.close();
            }
        } catch (const boost::system::system_error&) {
            // Se remote_endpoint() fallisce, il socket è "orfano", riconnetti
            need_reconnect = true;
            socket_.close();
        }
    }
    
    // 3. Riconnetti se necessario (nuova destinazione o socket chiuso)
    if (need_reconnect) {
        socket_.connect(target_endpoint, ec);
        if (ec) {
            std::cerr << "[TCP Sender] Errore connessione a " << target_host 
                      << ":" << target_port << ": " << ec.message() << std::endl;
            return;
        } else {
            std::cout << "[TCP Sender] Connessione stabilita con " 
                      << target_host << ":" << target_port << std::endl;
        }
    }
    
    // 4. Invia i dati
    boost::asio::write(socket_, boost::asio::buffer(packet.data), ec);
    if (ec) {
        std::cerr << "[TCP Sender] Errore invio a " << target_host << ": " << ec.message() 
                  << ". Chiudo connessione..." << std::endl;
        socket_.close();
    }
}