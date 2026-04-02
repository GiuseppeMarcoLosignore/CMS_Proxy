#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

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

    // --- Snapshot ---
    SystemStateSnapshot getSnapshot() const override;

    // --- Delta apply ---
    void apply(const StateUpdate& update);
    void applyBatch(const std::vector<StateUpdate>& updates);

    // --- Timestamp ---
    uint64_t getLastUpdatedMs() const;

private:
    bool applyUnlocked(const StateUpdate& update);
    void touch();  // aggiorna il timestamp

    mutable std::mutex mutex_;
    std::string systemMode_;
    std::map<uint16_t, StateUpdate> lradStates_;
    uint64_t lastUpdatedMs_ = 0;
};
