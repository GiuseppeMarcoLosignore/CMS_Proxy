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
    std::string multicast_group;
    uint16_t multicast_port = 0;
    std::string tcp_listen_ip;
    uint16_t tcp_listen_port = 0;
    std::string tx_multicast_group;
    uint16_t tx_multicast_port = 0;
    std::map<uint16_t, AcsDestination> destinations;
};

struct CmsConfig {
    std::string multicast_group;
    uint16_t multicast_port = 0;
    std::vector<CmsUnicastRelayConfig> unicast_relays;  // relay verso altre entità
};

struct NavsTopicBinding {
    uint32_t message_id = 0;
    std::string topic;
};

struct NavsConfig {
    bool enabled = false;
    std::string listen_ip;
    std::string multicast_group;
    uint16_t multicast_port = 0;
    std::vector<NavsTopicBinding> topic_bindings;
};

struct AppConfig {
    CmsConfig cms;
    AcsConfig acs;
    NavsConfig navs;
};

AppConfig loadAppConfig(const std::string& config_path);
