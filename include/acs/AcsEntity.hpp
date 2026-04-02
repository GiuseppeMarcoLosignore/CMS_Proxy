#pragma once

#include "AcsEvents.hpp"
#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IEntity.hpp"
#include "IInterfaces.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <optional>
#include <thread>

class AcsEntity : public IEntity {
public:
    AcsEntity(const AcsConfig& config,
              std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);

    AcsConfig config_;
    std::shared_ptr<EventBus> eventBus_;
    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::shared_ptr<IReceiver> receiver_;
    std::jthread rxThread_;
};