#include "SystemState.hpp"

#include "EventBus.hpp"
#include "Topics.hpp"

#include <chrono>

namespace {

StateUpdate makeDefaultLradState(uint16_t lradId) {
    StateUpdate state;
    state.lradId = lradId;
    state.systemMode = "Operative";
    state.cueingStatus = "0";
    state.configuration = "integrated";
    state.online = true;
    state.engaged = false;
    state.audioEnabled = false;
    state.ladEnabled = false;
    state.lrfEnabled = false;
    state.inibithionSector1 = 0.0;
    state.inibithionSector2 = 0.0;

    // LRAS_CS_lrad_x_status_INS defaults
    state.lradStatus = 1;
    state.lradMode = 1;
    state.videoTrackingStatus = 0;
    state.azimuth = 0.0f;
    state.elevation = 0.0f;
    state.lrfDistance = static_cast<int16_t>(-1);
    state.withinInhibitionSector = false;
    state.searchlightPower = 0;
    state.searchlightZoom = 0;
    state.laserDazzlerMode = 0;
    state.videoZoom = 0;
    state.gyroSelection = 2;
    state.gyroUsed = 0;

    // LRAD full diagnostics defaults
    state.communication = 0;
    state.motionAzimuthStatus = 0;
    state.motionElevationStatus = 0;
    state.audioEmitterOvertemperature = 0;
    state.audioEmitterCommFailure = 0;
    state.searchlightOvertemperature = 0;
    state.searchlightCommFailure = 0;
    state.laserDazzlerOvertemperature = 0;
    state.laserDazzlerCommFailure = 0;
    state.lrfOvertemperature = 0;
    state.lrfCommFailure = 0;
    state.trackingBoardStatus = 0;
    state.visibleCameraStatus = 0;
    state.visibleCameraSignal = 0;
    state.internalImuStatus = 0;
    state.canBus1 = 0;
    state.canBus2 = 0;
    state.cpuSlaveStatus = 0;
    state.electronicBoxTemperature = 0;
    state.interfaceBoxTemperature = 0;
    state.irCameraStatus = 0;
    state.irCameraSignal = 0;

    // LRAS_MULTI_health_status_INS per-LRAD defaults
    state.lradConfiguration = 1;
    state.lradCondition = 1;
    state.lradOperativeState = 1;
    state.hwEmissionAuth = 0;
    state.audioEmitterCondition = 1;
    state.volumeLevel = 0;
    state.audioVolumeDb = -128.0f;
    state.mute = 0;
    state.audioMode = 0;
    state.recordedMessageId = 0;
    state.recordedMessageLanguage = 0;
    state.recordedMessageLoop = 0;
    state.freeTextLanguageIn = 0;
    state.freeTextLanguageOut = 0;
    state.freeTextMessage = "";
    state.freeTextLoop = 0;
    state.searchlightCondition = 1;
    state.laserDazzlerCondition = 1;
    state.laserMode = 0;
    state.lrfCondition = 1;
    state.lrfOnOff = 0;
    state.cameraCondition = 1;
    state.cameraZoom = 0;
    state.imuCondition = 1;
    state.recorderCondition = 1;
    state.recorderMode = 0;
    state.recorderElapsedSeconds = 0;
    state.recorderElapsedMicroseconds = 0;
    state.horizontalReference = 0;
    state.inhibitSector1Active = 0;
    state.inhibitSector1Start = 0.0f;
    state.inhibitSector1Stop = 0.0f;
    state.inhibitSector2Active = 0;
    state.inhibitSector2Start = 0.0f;
    state.inhibitSector2Stop = 0.0f;

    return state;
}

SystemHealthUpdate makeDefaultSystemHealth() {
    SystemHealthUpdate health;
    health.lrasCondition = 1;
    health.lrasOperativeState = 1;
    health.laserDazzlerMainAuth = 0;
    health.lrasServerStatus = 0;
    health.console1Health = 0;
    health.console1ControlledLrad = 0;
    health.console2Health = 0;
    health.console2ControlledLrad = 0;
    return health;
}

} // namespace

static uint64_t nowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

SystemState::SystemState() {
    systemMode_ = "Operative";
    lradStates_.emplace(1, makeDefaultLradState(1));
    lradStates_.emplace(2, makeDefaultLradState(2));
    systemHealth_ = makeDefaultSystemHealth();
    touch();
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
    snap.systemHealth = systemHealth_;
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

bool SystemState::isDataFresh(uint64_t timeoutMs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = nowMs();
    
    // Se lastUpdatedMs_ è 0 (mai aggiornato), considerare non-fresh
    if (lastUpdatedMs_ == 0) {
        return false;
    }
    
    // Calcolare il tempo trascorso dall'ultimo aggiornamento
    uint64_t elapsed = now - lastUpdatedMs_;
    
    // Restituire true se non è scaduto (elapsed < timeout)
    return elapsed < timeoutMs;
}

void SystemState::resetToDefaults() {
    // Nota: questa funzione deve essere chiamata con il lock già acquisito
    systemMode_ = "normal";
    lradStates_[1] = makeDefaultLradState(1);
    lradStates_[2] = makeDefaultLradState(2);
    systemHealth_ = makeDefaultSystemHealth();
    touch();
}

bool SystemState::checkDataHealth(uint64_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t now = nowMs();
    
    // Se lastUpdatedMs_ è 0 (mai aggiornato), considerare scaduto
    if (lastUpdatedMs_ == 0) {
        resetToDefaults();
        return true;
    }
    
    // Calcolare il tempo trascorso
    uint64_t elapsed = now - lastUpdatedMs_;
    
    // Se i dati sono ancora freschi, non fare nulla
    if (elapsed < timeoutMs) {
        return false;
    }
    
    // I dati sono scaduti: resetta ai valori di default
    resetToDefaults();
    return true;
}

void SystemState::applySystemHealth(const SystemHealthUpdate& health) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool changed = false;

    if (health.lrasCondition.has_value()) { systemHealth_.lrasCondition = health.lrasCondition; changed = true; }
    if (health.lrasOperativeState.has_value()) { systemHealth_.lrasOperativeState = health.lrasOperativeState; changed = true; }
    if (health.laserDazzlerMainAuth.has_value()) { systemHealth_.laserDazzlerMainAuth = health.laserDazzlerMainAuth; changed = true; }
    if (health.lrasServerStatus.has_value()) { systemHealth_.lrasServerStatus = health.lrasServerStatus; changed = true; }
    if (health.console1Health.has_value()) { systemHealth_.console1Health = health.console1Health; changed = true; }
    if (health.console1ControlledLrad.has_value()) { systemHealth_.console1ControlledLrad = health.console1ControlledLrad; changed = true; }
    if (health.console2Health.has_value()) { systemHealth_.console2Health = health.console2Health; changed = true; }
    if (health.console2ControlledLrad.has_value()) { systemHealth_.console2ControlledLrad = health.console2ControlledLrad; changed = true; }

    if (changed) {
        touch();
    }
}
