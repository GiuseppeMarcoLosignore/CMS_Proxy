#include <iostream>
#include <memory>
#include <thread>
#include <boost/asio.hpp>

#include "ProxyEngine.hpp"
#include "UdpMulticastReceiver.hpp"
#include "TcpSender.hpp"
#include "BinaryConverter.hpp"

int main() {
    // Inizializzazione socket per Windows
    // (Boost.Asio lo fa internamente, ma è bene sapere che usa Winsock)
    
    try {
        boost::asio::io_context rx_io_ctx;
        boost::asio::io_context delivery_io_ctx;
        auto delivery_work_guard = boost::asio::make_work_guard(delivery_io_ctx);
        std::jthread delivery_thread([&delivery_io_ctx]() {
            delivery_io_ctx.run();
        });

        // PARAMETRI DI RETE (Personalizzali qui)
        std::string listen_ip = "127.0.0.1";      // Ascolta su localhost per test
        std::string mcast_group = "239.0.0.1";  // Gruppo Multicast sorgente
        int mcast_port = 12345;                 // Porta UDP sorgente

        std::string target_ip = "127.0.0.1";    // Server TCP di destinazione (default)
        int target_port = 9000;                 // Porta TCP di destinazione

        // INDIRIZZO UNICAST per JSON (specificare qui l'IP di destinazione)
        std::string unicast_target_ip = "127.0.0.1";  // Modifica con l'IP effettivo

        // ISTANZIAMENTO COMPONENTI
        auto receiver = std::make_shared<UdpMulticastReceiver>(rx_io_ctx, listen_ip, mcast_group, mcast_port);
        auto sender = std::make_shared<TcpSender>(delivery_io_ctx, target_ip, target_port);
        auto converter = std::make_shared<BinaryConverter>();

        // Configura il target unicast per il TcpSender
        sender->set_unicast_target(unicast_target_ip);

        // ASSEMBLAGGIO
        ProxyEngine engine(receiver, converter, sender, delivery_io_ctx);
        engine.run();

        std::cout << "[SYSTEM] Proxy avviato correttamente." << std::endl;
        std::cout << "[SYSTEM] JSON verranno inviati a: " << unicast_target_ip << ":" << target_port << std::endl;

        // AVVIO DEL CICLO EVENTI RX (Bloccante)
        rx_io_ctx.run();

        // In caso di uscita ordinata, fermiamo il worker di delivery.
        delivery_work_guard.reset();
        delivery_io_ctx.stop();

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}