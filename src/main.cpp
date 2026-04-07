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
#include "SystemState.hpp"
#include "TcpJsonSender.hpp"

int main(int argc, char* argv[]) {
    try {
        const std::string config_path = (argc > 1) ? argv[1] : "config/network_config.json";
        const AppConfig config = loadAppConfig(config_path);

        boost::asio::io_context delivery_io_ctx;
        auto event_bus = std::make_shared<EventBus>();

        auto system_state = std::make_shared<SystemState>();
        auto acs_sender = std::make_shared<TcpJsonSender>(delivery_io_ctx);

        auto cms_entity = std::make_shared<CmsEntity>(
            config.cms, 
            event_bus, 
            system_state);
            
        auto acs_entity = std::make_shared<AcsEntity>(
            config.acs,
            event_bus,
            acs_sender,
            system_state
        );

        ProxyEngine engine(
            { cms_entity, acs_entity }
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