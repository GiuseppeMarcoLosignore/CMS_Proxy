#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IEntity.hpp"
#include "IInterfaces.hpp"

#include <map>
#include <memory>

class TcpSendEventHandler : public IEventHandler {
public:
    TcpSendEventHandler(std::shared_ptr<ISender> sender,
                        std::shared_ptr<EventBus> eventBus,
                        std::map<uint16_t, LradDestination> lradConfig);

    void start() override;
    void stop() override;

private:
    std::shared_ptr<ISender> sender_;
    std::shared_ptr<EventBus> eventBus_;
    std::map<uint16_t, LradDestination> lradConfig_;
};
