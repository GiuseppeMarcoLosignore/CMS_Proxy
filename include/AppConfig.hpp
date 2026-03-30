#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class PeriodicTransport {
    UdpMulticast,
    TcpUnicast
};

struct SourceProfileConfig {
    std::string name;
    std::string bind_ip;
};

struct PeriodicMulticastMessageConfig {
    uint32_t message_id = 0;
    std::string name;
    PeriodicTransport protocol = PeriodicTransport::UdpMulticast;
    std::string destination_ip;
    uint16_t destination_port = 0;
    std::string source_profile;
    uint32_t interval_ms = 0;
    bool enabled = true;
};

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
    std::vector<SourceProfileConfig> source_profiles;
    std::vector<PeriodicMulticastMessageConfig> periodic_messages;
};

AppConfig loadAppConfig(const std::string& config_path);
