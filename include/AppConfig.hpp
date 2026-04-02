#pragma once

#include <cstdint>
#include <map>
#include <string>

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

struct CmsHandlersConfig {
    CmsTcpSendHandlerConfig tcp_send;
    CmsAckSendHandlerConfig ack_send;
};

struct CmsConfig {
    std::string listen_ip;
    std::string multicast_group;
    uint16_t multicast_port = 0;
    CmsHandlersConfig handlers;
};

struct AppConfig {
    CmsConfig cms;
};

AppConfig loadAppConfig(const std::string& config_path);
