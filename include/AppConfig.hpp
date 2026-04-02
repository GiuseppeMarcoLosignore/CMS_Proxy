#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct LradDestination {
    uint16_t id;             // L'ID che arriva dal pacchetto (es: 1 o 2)
    std::string ip_address;  // L'IP del server TCP di destinazione
    uint16_t port;           // La porta del server TCP
};

struct CmsTcpSendHandlerConfig {
    std::map<uint16_t, LradDestination> lrad_destinations;
};

struct CmsAckSendHandlerConfig {
    std::string target_ip;
    uint16_t target_port = 0;
};

struct CmsUdpUnicastSendHandlerConfig {
    bool enabled = false;
    std::string target_ip;
    uint16_t target_port = 0;
};

struct CmsHandlersConfig {
    CmsTcpSendHandlerConfig tcp_send;
    CmsAckSendHandlerConfig ack_send;
    CmsUdpUnicastSendHandlerConfig udp_unicast_send;
};

struct CmsPeriodicHealthStatusConfig {
    bool enabled = false;
    uint32_t interval_ms = 1000;
};

struct CmsUnicastRelayConfig {
    std::string name;               // topic remoto (es: "acs.outgoing_packet")
    std::string destination_ip;
    uint16_t destination_port = 0;
};

struct AcsDestination {
    uint16_t id = 0;
    std::string ip_address;
    uint16_t port = 0;
};

struct AcsConfig {
    std::string listen_ip;
    uint16_t listen_port = 0;
    std::map<uint16_t, AcsDestination> destinations;
};

struct CmsConfig {
    std::string listen_ip;
    std::string multicast_group;
    uint16_t multicast_port = 0;
    CmsHandlersConfig handlers;
    CmsPeriodicHealthStatusConfig periodic_health_status;
    std::vector<CmsUnicastRelayConfig> unicast_relays;  // relay verso altre entità
};

struct AppConfig {
    CmsConfig cms;
    AcsConfig acs;
};

AppConfig loadAppConfig(const std::string& config_path);
