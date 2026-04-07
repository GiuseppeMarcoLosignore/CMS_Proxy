#pragma once

namespace Topics {

// ACS
inline constexpr const char* AcsOutgoingJson = "acs.outgoing_json";
inline constexpr const char* AcsStateUpdate = "acs.state_update";

// CMS
inline constexpr const char* CmsStateUpdate = "cms.state_update";

// Protocol message topics
inline constexpr const char* COS_LRAS_audio_live_1_PtP = "COS_LRAS_audio_live_1_PtP";
inline constexpr const char* COS_LRAS_audio_live_2_PtP = "COS_LRAS_audio_live_2_PtP";

inline constexpr const char* CS_LRAS_change_configuration_order_INS = "CS_LRAS_change_configuration_order_INS";
inline constexpr const char* CS_LRAS_cueing_order_cancellation_INS = "CS_LRAS_cueing_order_cancellation_INS";
inline constexpr const char* CS_LRAS_cueing_order_INS = "CS_LRAS_cueing_order_INS";
inline constexpr const char* CS_LRAS_emission_control_INS = "CS_LRAS_emission_control_INS";
inline constexpr const char* CS_LRAS_emission_mode_INS = "CS_LRAS_emission_mode_INS";
inline constexpr const char* CS_LRAS_inhibition_sectors_INS = "CS_LRAS_inhibition_sectors_INS";
inline constexpr const char* CS_LRAS_joystick_control_lrad_1_INS = "CS_LRAS_joystick_control_lrad_1_INS";
inline constexpr const char* CS_LRAS_joystick_control_lrad_2_INS = "CS_LRAS_joystick_control_lrad_2_INS";
inline constexpr const char* CS_LRAS_recording_command_INS = "CS_LRAS_recording_command_INS";
inline constexpr const char* CS_LRAS_request_emission_mode_INS = "CS_LRAS_request_emission_mode_INS";
inline constexpr const char* CS_LRAS_request_engagement_capability_INS = "CS_LRAS_request_engagement_capability_INS";
inline constexpr const char* CS_LRAS_request_full_status_INS = "CS_LRAS_request_full_status_INS";
inline constexpr const char* CS_LRAS_request_installation_data_INS = "CS_LRAS_request_installation_data_INS";
inline constexpr const char* CS_LRAS_request_message_table_INS = "CS_LRAS_request_message_table_INS";
inline constexpr const char* CS_LRAS_request_software_version_INS = "CS_LRAS_request_software_version_INS";
inline constexpr const char* CS_LRAS_request_thresholds_INS = "CS_LRAS_request_thresholds_INS";
inline constexpr const char* CS_LRAS_request_translation_INS = "CS_LRAS_request_translation_INS";
inline constexpr const char* CS_LRAS_video_tracking_command_INS = "CS_LRAS_video_tracking_command_INS";

inline constexpr const char* CS_MULTI_health_status_INS = "CS_MULTI_health_status_INS";
inline constexpr const char* CS_MULTI_update_cst_kinematics_INS = "CS_MULTI_update_cst_kinematics_INS";

inline constexpr const char* LRAS_CS_ack_INS = "LRAS_CS_ack_INS";
inline constexpr const char* LRAS_CS_change_configuration_request_INS = "LRAS_CS_change_configuration_request_INS";
inline constexpr const char* LRAS_CS_emission_mode_feedback_INS = "LRAS_CS_emission_mode_feedback_INS";
inline constexpr const char* LRAS_CS_engagement_capability_INS = "LRAS_CS_engagement_capability_INS";
inline constexpr const char* LRAS_CS_hw_limit_warning_INS = "LRAS_CS_hw_limit_warning_INS";
inline constexpr const char* LRAS_CS_installation_data_INS = "LRAS_CS_installation_data_INS";
inline constexpr const char* LRAS_CS_lrad_1_status_INS = "LRAS_CS_lrad_1_status_INS";
inline constexpr const char* LRAS_CS_lrad_2_status_INS = "LRAS_CS_lrad_2_status_INS";
inline constexpr const char* LRAS_CS_message_table_INS = "LRAS_CS_message_table_INS";
inline constexpr const char* LRAS_CS_software_version_INS = "LRAS_CS_software_version_INS";
inline constexpr const char* LRAS_CS_thresholds_INS = "LRAS_CS_thresholds_INS";
inline constexpr const char* LRAS_CS_translation_INS = "LRAS_CS_translation_INS";
inline constexpr const char* LRAS_CS_video_ir_lrad_1_INS = "LRAS_CS_video_ir_lrad_1_INS";
inline constexpr const char* LRAS_CS_video_ir_lrad_2_INS = "LRAS_CS_video_ir_lrad_2_INS";
inline constexpr const char* LRAS_CS_video_lrad_1_INS = "LRAS_CS_video_lrad_1_INS";
inline constexpr const char* LRAS_CS_video_lrad_2_INS = "LRAS_CS_video_lrad_2_INS";

inline constexpr const char* LRAS_MULTI_full_status_v2_INS = "LRAS_MULTI_full_status_v2_INS";
inline constexpr const char* LRAS_MULTI_health_status_INS = "LRAS_MULTI_health_status_INS";

inline constexpr const char* NAVS_MULTI_gyro_fore_nav_data_10ms_INS = "NAVS_MULTI_gyro_fore_nav_data_10ms_INS";
inline constexpr const char* NAVS_MULTI_health_status_INS = "NAVS_MULTI_health_status_INS";
inline constexpr const char* NAVS_MULTI_nav_data_100ms_INS = "NAVS_MULTI_nav_data_100ms_INS";
inline constexpr const char* NAVS_MULTI_ships_admin_force_time_INS = "NAVS_MULTI_ships_admin_force_time_INS";

inline constexpr const char* SFN_LRAS_authorize_lrad_1_PtP = "SFN_LRAS_authorize_lrad_1_PtP";
inline constexpr const char* SFN_LRAS_authorize_lrad_2_PtP = "SFN_LRAS_authorize_lrad_2_PtP";

} // namespace Topics
