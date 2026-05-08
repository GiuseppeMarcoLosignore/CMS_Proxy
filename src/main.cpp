#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <chrono>

#include "AcsEntity.hpp"
#include "AppConfig.hpp"
#include "CmsEntity.hpp"
#include "Orchestrator.hpp"
#include "EventBus.hpp"
#include "TcpSocket.hpp"

int main(int argc, char* argv[]) {
    try {
        const std::string config_path = (argc > 1) ? argv[1] : "config/network_config.ini";
        const AppConfig config = loadAppConfig(config_path);

        auto event_bus = std::make_shared<EventBus>();

        auto rxIoContext = std::make_shared<boost::asio::io_context>();

        auto cms_entity = std::make_shared<CmsEntity>(
            config.cms);
            
        auto acs_entity = std::make_shared<AcsEntity>(
            config.acs
        );

        auto tcpSocket_ = std::make_shared<TcpSocket>(
        *rxIoContext,
        config.acs.tcp_listen_ip,
        config.acs.tcp_listen_port
        );

        

        cms_entity->setMessageCallback([event_bus](const std::string& topic,const uint16_t& lradId, const nlohmann::json& message) {
            event_bus->publish(topic, lradId, message);
        });

        acs_entity->setMessageCallback([event_bus](const std::string& topic, const uint16_t& lradId, const nlohmann::json& message) {
            event_bus->publish(topic, lradId, message);
        });

        cms_entity->start();
        acs_entity->start();



        auto orchestrator = std::make_shared<Orchestrator>(*cms_entity, *acs_entity, event_bus);
        orchestrator->setNetConfigCallback([tcpSocket_](const std::string& ip, uint16_t port) {
            tcpSocket_->configure_listener(ip, port);
        });
        orchestrator->start();

        std::cout << "[SYSTEM] Proxy avviato correttamente con configurazione: "
                  << config_path << std::endl;
        std::cout << "[SYSTEM] CMS multicast in ascolto su: ";
        for (std::size_t i = 0; i < config.cms.multicast_groups.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << config.cms.multicast_groups[i] << ":" << config.cms.multicast_port;
        }
        std::cout << std::endl;
                std::cout << "[SYSTEM] ACS multicast in ascolto su: ";
                for (std::size_t i = 0; i < config.acs.multicast_groups.size(); ++i) {
                    if (i > 0) {
                        std::cout << ", ";
                    }
                    std::cout << config.acs.multicast_groups[i] << ":" << config.acs.multicast_port;
                }
                std::cout << std::endl;
                std::cout << "[SYSTEM] ACS TCP unicast in ascolto su: "
                                    << config.acs.tcp_listen_ip << ":" << config.acs.tcp_listen_port << std::endl;
                std::cout << "[SYSTEM] ACS multicast in invio su: "
                                    << config.acs.tx_multicast_group << ":" << config.acs.tx_multicast_port << std::endl;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL ERROR] " << e.what() << std::endl;
        return 1;
    }
}