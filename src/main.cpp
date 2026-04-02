#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <chrono>
#include <boost/asio.hpp>

#include "AcsEntity.hpp"
#include "AcsEvents.hpp"
#include "AppConfig.hpp"
#include "CmsEntity.hpp"
#include "CmsEvents.hpp"
#include "EventBus.hpp"
#include "ProxyEngine.hpp"
#include "TcpSender.hpp"
#include "BinaryConverter.hpp"
#include "SystemState.hpp"
#include "UdpAckSender.hpp"
#include "UdpJsonSender.hpp"

int main(int argc, char* argv[]) {
    try {
        const std::string config_path = (argc > 1) ? argv[1] : "config/network_config.json";
        const AppConfig config = loadAppConfig(config_path);

        boost::asio::io_context delivery_io_ctx;
        auto event_bus = std::make_shared<EventBus>();

        auto converter = std::make_shared<BinaryConverter>();
        
        // Creare un sender per il relay unicast (oppure nullptr se non ci sono relay)
        std::shared_ptr<ISender> unicast_relay_sender = nullptr;
        if (!config.cms.unicast_relays.empty()) {
            // TODO: Implementare un sender specifico per relay unicast
            // Per ora usiamo nullptr (il relay non sarà funzionante fino a implementazione)
            unicast_relay_sender = nullptr;
        }
        
        auto cms_entity = std::make_shared<CmsEntity>(config.cms, converter, event_bus, unicast_relay_sender);
        auto acs_entity = std::make_shared<AcsEntity>(config.acs, event_bus);

        const auto first_lrad_destination = config.cms.handlers.tcp_send.lrad_destinations.begin()->second;
        auto sender = std::make_shared<TcpSender>(
            delivery_io_ctx,
            first_lrad_destination.ip_address,
            first_lrad_destination.port
        );
        auto tcp_handler = std::make_shared<TcpSendEventHandler>(
            sender,
            event_bus,
            config.cms.handlers.tcp_send.lrad_destinations
        );

        auto ack_sender = std::make_shared<UdpAckSender>(
            delivery_io_ctx,
            config.cms.handlers.ack_send.target_ip,
            config.cms.handlers.ack_send.target_port
        );
        auto ack_handler = std::make_shared<AckSendEventHandler>(ack_sender, event_bus);

        auto system_state = std::make_shared<SystemState>();
        auto state_handler = std::make_shared<StateUpdateEventHandler>(system_state, event_bus);

        auto periodic_health_builder_handler = std::make_shared<PeriodicHealthStatusBuildEventHandler>(
            system_state,
            event_bus
        );

        auto cms_udp_unicast_sender = std::make_shared<UdpJsonSender>(delivery_io_ctx);
        auto cms_udp_unicast_handler = std::make_shared<CmsUdpUnicastSendEventHandler>(
            cms_udp_unicast_sender,
            event_bus,
            config.cms.handlers.udp_unicast_send.target_ip,
            config.cms.handlers.udp_unicast_send.target_port,
            config.cms.handlers.udp_unicast_send.enabled
        );

        auto acs_sender = std::make_shared<UdpJsonSender>(delivery_io_ctx);
        auto acs_send_handler = std::make_shared<AcsJsonSendEventHandler>(
            acs_sender,
            event_bus,
            config.acs.destinations
        );
        auto acs_state_handler = std::make_shared<AcsStateUpdateEventHandler>(system_state, event_bus);

        ProxyEngine engine(
            { cms_entity, acs_entity },
            {
                tcp_handler,
                ack_handler,
                state_handler,
                periodic_health_builder_handler,
                cms_udp_unicast_handler,
                acs_send_handler,
                acs_state_handler
            }
        );
        engine.run();

        std::cout << "[SYSTEM] Proxy avviato correttamente con configurazione: "
                  << config_path << std::endl;
        std::cout << "[SYSTEM] CMS multicast in ascolto su: "
                  << config.cms.multicast_group << ":" << config.cms.multicast_port << std::endl;
        std::cout << "[SYSTEM] ACS unicast in ascolto su: "
              << config.acs.listen_ip << ":" << config.acs.listen_port << std::endl;

        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}