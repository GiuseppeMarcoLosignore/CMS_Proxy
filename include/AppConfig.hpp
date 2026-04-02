#pragma once

#include <cstdint>
#include <map>
#include <string>

struct LradDestination {
    uint16_t id;             // L'ID che arriva dal pacchetto (es: 1 o 2)
    std::string ip_address;  // L'IP del server TCP di destinazione
    uint16_t port;           // La porta del server TCP
};

struct AppConfig {
    std::string udp_listen_ip;
    std::string udp_multicast_group;
    uint16_t udp_multicast_port = 0;

    std::string tcp_default_target_ip;
    uint16_t tcp_default_target_port = 0;
    std::string tcp_unicast_target_ip;

    std::string ack_target_ip;
    uint16_t ack_target_port = 0;

    std::map<uint16_t, LradDestination> lrad_destinations;
};

AppConfig loadAppConfig(const std::string& config_path);
