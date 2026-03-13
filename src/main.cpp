#include <iostream>
#include <memory>
#include <boost/asio.hpp>

#include "ProxyEngine.hpp"
#include "UdpMulticastReceiver.hpp"
#include "TcpSender.hpp"
#include "BinaryConverter.hpp"

int main() {
    // Inizializzazione socket per Windows
    // (Boost.Asio lo fa internamente, ma è bene sapere che usa Winsock)
    
    try {
        boost::asio::io_context io_ctx;

        // PARAMETRI DI RETE (Personalizzali qui)
        std::string listen_ip = "0.0.0.0";      // Ascolta su tutte le interfacce
        std::string mcast_group = "239.0.0.1";  // Gruppo Multicast sorgente
        int mcast_port = 12345;                 // Porta UDP sorgente

        std::string target_ip = "127.0.0.1";    // Server TCP di destinazione
        int target_port = 9000;                 // Porta TCP di destinazione

        // ISTANZIAMENTO COMPONENTI
        auto receiver = std::make_shared<UdpMulticastReceiver>(io_ctx, listen_ip, mcast_group, mcast_port);
        auto sender = std::make_shared<TcpSender>(io_ctx, target_ip, target_port);
        auto converter = std::make_shared<BinaryConverter>();

        // ASSEMBLAGGIO
        ProxyEngine engine(receiver, converter, sender);
        engine.run();

        std::cout << "[SYSTEM] Proxy avviato correttamente." << std::endl;

        // AVVIO DEL CICLO EVENTI (Bloccante)
        io_ctx.run();

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}