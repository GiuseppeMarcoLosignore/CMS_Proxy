#include "SystemState.hpp"

#include "EventBus.hpp"
#include "Topics.hpp"

#include <chrono>

static uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void SystemState::subscribeToTopics(const std::shared_ptr<EventBus>& eventBus) {
    if (!eventBus) {
        return;
    }

    static const std::vector<std::string> stateTopics = {
        Topics::CS_LRAS_change_configuration_order_INS,
        Topics::CS_LRAS_cueing_order_cancellation_INS,
        Topics::CS_LRAS_cueing_order_INS,
        Topics::CS_LRAS_emission_control_INS,
        Topics::CS_LRAS_emission_mode_INS,
        Topics::CS_LRAS_inhibition_sectors_INS,
        Topics::CS_LRAS_joystick_control_lrad_1_INS,
        Topics::CS_LRAS_joystick_control_lrad_2_INS,
        Topics::CS_LRAS_recording_command_INS,
        Topics::CS_LRAS_request_emission_mode_INS,
        Topics::CS_LRAS_request_engagement_capability_INS,
        Topics::CS_LRAS_request_full_status_INS,
        Topics::CS_LRAS_request_installation_data_INS,
        Topics::CS_LRAS_request_message_table_INS,
        Topics::CS_LRAS_request_software_version_INS,
        Topics::CS_LRAS_request_thresholds_INS,
        Topics::CS_LRAS_request_translation_INS,
        Topics::CS_LRAS_video_tracking_command_INS,
        Topics::CS_MULTI_health_status_INS,
        Topics::CS_MULTI_update_cst_kinematics_INS
    };

    for (const auto& topic : stateTopics) {
        eventBus->subscribe(topic, [this](const EventBus::EventPtr& event) {
            handleTopicStateUpdateEvent(event);
        });
    }
}

void SystemState::handleTopicStateUpdateEvent(const std::shared_ptr<const IEvent>& event) {
    const auto stateEvent = std::dynamic_pointer_cast<const TopicStateUpdateEvent>(event);
    if (!stateEvent || stateEvent->updates.empty()) {
        return;
    }

    applyBatch(stateEvent->updates);
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

        state.lradId = *update.lradId;

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
        if (update.cueingStatus.has_value()) {
            state.cueingStatus = *update.cueingStatus;
            changed = true;
        }
        if (update.configuration.has_value()) {
            state.configuration = *update.configuration;
            changed = true;
        }
        if (update.lradStatus.has_value()) {
            state.lradStatus = *update.lradStatus;
            changed = true;
        }
        if (update.lradMode.has_value()) {
            state.lradMode = *update.lradMode;
            changed = true;
        }
        if (update.videoTrackingStatus.has_value()) {
            state.videoTrackingStatus = *update.videoTrackingStatus;
            changed = true;
        }
        if (update.azimuth.has_value()) {
            state.azimuth = *update.azimuth;
            changed = true;
        }
        if (update.elevation.has_value()) {
            state.elevation = *update.elevation;
            changed = true;
        }
        if (update.lrfDistance.has_value()) {
            state.lrfDistance = *update.lrfDistance;
            changed = true;
        }
        if (update.withinInhibitionSector.has_value()) {
            state.withinInhibitionSector = *update.withinInhibitionSector;
            changed = true;
        }
        if (update.searchlightPower.has_value()) {
            state.searchlightPower = *update.searchlightPower;
            changed = true;
        }
        if (update.searchlightZoom.has_value()) {
            state.searchlightZoom = *update.searchlightZoom;
            changed = true;
        }
        if (update.laserDazzlerMode.has_value()) {
            state.laserDazzlerMode = *update.laserDazzlerMode;
            changed = true;
        }
        if (update.videoZoom.has_value()) {
            state.videoZoom = *update.videoZoom;
            changed = true;
        }
        if (update.gyroSelection.has_value()) {
            state.gyroSelection = *update.gyroSelection;
            changed = true;
        }
        if (update.gyroUsed.has_value()) {
            state.gyroUsed = *update.gyroUsed;
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
