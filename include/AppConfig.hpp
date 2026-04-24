#pragma once

#include "IInterfaces.hpp"
#include "Topics.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct AcsDestination {
    uint16_t id = 0;
    std::string ip_address;
    uint16_t port = 0;
};

struct AcsConfig {
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
};

struct NavsTopicBinding {
    uint32_t message_id = 0;
    std::string topic;
};

struct NavsConfig {
    bool enabled = false;
    std::string multicast_group;
    uint16_t multicast_port = 0;
    std::vector<NavsTopicBinding> topic_bindings;
};

struct AppConfig {
    CmsConfig cms;
    AcsConfig acs;
    NavsConfig navs;
};

struct NetworkConfigChangedEvent : public IEvent {
    AcsConfig acs;

    const std::string& topic() const override {
        static const std::string kTopic = Topics::NetworkConfigChanged;
        return kTopic;
    }
};

AppConfig loadAppConfig(const std::string& config_path);
