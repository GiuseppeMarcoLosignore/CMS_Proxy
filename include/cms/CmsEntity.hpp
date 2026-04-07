#pragma once

#include "AppConfig.hpp"
#include "CmsEvents.hpp"
#include "EventBus.hpp"
#include "IEntity.hpp"
#include "IInterfaces.hpp"
#include "SystemState.hpp"
#include "UdpMulticastSender.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <mutex>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>

class CmsEntity : public IEntity {
public:
    CmsEntity(const CmsConfig& config,
              std::shared_ptr<EventBus> eventBus,
              std::shared_ptr<SystemState> systemState);

    void start() override;
    void stop() override;

private:
    struct ParsedHeader {
        uint32_t messageId = 0;
        uint16_t messageLength = 0;
    };

    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void subscribeTopics();
    void handleDispatchTopicEvent(const EventBus::EventPtr& event);
    void handleStateUpdateEvent(const EventBus::EventPtr& event);
    void handlePeriodicTickEvent(const EventBus::EventPtr& event);
    void schedulePeriodicHealthStatusTick();
    void publishPeriodicHealthStatusTick();
    void initializeAckSocket();
    void initializePeriodicMulticastSender();

    ConversionResult convertIncomingPacket(const RawPacket& packet, const SystemStateSnapshot& snapshot) const;
    bool parseHeader(const RawPacket& packet, ParsedHeader& out) const;
    std::vector<RawPacket> parse_CS_LRAS_change_configuration_order_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_cueing_order_cancellation_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_cueing_order_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_emission_control_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_emission_mode_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_inhibition_sectors_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_joystick_control_lrad_1_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_joystick_control_lrad_2_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_recording_command_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_emission_mode_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_engagement_capability_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_full_status_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_installation_data_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_message_table_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_software_version_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_thresholds_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_request_translation_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_LRAS_video_tracking_command_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_MULTI_health_status_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    std::vector<RawPacket> parse_CS_MULTI_update_cst_kinematics_INS(
        const RawPacket& packet,
        std::vector<StateUpdate>& stateUpdates) const;
    RawPacket buildHealthStatusPacket() const;
    RawPacket buildAckPacket(uint32_t actionId, uint32_t sourceMessageId, const SendResult& result) const;
    uint32_t extractActionIdFromPayload(const RawPacket& packet) const;
    SendResult sendTcpToLrad(const RawPacket& packet);
    void sendAck(const RawPacket& packet);
    void sendPeriodicUnicast(const RawPacket& packet);

    CmsConfig config_;
    std::shared_ptr<EventBus> eventBus_;
    std::shared_ptr<SystemState> systemState_;

    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::shared_ptr<IReceiver> receiver_;
    std::optional<boost::asio::steady_timer> periodicHealthTimer_;
    boost::asio::ip::tcp::socket tcpSocket_;
    boost::asio::ip::udp::socket ackSocket_;
    boost::asio::ip::udp::endpoint ackTargetEndpoint_;
    bool ackSocketReady_ = false;
    std::unique_ptr<UdpMulticastSender> periodicMulticastSender_;
    bool periodicUnicastSocketReady_ = false;
    std::unordered_map<std::string, boost::asio::ip::tcp::endpoint> tcpEndpointCache_;
    mutable std::mutex transportMutex_;
    std::jthread rxThread_;
};
