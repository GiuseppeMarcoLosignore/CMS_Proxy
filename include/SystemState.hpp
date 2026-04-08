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
struct StateUpdate {
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
    SystemState() = default;

    // Subscribe directly to protocol topics that can carry state updates.
    void subscribeToTopics(const std::shared_ptr<EventBus>& eventBus);

    // --- Snapshot ---
    SystemStateSnapshot getSnapshot() const override;

    // --- Delta apply ---
    void apply(const StateUpdate& update);
    void applyBatch(const std::vector<StateUpdate>& updates);

    // --- Timestamp ---
    uint64_t getLastUpdatedMs() const;

private:
    void handleTopicStateUpdateEvent(const std::shared_ptr<const IEvent>& event);
    bool applyUnlocked(const StateUpdate& update);
    void touch();  // aggiorna il timestamp

    mutable std::mutex mutex_;
    std::string systemMode_;
    std::map<uint16_t, StateUpdate> lradStates_;
    uint64_t lastUpdatedMs_ = 0;
};
