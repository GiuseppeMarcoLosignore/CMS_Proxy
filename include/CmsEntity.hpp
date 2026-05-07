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

class CmsEntity : public IEntity, public IRemote {
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
    void eventStatus(const std::string& topic, StatusEventValue value) override;
    void sendControlReq(const uint16_t& lradId) override;
    uint32_t extractMessageIdFromTopic(const char* topic) const;

    void setMessageCallback(std::function<void(const std::string&, const uint16_t&, const nlohmann::json&)> cb);

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

    void sendLRAS_CS_ack_INS(uint32_t actionId,
                             uint32_t sourceMessageId,
                             uint16_t ackNackAccepted,
                             uint16_t nackReason) const;
    void sendLRAS_CS_change_configuration_request_INS(uint16_t lradId,
                                                      uint16_t configuration) const;
    void sendLRAS_CS_emission_mode_feedback_INS(uint16_t lradId,
                                                uint16_t audioEnable,
                                                float audioLevel1,
                                                float audioLevel2,
                                                float audioLevel3,
                                                uint16_t laserEnable,
                                                uint32_t laserMinDistance,
                                                uint16_t lightEnable,
                                                uint16_t lightMaxW,
                                                uint16_t lrfEnable) const;
    void sendLRAS_CS_engagement_capability_INS(uint32_t cstn,
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
    void sendLRAS_CS_hw_limit_warning_INS(uint16_t lradId,
                                          int32_t limit) const;
    void sendLRAS_CS_installation_data_INS(float arcStart1,
                                           float arcStop1,
                                           float x1,
                                           float y1,
                                           float z1,
                                           float arcStart2,
                                           float arcStop2,
                                           float x2,
                                           float y2,
                                           float z2) const;
    void sendLRAS_CS_message_table_INS(uint16_t totalMessagesNumber,
                                       uint16_t messageNumber,
                                       uint16_t dbItemsNumber,
                                       uint32_t messageId,
                                       const std::string& summaryText,
                                       uint16_t numberOfLanguages,
                                       uint32_t recordId,
                                       uint16_t language,
                                       uint8_t associatedAudio,
                                       const std::string& messageText) const;
    void sendLRAS_CS_software_version_INS(const std::string& lrasServerSwName,
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
    void sendLRAS_CS_thresholds_INS(uint32_t warningDistance1,
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
    void sendLRAS_CS_translation_INS(uint16_t lradId,
                                     uint16_t status,
                                     uint16_t languageIn,
                                     uint16_t languageOut,
                                     const std::string& messageText) const;
    void sendLRAS_CS_lrad_1_status_INS(uint16_t lradStatus,
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
    void sendLRAS_CS_lrad_2_status_INS(uint16_t lradStatus,
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
    void sendLRAS_MULTI_full_status_v2_INS(uint16_t lrad1Status,
                                           uint16_t lrad1MotionAzState,
                                           uint16_t lrad1MotionElState,
                                           uint16_t lrad1AudioEmitterMode,
                                           uint16_t lrad1AudioEmitterStatus,
                                           uint16_t lrad1SearchlightMode,
                                           uint16_t lrad1SearchlightStatus,
                                           uint16_t lrad1LaserDazzlerMode,
                                           uint16_t lrad1LaserDazzlerStatus,
                                           uint16_t lrad1LrfStatus,
                                           uint16_t lrad1LrfOnOff,
                                           uint16_t lrad1TrackingBoardStatus,
                                           uint16_t lrad1HdCameraStatus,
                                           uint16_t lrad1HdCameraZoomLevel,
                                           uint16_t lrad1ImuStatus,
                                           uint16_t lrad1CanBus1Status,
                                           uint16_t lrad1CanBus2Status,
                                           uint16_t lrad1CpuSlaveStatus,
                                           uint16_t lrad1ElectronicBoxTemperature,
                                           uint16_t lrad1InterfaceBoxTemperature,
                                           uint16_t lrad1IrCameraStatus,
                                           uint16_t lrad1IrCameraZoomLevel,
                                           uint16_t lrad2Status,
                                           uint16_t lrad2MotionAzState,
                                           uint16_t lrad2MotionElState,
                                           uint16_t lrad2AudioEmitterMode,
                                           uint16_t lrad2AudioEmitterStatus,
                                           uint16_t lrad2SearchlightMode,
                                           uint16_t lrad2SearchlightStatus,
                                           uint16_t lrad2LaserDazzlerMode,
                                           uint16_t lrad2LaserDazzlerStatus,
                                           uint16_t lrad2LrfStatus,
                                           uint16_t lrad2LrfOnOff,
                                           uint16_t lrad2TrackingBoardStatus,
                                           uint16_t lrad2HdCameraStatus,
                                           uint16_t lrad2HdCameraZoomLevel,
                                           uint16_t lrad2ImuStatus,
                                           uint16_t lrad2CanBus1Status,
                                           uint16_t lrad2CanBus2Status,
                                           uint16_t lrad2CpuSlaveStatus,
                                           uint16_t lrad2ElectronicBoxTemperature,
                                           uint16_t lrad2InterfaceBoxTemperature,
                                           uint16_t lrad2IrCameraStatus,
                                           uint16_t lrad2IrCameraZoomLevel) const;
    void sendLRAS_MULTI_health_status_INS(uint16_t systemCondition,
                                          uint16_t systemOperativeState,
                                          uint16_t systemTemperature,
                                          uint16_t lrad1Configuration,
                                          uint16_t lrad1Condition,
                                          uint16_t lrad1OperativeState,
                                          uint16_t lrad1HwEmissionAuthorization,
                                          uint16_t lrad1AudioEmitterCondition,
                                          uint16_t lrad1VolumeLevel,
                                          float lrad1AudioVolumeDb,
                                          uint16_t lrad1Mute,
                                          uint16_t lrad1AudioMode,
                                          uint32_t lrad1RecordedMessageId,
                                          uint16_t lrad1RecordedLanguage,
                                          uint16_t lrad1RecordedLoop,
                                          uint16_t lrad1FreeTextLanguageIn,
                                          uint16_t lrad1FreeTextLanguageOut,
                                          const std::string& lrad1FreeTextMessage,
                                          uint16_t lrad1FreeTextLoop,
                                          uint16_t lrad1SearchlightCondition,
                                          uint16_t lrad1LightPower,
                                          uint16_t lrad1LightZoom,
                                          uint16_t lrad1LaserDazzlerCondition,
                                          uint16_t lrad1LaserMode,
                                          uint16_t lrad1LrfCondition,
                                          uint16_t lrad1LrfOnOff,
                                          uint16_t lrad1CameraCondition,
                                          uint16_t lrad1CameraZoom,
                                          uint16_t lrad1ImuCondition,
                                          uint16_t lrad1RecorderCondition,
                                          uint16_t lrad1RecorderMode,
                                          uint32_t lrad1RecorderElapsedSec,
                                          uint32_t lrad1RecorderElapsedUsec,
                                          uint16_t lrad1HorizontalReference,
                                          uint16_t lrad1InhibitSector1OnOff,
                                          float lrad1InhibitSector1AzStart,
                                          float lrad1InhibitSector1AzStop,
                                          uint16_t lrad1InhibitSector2OnOff,
                                          float lrad1InhibitSector2AzStart,
                                          float lrad1InhibitSector2AzStop,
                                          uint16_t lrad2Configuration,
                                          uint16_t lrad2Condition,
                                          uint16_t lrad2OperativeState,
                                          uint16_t lrad2HwEmissionAuthorization,
                                          uint16_t lrad2AudioEmitterCondition,
                                          uint16_t lrad2VolumeLevel,
                                          float lrad2AudioVolumeDb,
                                          uint16_t lrad2Mute,
                                          uint16_t lrad2AudioMode,
                                          uint32_t lrad2RecordedMessageId,
                                          uint16_t lrad2RecordedLanguage,
                                          uint16_t lrad2RecordedLoop,
                                          uint16_t lrad2FreeTextLanguageIn,
                                          uint16_t lrad2FreeTextLanguageOut,
                                          const std::string& lrad2FreeTextMessage,
                                          uint16_t lrad2FreeTextLoop,
                                          uint16_t lrad2SearchlightCondition,
                                          uint16_t lrad2LightPower,
                                          uint16_t lrad2LightZoom,
                                          uint16_t lrad2LaserDazzlerCondition,
                                          uint16_t lrad2LaserMode,
                                          uint16_t lrad2LrfCondition,
                                          uint16_t lrad2LrfOnOff,
                                          uint16_t lrad2CameraCondition,
                                          uint16_t lrad2CameraZoom,
                                          uint16_t lrad2ImuCondition,
                                          uint16_t lrad2RecorderCondition,
                                          uint16_t lrad2RecorderMode,
                                          uint32_t lrad2RecorderElapsedSec,
                                          uint32_t lrad2RecorderElapsedUsec,
                                          uint16_t lrad2HorizontalReference,
                                          uint16_t lrad2InhibitSector1OnOff,
                                          float lrad2InhibitSector1AzStart,
                                          float lrad2InhibitSector1AzStop,
                                          uint16_t lrad2InhibitSector2OnOff,
                                          float lrad2InhibitSector2AzStart,
                                          float lrad2InhibitSector2AzStop,
                                          uint16_t serverStatus,
                                          uint16_t console1Status,
                                          uint16_t console2Status,
                                          uint16_t console3Status,
                                          uint16_t console4Status) const;
    
    void sendMulticastPacket(const RawPacket& packet, const char* messageName) const;

private:
    CmsConfig config_;
    std::function<void(const std::string&, const uint16_t&, const nlohmann::json&)> messageCallback_;

    mutable std::atomic<uint32_t> lastActionId{0};


    boost::asio::io_context rxIoContext_;
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> rxWorkGuard_;
    std::optional<boost::asio::steady_timer> periodicTimer_;
    std::shared_ptr<UdpSocket> udpSocket_;
    std::jthread rxThread_;
    std::atomic<bool> subscribed_{false};
    std::atomic<bool> running_{false};
};
