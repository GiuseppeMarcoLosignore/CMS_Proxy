#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <boost/asio.hpp>

#include "AppConfig.hpp"
#include "ProxyEngine.hpp"
#include "UdpMulticastReceiver.hpp"
#include "UdpAckSender.hpp"
#include "TcpSender.hpp"
#include "BinaryConverter.hpp"
#include "SystemState.hpp"

int main(int argc, char* argv[]) {
    // Inizializzazione socket per Windows
    // (Boost.Asio lo fa internamente, ma è bene sapere che usa Winsock)
    
    try {
        const std::string config_path = (argc > 1) ? argv[1] : "config/network_config.json";
        const AppConfig config = loadAppConfig(config_path);

        boost::asio::io_context rx_io_ctx;
        boost::asio::io_context delivery_io_ctx;
        auto delivery_work_guard = boost::asio::make_work_guard(delivery_io_ctx);
        std::jthread delivery_thread([&delivery_io_ctx]() {
            delivery_io_ctx.run();
        });

        // ISTANZIAMENTO COMPONENTI
        auto receiver = std::make_shared<UdpMulticastReceiver>(
            rx_io_ctx,
            config.udp_listen_ip,
            config.udp_multicast_group,
            config.udp_multicast_port
        );
        auto sender = std::make_shared<TcpSender>(
            delivery_io_ctx,
            config.tcp_default_target_ip,
            config.tcp_default_target_port
        );
        auto converter = std::make_shared<BinaryConverter>();
        auto system_state = std::make_shared<SystemState>();
        auto ack_sender = std::make_shared<UdpAckSender>(
            delivery_io_ctx,
            config.ack_target_ip,
            config.ack_target_port
        );

        // Configura il target unicast per il TcpSender
        sender->set_unicast_target(config.tcp_unicast_target_ip);

        // ASSEMBLAGGIO
        auto engine = std::make_shared<ProxyEngine>(
            receiver,
            converter,
            sender,
            ack_sender,
            system_state,
            delivery_io_ctx,
            config.lrad_destinations
        );
        engine->configurePeriodicMessages(config.periodic_messages, config.source_profiles);
        engine->startPeriodicMessages();
        engine->run();

        std::cout << "[SYSTEM] Proxy avviato correttamente con configurazione: "
                  << config_path << std::endl;
        std::cout << "[SYSTEM] JSON verranno inviati a: "
                  << config.tcp_unicast_target_ip << ":" << config.tcp_default_target_port << std::endl;
        
        // AVVIO DEL CICLO EVENTI RX (Bloccante)
        rx_io_ctx.run();

        engine->stopPeriodicMessages();

        // In caso di uscita ordinata, fermiamo il worker di delivery.
        delivery_work_guard.reset();
        delivery_io_ctx.stop();

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}