#include "Orchestrator.hpp"

#include "AcsEntity.hpp"
#include "EventBus.hpp"
#include "Topics.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <mutex>
#include <nlohmann/json.hpp>

namespace {
bool isKnownLradSender(const std::string& sender) {
    return sender == "LRAD1" || sender == "LRAD2" || sender == "PORT" || sender == "STARBOARD";
}

std::optional<uint32_t> json_u32_value(const nlohmann::json& value) {
    if (value.is_number_unsigned()) {
        return value.get<uint32_t>();
    }

    if (value.is_number_integer()) {
        const auto signedValue = value.get<int64_t>();
        if (signedValue >= 0 && signedValue <= static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
            return static_cast<uint32_t>(signedValue);
        }
    }

    return std::nullopt;
}

std::optional<uint32_t> extract_action_id(const nlohmann::json& payload) {
    if (payload.contains("Action Id")) {
        if (const auto actionId = json_u32_value(payload.at("Action Id")); actionId.has_value()) {
            return actionId;
        }
    }

    if (payload.contains("action_id")) {
        if (const auto actionId = json_u32_value(payload.at("action_id")); actionId.has_value()) {
            return actionId;
        }
    }

    if (payload.contains("meta") && payload.at("meta").is_object() && payload.at("meta").contains("action_id")) {
        if (const auto actionId = json_u32_value(payload.at("meta").at("action_id")); actionId.has_value()) {
            return actionId;
        }
    }

    return std::nullopt;
}

uint32_t source_message_id_from_topic(const std::string& topic) {
    if (topic == "CS_LRAS_video_tracking_command_INS") return 1679949840;
    if (topic == "CS_MULTI_health_status_INS") return 1684229565;
    if (topic == "CS_MULTI_update_cst_kinematics_INS") return 1684229569;
    return 0;
}
}

Orchestrator::Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, std::shared_ptr<EventBus> eventBus)
    : cmsEntity_(cmsEntity),
      acsEntity_(acsEntity),
      eventBus_(std::move(eventBus)) {

        lrasStatus initialLras{};
        std::atomic_store(&lras, std::make_shared<lrasStatus>(std::move(initialLras)));

        std::atomic_store(&lradList_, std::make_shared<std::vector<lradStatus>>());
}

void Orchestrator::start() {
    std::cout << "[Orchestrator] Starting..." << std::endl;

    initializeDefaultStatus();
    
    subscribeTopics();
    
    std::cout << "[Orchestrator] Started" << std::endl;
}

void Orchestrator::initializeDefaultStatus() {
    lrasStatus defaultLras{};
    defaultLras.totalMessagesNumber = 0;
    defaultLras.messageNumber = 0;
    defaultLras.dbItemNumber = 0;
    defaultLras.swVersion = "Unknown";

    lradStatus port{};
    port.alive.name = "PORT";
    port.alive.state = "Unknown";
    port.alive.mode = "Unknown";
    port.alive.ipAddress = "0.0.0.0";
    port.alive.swVersion = "Unknown";
    port.lrad_id = 1;
    port.controlledByCms = false;
    port.cueingActive = false;
    port.videotracking = false;
    port.ladEnabled = true;
    port.searchlightEnabled = true;
    port.lrfEnabled = true;
    port.audioEnabled = true;
    port.isRecording = false;
    port.isCmsConnected = false;
    port.audio.gain = 0.0F;
    port.audio.mute = false;
    port.lad.mode = "OFF";
    port.searchlight.mode = "OFF";
    port.searchlight.power = "OFF";
    port.searchlight.focus = "0";
    port.lrf.mode = "OFF";
    port.lrf.value = 0.0F;
    port.zoom.id = "HD";
    port.zoom.value = 0.0F;

    lradStatus starboard = port;
    starboard.alive.name = "STARBOARD";
    starboard.lrad_id = 2;

    std::vector<lradStatus> defaults;
    defaults.push_back(std::move(port));
    defaults.push_back(std::move(starboard));

    {
        std::lock_guard<std::mutex> lradLock(lradMutex_);
        std::atomic_store(&lradList_, std::make_shared<std::vector<lradStatus>>(std::move(defaults)));
    }

    {
        std::lock_guard<std::mutex> lrasLock(lrasMutex_);
        std::atomic_store(&lras, std::make_shared<lrasStatus>(std::move(defaultLras)));
    }
}

void Orchestrator::stop() {
    std::cout << "[Orchestrator] Stopping..." << std::endl;
    
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
    
    std::cout << "[Orchestrator] Stopped" << std::endl;
}

void Orchestrator::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    //new topics
    eventBus_->subscribe(Topics::LRF_ON, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLRFon(lradId); 
    });

    eventBus_->subscribe(Topics::LRF_OFF, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLRFoff(lradId); 
    });


    eventBus_->subscribe(Topics::LRF_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractLRFdata(lradId, message);
    });


    eventBus_->subscribe(Topics::LAD_ON, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLADon(lradId); 
    });

    eventBus_->subscribe(Topics::LAD_OFF, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLADoff(lradId); 
    });

    eventBus_->subscribe(Topics::LAD_STROBE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLADstrobe(lradId); 
    });


    eventBus_->subscribe(Topics::LAD_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractLADdata(lradId, message);
    });

    eventBus_->subscribe(Topics::HD_ZOOM, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleHdZoom(lradId, message.get<uint8_t>());
    });

    eventBus_->subscribe(Topics::ZOOM_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractZOOMdata(lradId, message);
    });


    eventBus_->subscribe(Topics::SEARCHLIGHT_POWER, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightPower(lradId, message.get<uint8_t>());
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_ON, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightOn(lradId);
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_OFF, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightOff(lradId);
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_FOCUS, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightFocus(lradId, message.get<uint8_t>());
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_STROBE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightStrobe(lradId);
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractSEARCHLIGHTdata(lradId, message);
    });

    eventBus_->subscribe(Topics::AUDIO_GAIN, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleAudioGain(lradId, message.get<float>());
    });

    eventBus_->subscribe(Topics::AUDIO_MUTE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleAudioMute(lradId, message.get<bool>());
    });

    eventBus_->subscribe(Topics::CHANGE_REQ, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleChangeRequest(lradId, message.get<std::string>());
    });

    eventBus_->subscribe(Topics::MASTER_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractMASTERdata(lradId, message);
    });

    std::cout << "[Orchestrator] Topics subscribed" << std::endl;
}




void Orchestrator::sendAckForTopic(const std::string& topic, uint16_t nackreason, const nlohmann::json& message) const {
    const uint32_t sourceMessageId = source_message_id_from_topic(topic);
    if (sourceMessageId == 0) {
        std::cerr << "[Orchestrator] source_message_id non disponibile per topic ACK: " << topic << std::endl;
        return;
    }

    const auto actionId = extract_action_id(message);
    if (!actionId.has_value()) {
        std::cerr << "[Orchestrator] Action Id mancante nel messaggio ACK per topic: " << topic << std::endl;
        return;
    }

    const uint16_t ackNackAccepted = (nackreason == 0) ? 1u : 2u;
    cmsEntity_.sendLRAS_CS_ack_INS(*actionId, sourceMessageId, ackNackAccepted, nackreason);
}

bool Orchestrator::isDataUpdated() const {
    // Check if any LRAD or LRAS data has been updated
    // For now, return true to indicate data is available
    std::lock_guard<std::mutex> lradLock(lradMutex_);
    std::lock_guard<std::mutex> lrasLock(lrasMutex_);

    const std::shared_ptr<std::vector<lradStatus>> lradListPtr = std::atomic_load(&lradList_);
    const std::shared_ptr<lrasStatus> lrasPtr = std::atomic_load(&lras);

    const bool hasLrads = lradListPtr && !lradListPtr->empty();
    const bool lrasUpdated = lrasPtr && !lrasPtr->swVersion.empty();

    return hasLrads || lrasUpdated;
}

void Orchestrator::setLradFullStatus(lradStatus status, std::string name_) {
    std::lock_guard<std::mutex> lock(lradMutex_);

    status.alive.name = std::move(name_);

    std::vector<lradStatus> lradList;
    if (const std::shared_ptr<std::vector<lradStatus>> lradListPtr = std::atomic_load(&lradList_); lradListPtr) {
        lradList = *lradListPtr;
    }
    
    // Check if LRAD with same name already exists
    auto it = std::find_if(
        lradList.begin(),
        lradList.end(),
        [&status](const lradStatus& lrad) {
            return lrad.alive.name == status.alive.name;
        }
    );

    if (it != lradList.end()) {
        *it = status;  // Update existing
    } else {
        lradList.push_back(status);  // Add new
    }

    std::atomic_store(&lradList_, std::make_shared<std::vector<lradStatus>>(std::move(lradList)));
}

void Orchestrator::setLrasFullStatus(lrasStatus status) {
    std::lock_guard<std::mutex> lock(lrasMutex_);
    std::atomic_store(&lras, std::make_shared<lrasStatus>(std::move(status)));
}

lradStatus Orchestrator::getLradFullStatus(const std::string& name_) const {
    std::lock_guard<std::mutex> lock(lradMutex_);

    std::vector<lradStatus> lradList;
    if (const std::shared_ptr<std::vector<lradStatus>> lradListPtr = std::atomic_load(&lradList_); lradListPtr) {
        lradList = *lradListPtr;
    }

    auto it = std::find_if(
        lradList.begin(),
        lradList.end(),
        [&name_](const lradStatus& lrad) {
            return lrad.alive.name == name_;
        }
    );

    if (it == lradList.end()) {
        return lradStatus{};
    }

    return *it;
}

lrasStatus Orchestrator::getLrasFullStatus() const {
    std::lock_guard<std::mutex> lock(lrasMutex_);

    if (const std::shared_ptr<lrasStatus> lrasPtr = std::atomic_load(&lras); lrasPtr) {
        return *lrasPtr;
    }

    return lrasStatus{};
}


//TO TEST
void Orchestrator::extractALIVEdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;
    
    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    if(name == "PORT" || name == "LRAD1") {
        auto readStringField = [&param](const char* primaryKey, const char* fallbackKey = nullptr) -> std::string {
            if (param.contains(primaryKey) && param.at(primaryKey).is_string()) {
                return param.at(primaryKey).get<std::string>();
            }

            if (fallbackKey != nullptr && param.contains(fallbackKey) && param.at(fallbackKey).is_string()) {
                return param.at(fallbackKey).get<std::string>();
            }

            return {};
        };

        lradStatus lrad = getLradFullStatus(name);
        lrad.alive.state = readStringField("state");
        lrad.alive.mode = readStringField("mode");
        lrad.alive.ipAddress = readStringField("ipAddress", "ip");

        setLradFullStatus(std::move(lrad), name);

        lrasStatus lrasStatus = getLrasFullStatus();
        lrasStatus.swVersion = readStringField("swVersion");
        setLrasFullStatus(std::move(lrasStatus));
        
    }
    
}

void Orchestrator::extractDIAGNOSTICdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    auto readBoolField = [&param](const char* key) -> bool {
        if (!param.contains(key)) {
            return false;
        }

        const auto& value = param.at(key);
        if (value.is_boolean()) {
            return value.get<bool>();
        }

        if (value.is_number_integer()) {
            return value.get<int>() != 0;
        }

        if (value.is_string()) {
            const std::string strValue = value.get<std::string>();
            return strValue == "true" || strValue == "1";
        }

        return false;
    };

    lradStatus currentLrad = getLradFullStatus(name);
    currentLrad.diagnostic.limitError = readBoolField("limitError");
    currentLrad.diagnostic.lad = readBoolField("lad");
    currentLrad.diagnostic.lrf = readBoolField("lrf");
    currentLrad.diagnostic.dsp = readBoolField("dsp");
    currentLrad.diagnostic.searchlight = readBoolField("searchlight");
    currentLrad.diagnostic.daq = readBoolField("daq");
    currentLrad.diagnostic.psu12 = readBoolField("psu12");
    currentLrad.diagnostic.psu24 = readBoolField("psu24");
    currentLrad.diagnostic.psu48 = readBoolField("psu48");
    currentLrad.diagnostic.tempVbox = readBoolField("tempVbox");
    currentLrad.diagnostic.tempAhd = readBoolField("tempAhd");

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractAUDIOdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    auto readBoolField = [&param](const char* key) -> bool {
        if (!param.contains(key)) {
            return false;
        }

        const auto& value = param.at(key);
        if (value.is_boolean()) {
            return value.get<bool>();
        }

        if (value.is_number_integer()) {
            return value.get<int>() != 0;
        }

        if (value.is_string()) {
            const std::string strValue = value.get<std::string>();
            return strValue == "true" || strValue == "1";
        }

        return false;
    };

    auto readFloatField = [&param](const char* key) -> float {
        if (!param.contains(key)) {
            return 0.0F;
        }

        const auto& value = param.at(key);
        if (value.is_number()) {
            return value.get<float>();
        }

        if (value.is_string()) {
            const std::string strValue = value.get<std::string>();
            if (strValue.empty()) {
                return 0.0F;
            }

            try {
                return std::stof(strValue);
            } catch (...) {
                return 0.0F;
            }
        }

        return 0.0F;
    };

    lradStatus currentLrad = getLradFullStatus(name);
    currentLrad.audio.gain = readFloatField("gain");
    currentLrad.audio.mute = readBoolField("mute");

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractLADdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    lradStatus currentLrad = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        currentLrad.lad.mode = param.at("mode").get<std::string>();
    }

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractSEARCHLIGHTdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    lradStatus currentLrad = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        currentLrad.searchlight.mode = param.at("mode").get<std::string>();
    }

    if (param.contains("power") && param.at("power").is_string()) {
        currentLrad.searchlight.power = param.at("power").get<std::string>();
    }

    if (param.contains("focus") && param.at("focus").is_number_unsigned()) {
        currentLrad.searchlight.focus = std::to_string(param.at("focus").get<uint16_t>());
    }

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractLRFdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    lradStatus currentLrad = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        currentLrad.lrf.mode = param.at("mode").get<std::string>();
    }

    if (param.contains("value") && param.at("value").is_number()) {
        currentLrad.lrf.value = param.at("value").get<float>();
    }

    if (param.contains("value") && param.at("value").is_string()) {
        currentLrad.lrf.value = std::stof(param.at("value").get<std::string>());
    }

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractSHADOWdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");
    if (!param.contains("sectors") || !param.at("sectors").is_array()) {
        return;
    }

    auto readFloatValue = [](const nlohmann::json& value) -> std::optional<float> {
        if (value.is_number()) {
            return value.get<float>();
        }

        if (value.is_string()) {
            const std::string text = value.get<std::string>();
            if (text.empty()) {
                return std::nullopt;
            }

            try {
                return std::stof(text);
            } catch (...) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    };

    lradStatus currentLrad = getLradFullStatus(name);

    for (const auto& sector : param.at("sectors")) {
        if (!sector.is_object()) {
            continue;
        }

        if (!sector.contains("target") || !sector.at("target").is_string()) {
            continue;
        }

        if (!sector.contains("start") || !sector.contains("stop")) {
            continue;
        }

        const std::optional<float> startValue = readFloatValue(sector.at("start"));
        const std::optional<float> stopValue = readFloatValue(sector.at("stop"));
        if (!startValue.has_value() || !stopValue.has_value()) {
            continue;
        }

        std::string target = sector.at("target").get<std::string>();
        std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });

        if (target == "AZ") {
            currentLrad.az1.enabled = true;
            currentLrad.az1.start = std::to_string(*startValue);
            currentLrad.az1.stop = std::to_string(*stopValue);
            continue;
        }

        if (target == "EL") {
            currentLrad.az2.enabled = true;
            currentLrad.az2.start = std::to_string(*startValue);
            currentLrad.az2.stop = std::to_string(*stopValue);
        }
    }

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractZOOMdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");
    if (!param.contains("id") || !param.at("id").is_string() || !param.contains("value")) {
        return;
    }

    std::string id = param.at("id").get<std::string>();
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    auto readZoomValue = [](const nlohmann::json& value) -> std::optional<uint16_t> {
        if (value.is_number_unsigned()) {
            return value.get<uint16_t>();
        }

        if (value.is_number_integer()) {
            const int parsedValue = value.get<int>();
            if (parsedValue < 0) {
                return std::nullopt;
            }
            return static_cast<uint16_t>(parsedValue);
        }

        if (value.is_string()) {
            const std::string text = value.get<std::string>();
            if (text.empty()) {
                return std::nullopt;
            }

            try {
                return static_cast<uint16_t>(std::stoul(text));
            } catch (...) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    };

    const std::optional<uint16_t> zoomValue = readZoomValue(param.at("value"));
    if (!zoomValue.has_value()) {
        return;
    }

    lradStatus currentLrad = getLradFullStatus(name);

    if (id == "HD") {
        currentLrad.zoom.id = "HD";
        currentLrad.zoom.value = static_cast<float>(*zoomValue);
    } else if (id == "TH") {
        currentLrad.zoom.id = "TH";
        currentLrad.zoom.value = static_cast<float>(*zoomValue);
    } else {
        return;
    }

    setLradFullStatus(std::move(currentLrad), name);
}

void Orchestrator::extractPOSITIONdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    auto readAngleValue = [](const nlohmann::json& value) -> std::optional<float> {
        if (value.is_number()) {
            return value.get<float>();
        }

        if (value.is_string()) {
            const std::string text = value.get<std::string>();
            if (text.empty()) {
                return std::nullopt;
            }

            try {
                return std::stof(text);
            } catch (...) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    };

    lradStatus currentLrad = getLradFullStatus(name);

    if (param.contains("az")) {
        const std::optional<float> azValue = readAngleValue(param.at("az"));
        if (azValue.has_value()) {
            currentLrad.position.az = std::to_string(*azValue);
        }
    }

    if (param.contains("el")) {
        const std::optional<float> elValue = readAngleValue(param.at("el"));
        if (elValue.has_value()) {
            currentLrad.position.el = std::to_string(*elValue);
        }
    }

    setLradFullStatus(std::move(currentLrad), name);
}

bool Orchestrator::isAcsConnected() const {
    return true;
}

bool Orchestrator::isCmsConnected() const {
    return true;
}

bool Orchestrator::isLradControlledByCms(int lradId) const {
    const std::string name = (lradId == 1) ? "PORT" : "STARBOARD";
    const lradStatus lrad = getLradFullStatus(name);
    return lrad.controlledByCms;
}

bool Orchestrator::isPayloadEnabled(int lradId, PayoladType type) const {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) {
        return false;
    }

    const lradStatus lrad = getLradFullStatus(name);
    switch (type) {
        case PayoladType::AUDIO:       return lrad.audioEnabled;
        case PayoladType::LAD:         return lrad.ladEnabled;
        case PayoladType::SEARCHLIGHT: return lrad.searchlightEnabled;
        case PayoladType::LRF:         return lrad.lrfEnabled;
    }

    return false;
}

bool Orchestrator::canLadFire(int lradId) const {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) {
        return false;
    }

    const lradStatus lrad = getLradFullStatus(name);
    return lrad.lrf.mode == "ON" && lrad.lrf.value > lrad.ladMinDistance;
}

bool Orchestrator::inInShadow(int lradId) const {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) {
        return false;
    }

    const lradStatus lrad = getLradFullStatus(name);

    auto parseFloat = [](const std::string& text) -> std::optional<float> {
        if (text.empty()) {
            return std::nullopt;
        }

        try {
            return std::stof(text);
        } catch (...) {
            return std::nullopt;
        }
    };

    const bool azSectorActive = lrad.az1.enabled;
    const bool elSectorActive = lrad.az2.enabled;

    if (!azSectorActive && !elSectorActive) {
        return false;
    }

    bool azInside = false;
    if (azSectorActive) {
        const std::optional<float> currentAz = parseFloat(lrad.position.az);
        const std::optional<float> azStart = parseFloat(lrad.az1.start);
        const std::optional<float> azStop = parseFloat(lrad.az1.stop);
        if (currentAz.has_value() && azStart.has_value() && azStop.has_value()) {
            const float minAz = std::min(*azStart, *azStop);
            const float maxAz = std::max(*azStart, *azStop);
            azInside = *currentAz >= minAz && *currentAz <= maxAz;
        }
    }

    bool elInside = false;
    if (elSectorActive) {
        const std::optional<float> currentEl = parseFloat(lrad.position.el);
        const std::optional<float> elStart = parseFloat(lrad.az2.start);
        const std::optional<float> elStop = parseFloat(lrad.az2.stop);
        if (currentEl.has_value() && elStart.has_value() && elStop.has_value()) {
            const float minEl = std::min(*elStart, *elStop);
            const float maxEl = std::max(*elStart, *elStop);
            elInside = *currentEl >= minEl && *currentEl <= maxEl;
        }
    }

    return azInside || elInside;
}

void Orchestrator::enablePayload(PayoladType /*type*/, std::string /*enable*/) {
    // TODO
}


void Orchestrator::extractMASTERdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;
    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }
    const auto& param = payload.at("param");
    lradStatus lrad = getLradFullStatus(name);
    if (param.contains("mode") && param.at("mode").is_string()) {
        const std::string mode = param.at("mode").get<std::string>();
        if(mode == "ACCEPT") lrad.controlledByCms = true;
        if(mode == "REFUSE") lrad.controlledByCms = false;
        if(mode == "REQUEST") cmsEntity_.sendControlReq(lradId);

    }
    setLradFullStatus(std::move(lrad), name);
}

void Orchestrator::handleLRFon(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LRF command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_ON, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LRF)) { //trial
        std::cout << "[Orchestrator] Cannot handle LRF command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_ON, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::LRF_ON, StatusEventValue::NO_ERR);
    acsEntity_.turnLRFon(destinationLradId);

    
}

void Orchestrator::handleLRFoff(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LRF command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_OFF, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LRF)) { //trial
        std::cout << "[Orchestrator] Cannot handle LRF command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_OFF, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::LRF_OFF, StatusEventValue::NO_ERR);
    acsEntity_.turnLRFoff(destinationLradId);

}


void Orchestrator::handleSearchlightOn(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ON, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ON, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ON, StatusEventValue::NO_ERR);
    acsEntity_.turnSearchlightOn(destinationLradId);
}


void Orchestrator::handleSearchlightOff(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight off command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_OFF, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight off command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_OFF, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::SEARCHLIGHT_OFF, StatusEventValue::NO_ERR);
    acsEntity_.turnSearchlightOff(destinationLradId);
}

void Orchestrator::handleSearchlightStrobe(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight strobe command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_STROBE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight strobe command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_STROBE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::SEARCHLIGHT_STROBE, StatusEventValue::NO_ERR);
    acsEntity_.turnSearchlightStrobe(destinationLradId);
}


void Orchestrator::handleLADstrobe(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LAD strobe command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_STROBE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LAD)) { //trial
        std::cout << "[Orchestrator] Cannot handle LAD strobe command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_STROBE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::LAD_STROBE, StatusEventValue::NO_ERR);
    acsEntity_.turnLADstrobe(destinationLradId);
}

void Orchestrator::handleLADon(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LAD on command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_ON, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LAD)) { //trial
        std::cout << "[Orchestrator] Cannot handle LAD on command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_ON, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!canLadFire(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LAD on command: LRF conditions not met" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_ON, StatusEventValue::SYSTEM_ERR);
        return;
    }

    cmsEntity_.eventStatus(Topics::LAD_ON, StatusEventValue::NO_ERR);
    acsEntity_.turnLADon(destinationLradId);
}

void Orchestrator::handleLADoff(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LAD off command: ACS not connected" << std::endl;
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LAD)) { //trial
        std::cout << "[Orchestrator] Cannot handle LAD off command: Payload not enabled" << std::endl;
        return;
    }
    if(!canLadFire(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LAD off command: LRF conditions not met" << std::endl;
        return;
    }

    cmsEntity_.eventStatus(Topics::LAD_OFF, StatusEventValue::NO_ERR);
    acsEntity_.turnLADoff(destinationLradId);
}


void Orchestrator::handleAudioGain(int destinationLradId, float gain) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Audio gain command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_GAIN, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::AUDIO)) { //trial
        std::cout << "[Orchestrator] Cannot handle Audio gain command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_GAIN, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::AUDIO_GAIN, StatusEventValue::NO_ERR);
    acsEntity_.setGain(destinationLradId, gain);
}

void Orchestrator::handleAudioMute(int destinationLradId, bool mute) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Audio mute command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_MUTE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::AUDIO)) { //trial
        std::cout << "[Orchestrator] Cannot handle Audio mute command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_MUTE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::AUDIO_MUTE, StatusEventValue::NO_ERR);
    acsEntity_.setMute(destinationLradId, mute);
}

void Orchestrator::handleSearchlightFocus(int destinationLradId, float focus) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight focus command: ACS not connected" << std::endl;
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight focus command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_FOCUS, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::SEARCHLIGHT_FOCUS, StatusEventValue::NO_ERR);
    acsEntity_.setSearchlightFocus(destinationLradId, focus);
}

void Orchestrator::handleSearchlightPower(int destinationLradId, const uint8_t  & power) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight power command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_POWER, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight power command: Payload not enabled" << std::endl;
        return;
    }
    cmsEntity_.eventStatus(Topics::SEARCHLIGHT_POWER, StatusEventValue::NO_ERR);
    acsEntity_.setSearchlightPower(destinationLradId, power);
}

void Orchestrator::handleHdZoom(int destinationLradId, const uint8_t zoomValue) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle HD zoom command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::HD_ZOOM, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!(zoomValue > 0 && zoomValue <= 100)) { //trial
        std::cout << "[Orchestrator] Cannot handle HD zoom command: Invalid zoom value" << std::endl;
        cmsEntity_.eventStatus(Topics::HD_ZOOM, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::HD_ZOOM, StatusEventValue::NO_ERR);
    acsEntity_.setHdZoom(destinationLradId, zoomValue);
}

void Orchestrator::handleThZoom(int destinationLradId, const uint8_t zoomValue) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle TH zoom command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::TH_ZOOM, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!(zoomValue > 0 && zoomValue <= 100)) { //trial
        std::cout << "[Orchestrator] Cannot handle TH zoom command: Invalid zoom value" << std::endl;
        cmsEntity_.eventStatus(Topics::TH_ZOOM, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cmsEntity_.eventStatus(Topics::TH_ZOOM, StatusEventValue::NO_ERR);
    acsEntity_.setThZoom(destinationLradId, zoomValue);
}

void Orchestrator::handleChangeRequest(int destinationLradId, const std::string& mode) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle change request command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::CHANGE_REQ, StatusEventValue::NETWORK_ERR);
        return;
    }
    std::string resolvedMode = mode;
    lradStatus lrad = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if (mode == "REQ" && isLradControlledByCms(destinationLradId)) {
        resolvedMode = "REFUSE";
    }
    if(mode == "RELEASE") {

        lrad.controlledByCms = false;
        setLradFullStatus(std::move(lrad), (destinationLradId == 1) ? "PORT" : "STARBOARD");
        if(isLradControlledByCms(destinationLradId)) resolvedMode = "ACCEPT";

    }

    
    cmsEntity_.eventStatus(Topics::CHANGE_REQ, StatusEventValue::NO_ERR);
    acsEntity_.setChangeRequest(destinationLradId, resolvedMode);
}


void Orchestrator::start_cueing() {
    std::cout << "[Orchestrator] start_cueing: TODO" << std::endl;
}

void Orchestrator::stop_cueing() {
    std::cout << "[Orchestrator] stop_cueing: TODO" << std::endl;
}

void Orchestrator::manage_recording(nlohmann::json /*message*/) {
    std::cout << "[Orchestrator] manage_recording: TODO" << std::endl;
}


