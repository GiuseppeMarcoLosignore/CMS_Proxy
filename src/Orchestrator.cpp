#include "Orchestrator.hpp"

#include "AcsEntity.hpp"
#include "EventBus.hpp"
#include "Topics.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <mutex>

namespace {
bool isKnownLradSender(const std::string& sender) {
    return sender == "LRAD1" || sender == "LRAD2" || sender == "PORT" || sender == "STARBOARD";
}
}

Orchestrator::Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, NavsEntity &navsEntity, std::shared_ptr<EventBus> eventBus)
    : cmsEntity_(cmsEntity),
      acsEntity_(acsEntity),
      navsEntity_(navsEntity),
      eventBus_(std::move(eventBus)) {

        Lras_full initialLras{};
        initialLras.lras_status = 0;
        initialLras.lras_mode = 0;
        std::atomic_store(&lras, std::make_shared<Lras_full>(std::move(initialLras)));

        std::atomic_store(&lradList_, std::make_shared<std::vector<Lrad_full>>());
}

void Orchestrator::start() {
    std::cout << "[Orchestrator] Starting..." << std::endl;
    
    subscribeTopics();
    
    std::cout << "[Orchestrator] Started" << std::endl;
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

    // CMS topics
    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_CS_ack_INS(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_cueing_order_cancellation_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_CS_ack_INS(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_emission_mode_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_CS_ack_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_CS_lrad_1_status_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_CS_lrad_1_status_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_CS_lrad_2_status_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_CS_lrad_2_status_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_MULTI_full_status_v2_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_MULTI_full_status_v2_INS(event);
    });

    eventBus_->subscribe(Topics::LRAS_MULTI_health_status_INS, [this](const EventBus::EventPtr& event) {
        cmsEntity_.sendLRAS_MULTI_health_status_INS(event);
    });



    //ACS topics
    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const EventBus::EventPtr& event) {
        acsEntity_.createMASTER(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_emission_control_INS, [this](const EventBus::EventPtr& event) {
        acsEntity_.createSEARCHLIGHT(event);
        acsEntity_.createAUDIO(event);
        acsEntity_.createLAD(event);
        acsEntity_.createLRF(event);
        acsEntity_.createZOOM(event);
        acsEntity_.createLRF(event);
    });   

    eventBus_->subscribe(Topics::CS_LRAS_inhibition_sectors_INS, [this](const EventBus::EventPtr& event) {
        acsEntity_.createSHADOW(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_joystick_control_lrad_1_INS, [this](const EventBus::EventPtr& event) {
        acsEntity_.createDELTA(event);
    });

    eventBus_->subscribe(Topics::CS_LRAS_joystick_control_lrad_2_INS, [this](const EventBus::EventPtr& event) {
        acsEntity_.createDELTA(event);
    });

    
    // From ACS to Orchestrator topics
    eventBus_->subscribe(Topics::AcsAlive, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractALIVEdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsDiagnostic, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractDIAGNOSTICdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsAudio, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractAUDIOdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsLad, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractLADdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsSearchlight, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractSEARCHLIGHTdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsLrf, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractLRFdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsShadow, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractSHADOWdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsZoom, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractZOOMdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsMaster, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractMASTERdata(acsEvent->payload);
        }
    });

    eventBus_->subscribe(Topics::AcsPosition, [this](const EventBus::EventPtr& event) {
        const auto* acsEvent = dynamic_cast<const AcsOutgoingJsonEvent*>(event.get());
        if (acsEvent) {
            extractPOSITIONdata(acsEvent->payload);
        }
    });


    std::cout << "[Orchestrator] Topics subscribed" << std::endl;
}

bool Orchestrator::isDataUpdated() const {
    // Check if any LRAD or LRAS data has been updated
    // For now, return true to indicate data is available
    std::lock_guard<std::mutex> lradLock(lradMutex_);
    std::lock_guard<std::mutex> lrasLock(lrasMutex_);

    const std::shared_ptr<std::vector<Lrad_full>> lradListPtr = std::atomic_load(&lradList_);
    const std::shared_ptr<Lras_full> lrasPtr = std::atomic_load(&lras);

    const bool hasLrads = lradListPtr && !lradListPtr->empty();
    const bool lrasUpdated = lrasPtr && lrasPtr->lras_status != 0;

    return hasLrads || lrasUpdated;
}

void Orchestrator::setLradFullStatus(Lrad_full status, std::string name_) {
    std::lock_guard<std::mutex> lock(lradMutex_);

    status.name = std::move(name_);

    std::vector<Lrad_full> lradList;
    if (const std::shared_ptr<std::vector<Lrad_full>> lradListPtr = std::atomic_load(&lradList_); lradListPtr) {
        lradList = *lradListPtr;
    }
    
    // Check if LRAD with same name already exists
    auto it = std::find_if(
        lradList.begin(),
        lradList.end(),
        [&status](const Lrad_full& lrad) {
            return lrad.name == status.name;
        }
    );

    if (it != lradList.end()) {
        *it = status;  // Update existing
    } else {
        lradList.push_back(status);  // Add new
    }

    std::atomic_store(&lradList_, std::make_shared<std::vector<Lrad_full>>(std::move(lradList)));
}

void Orchestrator::setLrasFullStatus(Lras_full status) {
    std::lock_guard<std::mutex> lock(lrasMutex_);
    std::atomic_store(&lras, std::make_shared<Lras_full>(std::move(status)));
}

Lrad_full Orchestrator::getLradFullStatus(const std::string& name_) const {
    std::lock_guard<std::mutex> lock(lradMutex_);

    std::vector<Lrad_full> lradList;
    if (const std::shared_ptr<std::vector<Lrad_full>> lradListPtr = std::atomic_load(&lradList_); lradListPtr) {
        lradList = *lradListPtr;
    }

    auto it = std::find_if(
        lradList.begin(),
        lradList.end(),
        [&name_](const Lrad_full& lrad) {
            return lrad.name == name_;
        }
    );

    if (it == lradList.end()) {
        return Lrad_full{};
    }

    return *it;
}

Lras_full Orchestrator::getLrasFullStatus() const {
    std::lock_guard<std::mutex> lock(lrasMutex_);

    if (const std::shared_ptr<Lras_full> lrasPtr = std::atomic_load(&lras); lrasPtr) {
        return *lrasPtr;
    }

    return Lras_full{};
}


//TO TEST
void Orchestrator::extractALIVEdata(const nlohmann::json& payload) {
    
    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    if(!param.contains("name")) {
        return;
    }

    const std::string name = param.at("name").get<std::string>();
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

        Lrad_full lrad = getLradFullStatus(name);
        lrad.state = readStringField("state");
        lrad.mode = readStringField("mode");
        lrad.ipAddress = readStringField("ipAddress", "ip");

        setLradFullStatus(std::move(lrad), name);

        Lras_full lrasStatus = getLrasFullStatus();
        lrasStatus.swVersion = readStringField("swVersion");
        setLrasFullStatus(std::move(lrasStatus));
        
    }
    
    if(name == "STARBOARD" || name == "LRAD2") {
        auto readStringField = [&param](const char* primaryKey, const char* fallbackKey = nullptr) -> std::string {
            if (param.contains(primaryKey) && param.at(primaryKey).is_string()) {
                return param.at(primaryKey).get<std::string>();
            }

            if (fallbackKey != nullptr && param.contains(fallbackKey) && param.at(fallbackKey).is_string()) {
                return param.at(fallbackKey).get<std::string>();
            }

            return {};
        };

        Lrad_full lrad = getLradFullStatus(name);
        lrad.state = readStringField("state");
        lrad.mode = readStringField("mode");
        lrad.ipAddress = readStringField("ipAddress", "ip");

        setLradFullStatus(std::move(lrad), name);

        const Lrad_full portLrad = getLradFullStatus("PORT");
        if (!portLrad.name.empty() && portLrad.mode != "Unknown") {
            Lras_full lrasStatus = getLrasFullStatus();
            lrasStatus.swVersion = readStringField("swVersion");
            setLrasFullStatus(std::move(lrasStatus));
        }
    }


}

void Orchestrator::extractDIAGNOSTICdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
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

    Lrad_full lradStatus = getLradFullStatus(name);
    lradStatus.limitError = readBoolField("limitError");
    lradStatus.lad = readBoolField("lad");
    lradStatus.lrf = readBoolField("lrf");
    lradStatus.dsp = readBoolField("dsp");
    lradStatus.searchlight = readBoolField("searchlight");
    lradStatus.daq = readBoolField("daq");
    lradStatus.psu12 = readBoolField("psu12");
    lradStatus.psu24 = readBoolField("psu24");
    lradStatus.psu48 = readBoolField("psu48");
    lradStatus.tempVbox = readBoolField("tempVbox");
    lradStatus.tempAhd = readBoolField("tempAhd");

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractAUDIOdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
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

    Lrad_full lradStatus = getLradFullStatus(name);
    lradStatus.gain = readFloatField("gain");
    lradStatus.mute = readBoolField("mute");

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractLADdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    Lrad_full lradStatus = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        lradStatus.laser_dazzler_mode = param.at("mode").get<std::string>() == "ON" ? 1 : 0;
    }

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractSEARCHLIGHTdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    Lrad_full lradStatus = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        lradStatus.searchlight_mode = param.at("mode").get<std::string>() == "ON" ? 1 : 0;
    }

    if (param.contains("power") && param.at("power").is_string()) {
        lradStatus.searchlight_power_level = static_cast<uint16_t>(std::stoul(param.at("power").get<std::string>()));
    }

    if (param.contains("focus") && param.at("focus").is_number_unsigned()) {
        lradStatus.searchlight_focus = param.at("focus").get<uint16_t>();
    }

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractLRFdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
        return;
    }

    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    Lrad_full lradStatus = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        lradStatus.lrf_on = param.at("mode").get<std::string>() == "ON";
    }

    if (param.contains("value") && param.at("value").is_string()) {
        lradStatus.lrf_value = std::stof(param.at("value").get<std::string>());
    }

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractSHADOWdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
        return;
    }

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

    Lrad_full lradStatus = getLradFullStatus(name);

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
            lradStatus.AzShadowStart = *startValue;
            lradStatus.AzShadowEnd = *stopValue;
            continue;
        }

        if (target == "EL") {
            lradStatus.ElShadowStart = *startValue;
            lradStatus.ElShadowEnd = *stopValue;
        }
    }

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractZOOMdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
        return;
    }

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

    Lrad_full lradStatus = getLradFullStatus(name);

    if (id == "HD") {
        lradStatus.hd_camera_zoom_level = *zoomValue;
    } else if (id == "TH") {
        lradStatus.th_camera_zoom_level = *zoomValue;
    } else {
        return;
    }

    setLradFullStatus(std::move(lradStatus), name);
}

void Orchestrator::extractPOSITIONdata(const nlohmann::json& payload) {
    if (!payload.contains("sender") || !payload.at("sender").is_string()) {
        return;
    }

    const std::string name = payload.at("sender").get<std::string>();
    if (!isKnownLradSender(name)) {
        return;
    }

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

    Lrad_full lradStatus = getLradFullStatus(name);

    if (param.contains("az")) {
        const std::optional<float> azValue = readAngleValue(param.at("az"));
        if (azValue.has_value()) {
            lradStatus.Azimuth_deg = *azValue;
        }
    }

    if (param.contains("el")) {
        const std::optional<float> elValue = readAngleValue(param.at("el"));
        if (elValue.has_value()) {
            lradStatus.Elevation_deg = *elValue;
        }
    }

    setLradFullStatus(std::move(lradStatus), name);
}
