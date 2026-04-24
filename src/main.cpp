#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <chrono>

#include "AcsEntity.hpp"
#include "AppConfig.hpp"
#include "CmsEntity.hpp"
#include "EventBus.hpp"
#include "NavsEntity.hpp"

int main(int argc, char* argv[]) {
    try {
        const std::string config_path = (argc > 1) ? argv[1] : "config/network_config.ini";
        const AppConfig config = loadAppConfig(config_path);

        auto event_bus = std::make_shared<EventBus>();

        auto cms_entity = std::make_shared<CmsEntity>(
            config.cms, 
            event_bus);
            
        auto acs_entity = std::make_shared<AcsEntity>(
            config.acs,
            event_bus
        );

        auto navs_entity = std::make_shared<NavsEntity>(
            config.navs,
            event_bus
        );

        cms_entity->start();
        acs_entity->start();
        navs_entity->start();

        std::cout << "[SYSTEM] Proxy avviato correttamente con configurazione: "
                  << config_path << std::endl;
        std::cout << "[SYSTEM] CMS multicast in ascolto su: "
                  << config.cms.multicast_group << ":" << config.cms.multicast_port << std::endl;
                std::cout << "[SYSTEM] ACS multicast in ascolto su: "
                                    << config.acs.multicast_group << ":" << config.acs.multicast_port
                                    << std::endl;
                std::cout << "[SYSTEM] ACS TCP unicast in ascolto su: "
                                    << config.acs.tcp_listen_ip << ":" << config.acs.tcp_listen_port << std::endl;
                std::cout << "[SYSTEM] ACS multicast in invio su: "
                                    << config.acs.tx_multicast_group << ":" << config.acs.tx_multicast_port << std::endl;
        if (config.navs.enabled) {
            std::cout << "[SYSTEM] NAVS multicast in ascolto su: "
                      << config.navs.multicast_group << ":" << config.navs.multicast_port << std::endl;
        }

        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}