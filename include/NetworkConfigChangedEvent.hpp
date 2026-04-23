#pragma once

#include "AppConfig.hpp"
#include "IInterfaces.hpp"
#include "Topics.hpp"

struct NetworkConfigChangedEvent : public IEvent {
    CmsConfig cms;
    AcsConfig acs;
    NavsConfig navs;

    const std::string& topic() const override {
        static const std::string kTopic = Topics::NetworkConfigChanged;
        return kTopic;
    }
};
