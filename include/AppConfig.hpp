#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

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
    std::vector<CmsUnicastRelayConfig> unicast_relays;  // relay verso altre entità
};

struct AppConfig {
    CmsConfig cms;
    AcsConfig acs;
};

AppConfig loadAppConfig(const std::string& config_path);
