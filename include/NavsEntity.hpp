#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IInterfaces.hpp"

#include <boost/asio.hpp>

#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <atomic>

class NavsEntity : public IEntity {
public:
    NavsEntity(const NavsConfig& config,
               std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    struct ParsedHeader {
        uint32_t messageId = 0;
        uint16_t messageLength = 0;
    };

    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    bool parseHeader(const RawPacket& packet, ParsedHeader& out) const;
    std::string resolveTopic(uint32_t messageId) const;

    NavsConfig config_;
    std::shared_ptr<EventBus> eventBus_;
    std::unordered_map<uint32_t, std::string> topicByMessageId_;

    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::shared_ptr<IReceiver> receiver_;
    std::jthread rxThread_;
    std::atomic<bool> running_{false};
};
