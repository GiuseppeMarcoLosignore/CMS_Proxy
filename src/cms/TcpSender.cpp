#include "TcpSender.hpp"

namespace {
std::string describe_transport_error(const boost::system::error_code& ec) {
    using namespace boost::asio;

    if (ec == error::connection_refused) {
        return "Connessione rifiutata dal peer (porta chiusa o servizio non in ascolto).";
    }
    if (ec == error::timed_out) {
        return "Timeout di rete (nessuna risposta entro il tempo previsto).";
    }
    if (ec == error::host_unreachable) {
        return "Host non raggiungibile (routing/host target non disponibile).";
    }
    if (ec == error::network_unreachable) {
        return "Rete non raggiungibile (problema di routing/interfaccia).";
    }
    if (ec == error::not_connected) {
        return "Socket non connesso.";
    }
    if (ec == error::connection_reset) {
        return "Connessione resettata dal peer (RST).";
    }
    if (ec == error::eof) {
        return "Connessione chiusa ordinatamente dal peer (EOF).";
    }
    if (ec == error::broken_pipe) {
        return "Scrittura su connessione non piu valida (broken pipe).";
    }
    if (ec == error::operation_aborted) {
        return "Operazione annullata (socket chiuso o stop richiesto).";
    }

    return std::string("Errore non classificato: [") + ec.category().name() + ":"
        + std::to_string(ec.value()) + "] " + ec.message();
}

SendResult make_send_result_from_ec(const boost::system::error_code& ec) {
    SendResult result;
    result.success = !ec;
    result.error_value = ec.value();
    result.error_category = ec.category().name();
    result.error_message = ec.message();
    return result;
}
}

TcpSender::TcpSender(boost::asio::io_context& io_ctx, const std::string& host, int port)
    : io_ctx_(io_ctx), socket_(io_ctx), host_(host), port_(port), unicast_host_("")
{
    boost::asio::ip::tcp::endpoint initial_endpoint;
    const SendResult result = resolve_target_endpoint(host_, static_cast<uint16_t>(port_), initial_endpoint);
    if (result.success) {
        endpoint_ = initial_endpoint;
        connect();
    } else {
        std::cerr << "[TCP Sender] Avvio senza connessione attiva (host non raggiungibile): "
                  << host_ << ":" << port_ << std::endl;
    }
}

SendResult TcpSender::resolve_target_endpoint(const std::string& target_host, uint16_t target_port,
                                              boost::asio::ip::tcp::endpoint& target_endpoint) {
    const std::string key = target_host + ":" + std::to_string(target_port);
    const auto cache_it = endpoint_cache_.find(key);
    if (cache_it != endpoint_cache_.end()) {
        target_endpoint = cache_it->second;
        SendResult success;
        success.success = true;
        return success;
    }

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(io_ctx_);
    boost::asio::ip::tcp::resolver::results_type results =
        resolver.resolve(target_host, std::to_string(target_port), ec);
    if (ec) {
        std::cerr << "[TCP Sender] Errore risoluzione host " << target_host
                  << " -> " << describe_transport_error(ec) << std::endl;
        return make_send_result_from_ec(ec);
    }
    if (results.begin() == results.end()) {
        std::cerr << "[TCP Sender] Risoluzione host senza endpoint validi per "
                  << target_host << ":" << target_port << std::endl;
        SendResult result;
        result.success = false;
        result.error_value = -1;
        result.error_category = "resolver";
        result.error_message = "Nessun endpoint valido trovato";
        return result;
    }

    target_endpoint = *results.begin();
    endpoint_cache_[key] = target_endpoint;

    SendResult success;
    success.success = true;
    return success;
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
        std::cerr << "[TCP Sender] Errore connessione a " << host_ << ":" << port_
                  << " -> " << describe_transport_error(ec) << std::endl;
    } else {
        std::cout << "[TCP Sender] Connesso al server di destinazione." << std::endl;
    }
}

SendResult TcpSender::send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) {
    boost::system::error_code ec;
    
    // 1. Determina l'endpoint target usando i parametri passati
    boost::asio::ip::tcp::endpoint target_endpoint;
    const SendResult resolve_result = resolve_target_endpoint(target_host, target_port, target_endpoint);
    if (!resolve_result.success) {
        return resolve_result;
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
                      << ":" << target_port << " -> " << describe_transport_error(ec) << std::endl;
            return make_send_result_from_ec(ec);
        } else {
            std::cout << "[TCP Sender] Connessione stabilita con " 
                      << target_host << ":" << target_port << std::endl;
        }
    }
    
    // 4. Invia i dati
    boost::asio::write(socket_, boost::asio::buffer(packet.data), ec);
    if (ec) {
        std::cerr << "[TCP Sender] Errore invio a " << target_host
                  << " -> " << describe_transport_error(ec)
                  << ". Chiudo connessione..." << std::endl;
        socket_.close();
        return make_send_result_from_ec(ec);
    }

    SendResult success;
    success.success = true;
    return success;
}