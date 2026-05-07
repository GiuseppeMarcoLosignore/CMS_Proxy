#pragma once

namespace Topics {

// ACS
inline constexpr const char* AcsOutgoingJson = "acs.outgoing_json";
inline constexpr const char* AcsAlive = "ALIVE";
inline constexpr const char* AcsDiagnostic = "DIAGNOSTIC";
inline constexpr const char* AcsError = "ERROR";
inline constexpr const char* AcsContext = "CONTEXT";
inline constexpr const char* AcsSet = "SET";
inline constexpr const char* AcsService = "SERVICE";
inline constexpr const char* AcsAlert = "ALERT";
inline constexpr const char* AcsMeteo = "METEO";
inline constexpr const char* AcsPower = "POWER";
inline constexpr const char* AcsReboot = "REBOOT";
inline constexpr const char* AcsAudio = "AUDIO";
inline constexpr const char* AcsLad = "LAD";
inline constexpr const char* AcsSearchlight = "SEARCHLIGHT";
inline constexpr const char* AcsLrf = "LRF";
inline constexpr const char* AcsShadow = "SHADOW";
inline constexpr const char* AcsZoom = "ZOOM";
inline constexpr const char* AcsMaster = "MASTER";
inline constexpr const char* AcsPosition = "POSITION";

// CC - ATOM
inline constexpr const char* CcAtomAudio = "AUDIO";
inline constexpr const char* CcAtomLad = "LAD";
inline constexpr const char* CcAtomSearchlight = "SEARCHLIGHT";
inline constexpr const char* CcAtomLrf = "LRF";
inline constexpr const char* CcAtomStabil = "STABIL";
inline constexpr const char* CcAtomShadow = "SHADOW";
inline constexpr const char* CcAtomZoom = "ZOOM";
inline constexpr const char* CcAtomMaster = "MASTER";
inline constexpr const char* CcAtomMasterGrant = "MASTERGRANT";
inline constexpr const char* CcAtomPosition = "POSITION";
inline constexpr const char* CcAtomDelta = "DELTA";
inline constexpr const char* CcAtomTracking = "TRACKING";
inline constexpr const char* CcAtomReboot = "REBOOT";
inline constexpr const char* CcAtomConfig = "CONFIG";
inline constexpr const char* CcAtomImu = "IMU";
inline constexpr const char* CcAtomNavs = "NAVS";
inline constexpr const char* CcAtomPower = "POWER";

// Runtime config
inline constexpr const char* NetworkConfigChanged = "system.network_config.changed";

inline constexpr const char* LRF_ON = "LRF_ON";
inline constexpr const char* LRF_OFF = "LRF_OFF";
inline constexpr const char* LRF_INFO = "LRF";

inline constexpr const char* LAD_ON = "LAD_ON";
inline constexpr const char* LAD_OFF = "LAD_OFF";
inline constexpr const char* LAD_STROBE = "LAD_STROBE";
inline constexpr const char* LAD_INFO = "LAD";

inline constexpr const char* SEARCHLIGHT_ON = "SEARCHLIGHT_ON";
inline constexpr const char* SEARCHLIGHT_OFF = "SEARCHLIGHT_OFF";
inline constexpr const char* SEARCHLIGHT_POWER = "SEARCHLIGHT_POWER";
inline constexpr const char* SEARCHLIGHT_STROBE = "SEARCHLIGHT_STROBE";
inline constexpr const char* SEARCHLIGHT_FOCUS = "SEARCHLIGHT_FOCUS";
inline constexpr const char* SEARCHLIGHT_INFO = "SEARCHLIGHT";

inline constexpr const char* AUDIO_GAIN = "AUDIO_GAIN";
inline constexpr const char* AUDIO_MUTE = "AUDIO_MUTE";
inline constexpr const char* AUDIO_INFO = "AUDIO";

inline constexpr const char* HD_ZOOM = "HD_ZOOM";
inline constexpr const char* TH_ZOOM = "TH_ZOOM";
inline constexpr const char* ZOOM_INFO = "ZOOM";

inline constexpr const char* LIGHT_ENABLE = "LIGHT_ENABLE";
inline constexpr const char* LAD_ENABLE = "LAD_ENABLE";
inline constexpr const char* LRF_ENABLE = "LRF_ENABLE";
inline constexpr const char* AUDIO_ENABLE = "AUDIO_ENABLE";

inline constexpr const char* CHANGE_REQ = "CHANGE_REQ";
inline constexpr const char* MASTER_INFO = "MASTER";



} // namespace Topics
