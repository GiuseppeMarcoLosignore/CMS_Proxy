#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IEntity.hpp"
#include "IInterfaces.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <optional>
#include <thread>

class CmsEntity : public IEntity {
public:
    CmsEntity(const CmsConfig& config,
              std::shared_ptr<IProtocolConverter> converter,
              std::shared_ptr<EventBus> eventBus,
              std::shared_ptr<ISender> unicast_relay_sender = nullptr);

    void start() override;
    void stop() override;

private:
    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void onRemoteEventReceived(const std::string& relay_name, const CmsUnicastRelayConfig& relay_cfg,
                              const std::shared_ptr<IEvent>& event);
    void subscribeToRelay(const std::string& relay_name, const CmsUnicastRelayConfig& relay_cfg);
    void relayCallback(const std::shared_ptr<IEvent>& event);

    CmsConfig config_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<ISender> unicast_relay_sender_;
    std::map<std::string, CmsUnicastRelayConfig> relay_config_;

    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::shared_ptr<IReceiver> receiver_;
    std::jthread rxThread_;
};
