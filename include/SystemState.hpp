#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "IEvent.hpp"

class EventBus;

// Delta update applicabile allo stato globale del sistema.

struct lrad_status {
    uint16_t lradStatus;           // 1=Operative, 2=Degraded, 3=Failure
    uint16_t lradMode;             // 1=Stand-by, 2=Manual Search, 3=Cueing, 4=Cueing in blind arc, 5=Video Tracking
    uint16_t CueingStatus;         // 0=No Cueing, 1=Cueing in progress, 2=Blind Arc, 3=Cueing in pause
    uint16_t videoTrackingStatus;  // 0=No Tracking, 1=Tracking, 2=Tracking prediction, 3=Target lost
    float azimuth;                 // deg [0..360]
    float elevation;               // deg [-90..90]
    int16_t lrfDistance;           // m [-1..4000], -1 = out of range/not valid
    bool withinInhibitionSector;
    uint16_t searchlightPower;     // 0=Off, 1=35W, 2=45W, 3=85W
    uint16_t searchlightZoom;      // [0..99]
    uint16_t laserDazzlerMode;     // 0=Off, 1=On, 2=Strobo
    uint16_t videoZoom;           // [0..99]
    uint16_t gyroSelection;       // 0=Force Internal IMU, 1=Force Ship Gyro, 2=Automatic
    uint16_t gyroUsed;            // 0=Force Internal IMU, 1=Force Ship Gyro
};

struct full_status_v2 {
    uint16_t communication;                // 0=OK, 1=Failure
    uint16_t motionAzimuthStatus;          // 0=OK, 1=Failure, 2=Overcurrent, 3=Emergency stop
    uint16_t motionElevationStatus;        // 0=OK, 1=Failure, 2=Overcurrent, 3=Emergency stop
    uint16_t audioEmitterOvertemperature;  // 0=Nominal, 1=Overtemperature
    uint16_t audioEmitterCommFailure;      // 0=Nominal, 1=Communication Failure
    uint16_t searchlightOvertemperature;   // 0=Nominal, 1=Overtemperature
    uint16_t searchlightCommFailure;       // 0=Nominal, 1=Communication Failure
    uint16_t laserDazzlerOvertemperature;   // 0=Nominal, 1=Overtemperature
    uint16_t laserDazzlerCommFailure;       // 0=Nominal, 1=Communication Failure
    uint16_t lrfOvertemperature;           // 0=Nominal, 1=Overtemperature (not used for LRF per spec)
    uint16_t lrfCommFailure;               // 0=Nominal, 1=Communication Failure
    uint16_t trackingBoardStatus;          // 0=Operative, 1=No Communications
    uint16_t visibleCameraStatus;          // 0=Operative, 1=No Communications
    uint16_t visibleCameraSignal;          // 0=OK, 1=Not OK
    uint16_t internalImuStatus;           // 0=OK, 1=Absence, 2=Corrupted, 3=Failure
    uint16_t canBus1;                     // 0=OK, 1=Not OK
    uint16_t canBus2;                     // 0=OK, 1=Not OK
    uint16_t cpuSlaveStatus;              // 0=OK, 1=Not OK
    uint16_t electronicBoxTemperature;     // 0=OK, 1=Sensor Failure, 2=Over Temperature
    uint16_t interfaceBoxTemperature;      // 0=OK, 1=Sensor Failure, 2=Over Temperature
    uint16_t irCameraStatus;              // 0=Operative, 1=No Communications
    uint16_t irCameraSignal;              // 0=OK, 1=Not OK
};

struct lrad_healt_status{
    uint16_t lradConfiguration;   // 0=Local, 1=Integrated
    uint16_t lradCondition;       // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    uint16_t lradOperativeState; // 0=Unknown, 1=Operative, 2=Not Operative
    uint16_t hwEmissionAuth;      // 0=FALSE, 1=TRUE (HW emission enabled)
    uint16_t audioEmitterCondition; // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
};

struct Enable_payload {
    std::optional<bool> audioEnabled;
    std::optional<bool> ladEnabled;
    std::optional<bool> lrfEnabled;
    std::optional<bool> lightEnabled;

};
struct StateUpdate {
    //cueing & emission configuration
    std::optional<std::string> systemMode;
    std::optional<uint16_t> lradId;
    std::optional<std::string> cueingStatus;
    std::optional<std::string> configuration;
    std::optional<bool> online;
    std::optional<bool> engaged;
    std::optional<bool> audioEnabled;
    std::optional<bool> ladEnabled;
    std::optional<bool> lrfEnabled;
    std::optional<double_t> inibithionSector1;
    std::optional<double_t> inibithionSector2;
    std::optional<std::string> swVersion;

    // LRAS_CS_lrad_x_status_INS
    // 1=Operative, 2=Degraded, 3=Failure
    std::optional<uint16_t> lradStatus;
    // 1=Stand-by, 2=Manual Search, 3=Cueing, 4=Cueing in blind arc, 5=Video Tracking
    std::optional<uint16_t> lradMode;
    // 0=No Tracking, 1=Tracking, 2=Tracking prediction, 3=Target lost
    std::optional<uint16_t> videoTrackingStatus;
    std::optional<float> azimuth;           // deg [0..360]
    std::optional<float> elevation;         // deg [-90..90]
    std::optional<int16_t> lrfDistance;     // m [-1..4000], -1 = out of range/not valid
    std::optional<bool> withinInhibitionSector;
    // 0=Off, 1=35W, 2=45W, 3=85W
    std::optional<uint16_t> searchlightPower;
    std::optional<uint16_t> searchlightZoom; // [0..99]
    // 0=Off, 1=On, 2=Strobo
    std::optional<uint16_t> laserDazzlerMode;
    std::optional<uint16_t> videoZoom;       // [0..99]
    // 0=Force Internal IMU, 1=Force Ship Gyro, 2=Automatic
    std::optional<uint16_t> gyroSelection;
    // 0=Force Internal IMU, 1=Force Ship Gyro
    std::optional<uint16_t> gyroUsed;

    // --- Lrad_full diagnostic status (from LRAD x full status message) ---
    // 0=OK, 1=Failure
    std::optional<uint16_t> communication;
    // 0=OK, 1=Failure, 2=Overcurrent, 3=Emergency stop
    std::optional<uint16_t> motionAzimuthStatus;
    // 0=OK, 1=Failure, 2=Overcurrent, 3=Emergency stop
    std::optional<uint16_t> motionElevationStatus;

    // Audio Emitter
    // 0=Nominal, 1=Overtemperature
    std::optional<uint16_t> audioEmitterOvertemperature;
    // 0=Nominal, 1=Communication Failure
    std::optional<uint16_t> audioEmitterCommFailure;

    // Searchlight
    // 0=Nominal, 1=Overtemperature
    std::optional<uint16_t> searchlightOvertemperature;
    // 0=Nominal, 1=Communication Failure
    std::optional<uint16_t> searchlightCommFailure;

    // Laser Dazzler
    // 0=Nominal, 1=Overtemperature
    std::optional<uint16_t> laserDazzlerOvertemperature;
    // 0=Nominal, 1=Communication Failure
    std::optional<uint16_t> laserDazzlerCommFailure;

    // LRF
    // 0=Nominal, 1=Overtemperature (not used for LRF per spec)
    std::optional<uint16_t> lrfOvertemperature;
    // 0=Nominal, 1=Communication Failure
    std::optional<uint16_t> lrfCommFailure;

    // 0=Operative, 1=No Communications
    std::optional<uint16_t> trackingBoardStatus;
    // 0=Operative, 1=No Communications
    std::optional<uint16_t> visibleCameraStatus;
    // 0=OK, 1=Not OK
    std::optional<uint16_t> visibleCameraSignal;
    // 0=OK, 1=Absence, 2=Corrupted, 3=Failure
    std::optional<uint16_t> internalImuStatus;
    // 0=OK, 1=Not OK
    std::optional<uint16_t> canBus1;
    // 0=OK, 1=Not OK
    std::optional<uint16_t> canBus2;
    // 0=OK, 1=Not OK
    std::optional<uint16_t> cpuSlaveStatus;
    // 0=OK, 1=Sensor Failure, 2=Over Temperature
    std::optional<uint16_t> electronicBoxTemperature;
    // 0=OK, 1=Sensor Failure, 2=Over Temperature
    std::optional<uint16_t> interfaceBoxTemperature;
    // 0=Operative, 1=No Communications
    std::optional<uint16_t> irCameraStatus;
    // 0=OK, 1=Not OK
    std::optional<uint16_t> irCameraSignal;

    // --- LRAD health (from LRAS_MULTI_health_status_INS) ---
    // 0=Local, 1=Integrated
    std::optional<uint16_t> lradConfiguration;
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> lradCondition;
    // 0=Unknown, 1=Operative, 2=Not Operative
    std::optional<uint16_t> lradOperativeState;
    // 0=FALSE, 1=TRUE (HW emission enabled)
    std::optional<uint16_t> hwEmissionAuth;
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> audioEmitterCondition;

    // Volume Mode
    // 0=no limits, 1=level 1, 2=level 2, 3=level 3
    std::optional<uint16_t> volumeLevel;
    std::optional<float> audioVolumeDb;      // dB [-128..0]
    // 0=Off, 1=On
    std::optional<uint16_t> mute;
    // 0=Off, 1=Tone, 2=Free Voice, 3=Recorded Message, 4=Free Text
    std::optional<uint16_t> audioMode;

    // Recorded Message-Tone
    std::optional<uint32_t> recordedMessageId;
    // 0=Italian, 1=English, 2=Arabic-Egypt, 99=Tone
    std::optional<uint16_t> recordedMessageLanguage;
    // 0=FALSE, 1=TRUE
    std::optional<uint16_t> recordedMessageLoop;

    // Free Text
    std::optional<uint16_t> freeTextLanguageIn;
    std::optional<uint16_t> freeTextLanguageOut;
    std::optional<std::string> freeTextMessage;  // UTF-8, max 768 bytes
    // 0=FALSE, 1=TRUE
    std::optional<uint16_t> freeTextLoop;

    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> searchlightCondition;
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> laserDazzlerCondition;
    // 0=Off, 1=On, 2=Strobo
    std::optional<uint16_t> laserMode;
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> lrfCondition;
    // 0=Off, 1=On
    std::optional<uint16_t> lrfOnOff;
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> cameraCondition;
    std::optional<uint16_t> cameraZoom;          // [0..99]
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> imuCondition;
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> recorderCondition;
    // 0=Stop, 1=On Unlimited, 2=On Time Limit
    std::optional<uint16_t> recorderMode;
    std::optional<uint32_t> recorderElapsedSeconds;
    std::optional<uint32_t> recorderElapsedMicroseconds;
    // 0=Ships Gyro, 1=IMU
    std::optional<uint16_t> horizontalReference;

    // Inhibit Sector 1
    // 0=Off, 1=On
    std::optional<uint16_t> inhibitSector1Active;
    std::optional<float> inhibitSector1Start;    // deg [-180..180]
    std::optional<float> inhibitSector1Stop;     // deg [-180..180]

    // Inhibit Sector 2
    // 0=Off, 1=On
    std::optional<uint16_t> inhibitSector2Active;
    std::optional<float> inhibitSector2Start;    // deg [-180..180]
    std::optional<float> inhibitSector2Stop;     // deg [-180..180]
};

// System-level health fields not tied to a specific LRAD (from LRAS_MULTI_health_status_INS)
struct SystemHealthUpdate {
    // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::optional<uint16_t> lrasCondition;
    // 0=Unknown, 1=Operative, 2=Not Operative
    std::optional<uint16_t> lrasOperativeState;
    // 0=Disable, 1=Enable
    std::optional<uint16_t> laserDazzlerMainAuth;
    // 0=Operative, 1=Degraded, 2=Not Operative
    std::optional<uint16_t> lrasServerStatus;
    // Console 1
    // 0=Operative, 1=Degraded, 2=Not Operative
    std::optional<uint16_t> console1Health;
    // 0=none, 1=LRAD 1, 2=LRAD 2, 3=both
    std::optional<uint16_t> console1ControlledLrad;
    // Console 2
    std::optional<uint16_t> console2Health;
    std::optional<uint16_t> console2ControlledLrad;
};

struct TopicStateUpdateEvent : public IEvent {
    std::string sourceTopic;
    std::vector<StateUpdate> updates;
    const std::string& topic() const override { return sourceTopic; }
};

// Snapshot immutabile dello stato di sistema (copia point-in-time)
struct SystemStateSnapshot {
    std::string systemMode;
    std::map<uint16_t, StateUpdate> lradStates;
    SystemHealthUpdate systemHealth;
    uint64_t timestampMs = 0;
};

class IStateProvider {
public:
    virtual ~IStateProvider() = default;
    virtual SystemStateSnapshot getSnapshot() const = 0;
};

// Classe thread-safe che gestisce lo stato del sistema
class SystemState : public IStateProvider {
public:
    SystemState();

    // Subscribe directly to protocol topics that can carry state updates.
    void subscribeToTopics(const std::shared_ptr<EventBus>& eventBus);

    // --- Snapshot ---
    SystemStateSnapshot getSnapshot() const override;

    // --- Delta apply ---
    void apply(const StateUpdate& update);
    void applyBatch(const std::vector<StateUpdate>& updates);
    void applySystemHealth(const SystemHealthUpdate& health);

    // --- Timestamp ---
    uint64_t getLastUpdatedMs() const;
    
    // Verifica se i dati sono ancora freschi (non scaduti)
    // Restituisce true se il timestamp è più recente di timeoutMs
    bool isDataFresh(uint64_t timeoutMs) const;
    
    // Controlla la salute dei dati: se scaduti, resetta ai valori di default
    // Restituisce true se è stato necessario fare il reset
    bool checkDataHealth(uint64_t timeoutMs);

private:
    void handleTopicStateUpdateEvent(const std::shared_ptr<const IEvent>& event);
    bool applyUnlocked(const StateUpdate& update);
    void touch();  // aggiorna il timestamp
    void resetToDefaults();  // Resetta lo stato ai valori di default

    mutable std::mutex mutex_;
    std::string systemMode_;
    std::map<uint16_t, StateUpdate> lradStates_;
    SystemHealthUpdate systemHealth_;
    uint64_t lastUpdatedMs_ = 0;

    // healt_status fields (from LRAS_MULTI_health_status_INS)
    std::uint16_t LRAS_Condition; // 0=Unknown, 1=Normal, 2=Degraded, 3=Fault
    std::uint16_t LRAS_OperativeState; // 0=Unknown, 1=Operative, 2=Not Operative
    std::uint16_t laserDazzlerMainAuth; // 0=Disable, 1=Enable
};
