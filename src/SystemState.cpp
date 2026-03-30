#include "SystemState.hpp"
#include <chrono>

static uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void SystemState::touch() {
    lastUpdatedMs_ = nowMs();
}

bool SystemState::applyUnlocked(const StateUpdate& update) {
    bool changed = false;

    if (update.systemMode.has_value()) {
        systemMode_ = *update.systemMode;
        changed = true;
    }

    if (update.lradId.has_value()) {
        StateUpdate& state = lradStates_[*update.lradId];

        if (update.online.has_value()) {
            state.online = *update.online;
            changed = true;
        }
        if (update.engaged.has_value()) {
            state.engaged = *update.engaged;
            changed = true;
        }
        if (update.audioEnabled.has_value()) {
            state.audioEnabled = *update.audioEnabled;
            changed = true;
        }
        if (update.ladEnabled.has_value()) {
            state.ladEnabled = *update.ladEnabled;
            changed = true;
        }
        if (update.lrfEnabled.has_value()) {
            state.lrfEnabled = *update.lrfEnabled;
            changed = true;
        }
        if (update.inibithionSector1.has_value()) {
            state.inibithionSector1 = *update.inibithionSector1;
            changed = true;
        }
        if (update.inibithionSector2.has_value()) {
            state.inibithionSector2 = *update.inibithionSector2;
            changed = true;
        }
        if (update.swVersion.has_value()) {
            state.swVersion = *update.swVersion;
            changed = true;
        }
    }

    return changed;
}

// --- Snapshot ---

SystemStateSnapshot SystemState::getSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SystemStateSnapshot snap;
    snap.systemMode  = systemMode_;
    snap.lradStates  = lradStates_;
    snap.timestampMs = lastUpdatedMs_;
    return snap;
}

void SystemState::apply(const StateUpdate& update) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (applyUnlocked(update)) {
        touch();
    }
}

void SystemState::applyBatch(const std::vector<StateUpdate>& updates) {
    std::lock_guard<std::mutex> lock(mutex_);

    bool changed = false;
    for (const auto& update : updates) {
        changed = applyUnlocked(update) || changed;
    }

    if (changed) {
        touch();
    }
}

// --- Timestamp ---

uint64_t SystemState::getLastUpdatedMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastUpdatedMs_;
}
