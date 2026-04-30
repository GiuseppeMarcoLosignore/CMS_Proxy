#pragma once

#include "AppConfig.hpp"
#include "IInterfaces.hpp"

#include "Topics.hpp"

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <atomic>

class UdpSocket;

class CmsEntity : public IEntity {
public:
    CmsEntity(const CmsConfig& config);

    void start() override;
    void stop() override;


    struct ParsedHeader {
        uint32_t messageId = 0;
        uint16_t messageLength = 0;
    };

    void onPacketReceived(const RawPacket& packet, const PacketSourceInfo& sourceInfo);
    void subscribeTopics();
    void periodicMessages();

    void setMessageCallback(std::function<void(const std::string&, const nlohmann::json&)> cb);

    bool convertIncomingPacket(const RawPacket& packet,
                               std::string& outTopic,
                               nlohmann::json& outMessage) const;
    bool parseHeader(const RawPacket& packet, ParsedHeader& out) const;

    nlohmann::json parse_CS_LRAS_change_configuration_order_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_cueing_order_cancellation_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_cueing_order_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_emission_control_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_emission_mode_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_inhibition_sectors_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_joystick_control_lrad_1_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_joystick_control_lrad_2_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_recording_command_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_emission_mode_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_engagement_capability_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_full_status_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_installation_data_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_message_table_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_software_version_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_thresholds_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_request_translation_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_LRAS_video_tracking_command_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_MULTI_health_status_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_CS_MULTI_update_cst_kinematics_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;

    nlohmann::json parse_NAVS_MULTI_gyro_fore_nav_data_10ms_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_NAVS_MULTI_health_status_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_NAVS_MULTI_nav_data_100ms_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;
    nlohmann::json parse_NAVS_MULTI_ships_admin_force_time_INS(const RawPacket& packet, uint16_t& destinationLradId, uint16_t& nackreason) const;

    void sendLRAS_CS_ack_INS(const std::string& topic, uint16_t nackreason, const nlohmann::json& message) const;
    void sendLRAS_CS_change_configuration_request_INS(const std::string& topic,
                                                      const nlohmann::json& message,
                                                      uint16_t lradId,
                                                      uint16_t configuration) const;
    void sendLRAS_CS_emission_mode_feedback_INS(const std::string& topic,
                                                const nlohmann::json& message,
                                                uint16_t lradId,
                                                uint16_t audioEnable,
                                                float audioLevel1,
                                                float audioLevel2,
                                                float audioLevel3,
                                                uint16_t laserEnable,
                                                uint32_t laserMinDistance,
                                                uint16_t lightEnable,
                                                uint16_t lightMaxW,
                                                uint16_t lrfEnable) const;
    void sendLRAS_CS_engagement_capability_INS(const std::string& topic,
                                               const nlohmann::json& message,
                                               uint32_t cstn,
                                               uint16_t engagementPossible1,
                                               uint16_t searchlightCapability1,
                                               uint16_t warningCapability1,
                                               float warningMinDb1,
                                               float warningMaxDb1,
                                               uint16_t dissuasionCapability1,
                                               float dissuasionMinDb1,
                                               float dissuasionMaxDb1,
                                               uint16_t persuasionCapability1,
                                               float persuasionMinDb1,
                                               float persuasionMaxDb1,
                                               uint16_t laserDazzlerCapability1,
                                               uint16_t engagementPossible2,
                                               uint16_t searchlightCapability2,
                                               uint16_t warningCapability2,
                                               float warningMinDb2,
                                               float warningMaxDb2,
                                               uint16_t dissuasionCapability2,
                                               float dissuasionMinDb2,
                                               float dissuasionMaxDb2,
                                               uint16_t persuasionCapability2,
                                               float persuasionMinDb2,
                                               float persuasionMaxDb2,
                                               uint16_t laserDazzlerCapability2) const;
    void sendLRAS_CS_hw_limit_warning_INS(const std::string& topic,
                                          const nlohmann::json& message,
                                          uint16_t lradId,
                                          int32_t limit) const;
    void sendLRAS_CS_installation_data_INS(const std::string& topic,
                                           const nlohmann::json& message,
                                           float arcStart1,
                                           float arcStop1,
                                           float x1,
                                           float y1,
                                           float z1,
                                           float arcStart2,
                                           float arcStop2,
                                           float x2,
                                           float y2,
                                           float z2) const;
    void sendLRAS_CS_message_table_INS(const std::string& topic,
                                       const nlohmann::json& message,
                                       uint16_t totalMessagesNumber,
                                       uint16_t messageNumber,
                                       uint16_t dbItemsNumber,
                                       uint32_t messageId,
                                       const std::string& summaryText,
                                       uint16_t numberOfLanguages,
                                       uint32_t recordId,
                                       uint16_t language,
                                       uint8_t associatedAudio,
                                       const std::string& messageText) const;
    void sendLRAS_CS_software_version_INS(const std::string& topic,
                                          const nlohmann::json& message,
                                          const std::string& lrasServerSwName,
                                          const std::string& lrasServerSwVersion,
                                          const std::string& lrad1MasterSwName,
                                          const std::string& lrad1MasterSwVersion,
                                          const std::string& lrad1SlaveSwName,
                                          const std::string& lrad1SlaveSwVersion,
                                          const std::string& lrad1TrackingSwName,
                                          const std::string& lrad1TrackingSwVersion,
                                          const std::string& lrad2MasterSwName,
                                          const std::string& lrad2MasterSwVersion,
                                          const std::string& lrad2SlaveSwName,
                                          const std::string& lrad2SlaveSwVersion,
                                          const std::string& lrad2TrackingSwName,
                                          const std::string& lrad2TrackingSwVersion,
                                          const std::string& console1SwName,
                                          const std::string& console1SwVersion,
                                          const std::string& console2SwName,
                                          const std::string& console2SwVersion) const;
    void sendLRAS_CS_thresholds_INS(const std::string& topic,
                                    const nlohmann::json& message,
                                    uint32_t warningDistance1,
                                    uint32_t dissuasionDistance1,
                                    uint32_t persuasionDistance1,
                                    uint32_t nohdDistance1,
                                    uint32_t acousticDamageDistance1,
                                    uint32_t maxDazzlerDistance1,
                                    uint32_t maxLightDistance1,
                                    uint32_t warningDistance2,
                                    uint32_t dissuasionDistance2,
                                    uint32_t persuasionDistance2,
                                    uint32_t nohdDistance2,
                                    uint32_t acousticDamageDistance2,
                                    uint32_t maxDazzlerDistance2,
                                    uint32_t maxLightDistance2) const;
    void sendLRAS_CS_translation_INS(const std::string& topic,
                                     const nlohmann::json& message,
                                     uint16_t lradId,
                                     uint16_t status,
                                     uint16_t languageIn,
                                     uint16_t languageOut,
                                     const std::string& messageText) const;
    void sendLRAS_CS_lrad_1_status_INS(const std::string& topic,
                                       const nlohmann::json& message,
                                       uint16_t lradStatus,
                                       uint16_t lradMode,
                                       uint16_t cueingStatus,
                                       uint16_t videoTrackingStatus,
                                       float azimuth,
                                       float elevation,
                                       int16_t lrfDistance,
                                       uint16_t inhibitionSectorFlag,
                                       uint16_t warningStep,
                                       uint16_t dissuasionStep,
                                       uint16_t laserDazzlerMode,
                                       uint16_t persuasionStep,
                                       uint16_t laserPulseLength,
                                       uint16_t lightPower) const;
    void sendLRAS_CS_lrad_2_status_INS(const std::string& topic,
                                       const nlohmann::json& message,
                                       uint16_t lradStatus,
                                       uint16_t lradMode,
                                       uint16_t cueingStatus,
                                       uint16_t videoTrackingStatus,
                                       float azimuth,
                                       float elevation,
                                       int16_t lrfDistance,
                                       uint16_t inhibitionSectorFlag,
                                       uint16_t warningStep,
                                       uint16_t dissuasionStep,
                                       uint16_t laserDazzlerMode,
                                       uint16_t persuasionStep,
                                       uint16_t laserPulseLength,
                                       uint16_t lightPower) const;
    void sendLRAS_MULTI_full_status_v2_INS(const std::string& topic,
                                           const nlohmann::json& message,
                                           const std::vector<uint8_t>& lrad1FullStatusBlock,
                                           const std::vector<uint8_t>& lrad2FullStatusBlock) const;
    void sendLRAS_MULTI_health_status_INS(const std::string& topic,
                                          const nlohmann::json& message,
                                          uint16_t systemCondition,
                                          uint16_t systemOperativeState,
                                          uint16_t systemTemperature,
                                          const std::vector<uint8_t>& lrad1HealthBlock,
                                          const std::vector<uint8_t>& lrad2HealthBlock,
                                          uint16_t serverStatus,
                                          uint16_t console1Status,
                                          uint16_t console2Status,
                                          uint16_t console3Status,
                                          uint16_t console4Status) const;
    
    void sendMulticastPacket(const RawPacket& packet, const char* messageName) const;

private:
    CmsConfig config_;
    std::function<void(const std::string&, const nlohmann::json&)> messageCallback_;


    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::optional<boost::asio::steady_timer> periodicTimer_;
    std::shared_ptr<UdpSocket> udpSocket_;
    std::jthread rxThread_;
    std::atomic<bool> subscribed_{false};
    std::atomic<bool> running_{false};
};
