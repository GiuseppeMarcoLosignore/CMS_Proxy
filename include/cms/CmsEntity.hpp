#pragma once

#include "AppConfig.hpp"
#include "EventBus.hpp"
#include "IEntity.hpp"
#include "IInterfaces.hpp"
#include "IEvent.hpp"
#include "StateUpdate.hpp"
#include "TopicStateUpdateEvent.hpp"

#include "Topics.hpp"

struct CmsDispatchTopicPacketEvent : public IEvent {
    std::string dispatchTopic;
    RawPacket packet;
    uint16_t nackreason = 0;
    const std::string& topic() const override { return dispatchTopic; }
};

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>

class CmsEntity : public IEntity {
public:
    CmsEntity(const CmsConfig& config,
              std::shared_ptr<EventBus> eventBus);

    void start() override;
    void stop() override;

private:
    struct ParsedHeader {
        uint32_t messageId = 0;
        uint16_t messageLength = 0;
    };

    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void subscribeTopics();
    void periodicMessages();

    ConversionResult convertIncomingPacket(const RawPacket& packet) const;
    bool parseHeader(const RawPacket& packet, ParsedHeader& out) const;

    RawPacket parse_CS_LRAS_change_configuration_order_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_cueing_order_cancellation_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_cueing_order_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_emission_control_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_emission_mode_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_inhibition_sectors_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_joystick_control_lrad_1_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_joystick_control_lrad_2_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_recording_command_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_emission_mode_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_engagement_capability_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_full_status_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_installation_data_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_message_table_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_software_version_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_thresholds_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_request_translation_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_LRAS_video_tracking_command_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_MULTI_health_status_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;
    RawPacket parse_CS_MULTI_update_cst_kinematics_INS(const RawPacket& packet, std::vector<StateUpdate>& stateUpdates) const;

    void sendLRAS_CS_ack_INS(const EventBus::EventPtr& event) const;
    void sendLRAS_CS_lrad_1_status_INS(const EventBus::EventPtr& event) const;
    void sendLRAS_CS_lrad_2_status_INS(const EventBus::EventPtr& event) const;
    void sendLRAS_MULTI_full_status_v2_INS(const EventBus::EventPtr& event) const;
    void sendLRAS_MULTI_health_status_INS(const EventBus::EventPtr& event) const;
    CmsConfig config_;
    std::shared_ptr<EventBus> eventBus_;

    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::optional<boost::asio::steady_timer> periodicTimer_;
    std::shared_ptr<IReceiver> receiver_;
    std::jthread rxThread_;
};
