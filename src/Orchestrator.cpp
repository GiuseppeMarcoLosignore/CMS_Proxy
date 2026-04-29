#include "Orchestrator.hpp"

#include "AcsEntity.hpp"
#include "EventBus.hpp"
#include "Topics.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>

namespace {
bool isKnownLradSender(const std::string& sender) {
    return sender == "LRAD1" || sender == "LRAD2" || sender == "PORT" || sender == "STARBOARD";
}
}

Orchestrator::Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, std::shared_ptr<EventBus> eventBus)
    : cmsEntity_(cmsEntity),
      acsEntity_(acsEntity),
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


    //ACS topics
    eventBus_->subscribe(Topics::CS_LRAS_change_configuration_order_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_change_configuration_order_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_cueing_order_cancellation_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_cueing_order_cancellation_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_cueing_order_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_cueing_order_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_emission_control_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_emission_control_INS(message);
    });  
    
    eventBus_->subscribe(Topics::CS_LRAS_emission_mode_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_emission_mode_INS(message);
    }); 

    eventBus_->subscribe(Topics::CS_LRAS_inhibition_sectors_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_inhibition_sectors_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_joystick_control_lrad_1_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_joystick_control_lrad_1_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_joystick_control_lrad_2_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_joystick_control_lrad_2_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_recording_command_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_recording_command_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_emission_mode_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_emission_mode_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_engagement_capability_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_engagement_capability_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_full_status_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_full_status_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_installation_data_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_installation_data_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_software_version_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_software_version_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_message_table_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_message_table_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_thresholds_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_thresholds_INS(message);
    });

    eventBus_->subscribe(Topics::CS_LRAS_request_translation_INS, [this](const std::string& topic, const nlohmann::json& message) {
        handleCS_LRAS_request_translation_INS(message);
    });


    
    // From ACS to Orchestrator topics
    eventBus_->subscribe(Topics::AcsAlive, [this](const std::string& topic, const nlohmann::json& message) {
        extractALIVEdata(message);
    });

    eventBus_->subscribe(Topics::AcsDiagnostic, [this](const std::string& topic, const nlohmann::json& message) {
        extractDIAGNOSTICdata(message);
    });

    eventBus_->subscribe(Topics::AcsAudio, [this](const std::string& topic, const nlohmann::json& message) {
        extractAUDIOdata(message);
    });

    eventBus_->subscribe(Topics::AcsLad, [this](const std::string& topic, const nlohmann::json& message) {
        extractLADdata(message);
    });

    eventBus_->subscribe(Topics::AcsSearchlight, [this](const std::string& topic, const nlohmann::json& message) {
        extractSEARCHLIGHTdata(message);
    });

    eventBus_->subscribe(Topics::AcsLrf, [this](const std::string& topic, const nlohmann::json& message) {
        extractLRFdata(message);
    });

    eventBus_->subscribe(Topics::AcsShadow, [this](const std::string& topic, const nlohmann::json& message) {
        extractSHADOWdata(message);
    });

    eventBus_->subscribe(Topics::AcsZoom, [this](const std::string& topic, const nlohmann::json& message) {
        extractZOOMdata(message);
    });

    eventBus_->subscribe(Topics::AcsMaster, [this](const std::string& topic, const nlohmann::json& message) {
        extractMASTERdata(message);
    });

    eventBus_->subscribe(Topics::AcsPosition, [this](const std::string& topic, const nlohmann::json& message) {
        extractPOSITIONdata(message);
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


void Orchestrator::handleCS_LRAS_change_configuration_order_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_change_configuration_order_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_change_configuration_order_INS, nackreason, message);
        return;
    }
    if(message.contains("LRAD ID") && message.contains("Configuration")) {
        const uint16_t lradId = message.at("LRAD ID").get<uint16_t>();
        const uint16_t rawConfig = message.at("Configuration").get<uint16_t>();

        if(!isLradControlledByCms(lradId) && rawConfig == 1) {
            acsEntity_.createMASTER(lradId, "REQ");
        }
        if(isLradControlledByCms(lradId) && rawConfig == 1) {
            acsEntity_.createMASTER(lradId, "REFUSE");
        }
        if(isLradControlledByCms(lradId) && rawConfig == 0) {
            acsEntity_.createMASTER(lradId, "RELEASE");
            acsEntity_.createMASTER(lradId, "ACCEPT");
        }

        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_change_configuration_order_INS, nackreason, message);
        // Process the configuration change as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_change_configuration_order_INS, nackreason, message);
    }
}

void Orchestrator::handleCS_LRAS_cueing_order_cancellation_INS(const nlohmann::json& message) {

    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_cueing_order_cancellation_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_cueing_order_cancellation_INS, nackreason, message);
        return;
    }
    stop_cueing();
    cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_cueing_order_cancellation_INS, nackreason, message);
    // Process the cueing order cancellation as needed
}

void Orchestrator::handleCS_LRAS_cueing_order_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_cueing_order_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_cueing_order_INS, nackreason, message);
        return;
    }
    start_cueing();
    cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_cueing_order_INS, nackreason, message);
    // Process the cueing order as needed
}

void Orchestrator::handleCS_LRAS_emission_control_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_emission_control_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        return;
    }

    if(message.contains("LRAD ID")) {
        const uint16_t lradId = message.at("LRAD ID").get<uint16_t>();

        if(isLradControlledByCms(lradId)) {
            if(isPayloadEnabled(PayoladType::LAD) && message["Laser Enable Validity"] == 1) {
                acsEntity_.createLAD(lradId, message["Laser on off"] == 0 ? "OFF" : "ON", false);

            }
            if(isPayloadEnabled(PayoladType::SEARCHLIGHT) && message["Light Enable Validity"] == 1) {
                acsEntity_.createSEARCHLIGHT(
                    lradId, 
                    message["Light Power"] == 0 ? "OFF" :  message["Light Power"] == 1 ? "ON" : "STROBE",
                    message["Light Zoom"], 
                    message["Light Mode"] == 1 ? "35W" : message["Light Mode"] == 2 ? "45W" : "85W"
                    );
            }
            if(isPayloadEnabled(PayoladType::LRF) && message["Laser Range Finder Enable Validity"] == 1) {
                acsEntity_.createLRF(lradId,
                    message["LRF on off"] == 0 ? "OFF" :  "ON");
            }
            if(isPayloadEnabled(PayoladType::AUDIO) && message["Audio Enable Validity"] == 1) {
                acsEntity_.createAUDIO(lradId, message["Audio Volume dB"], message["Mute"]);
            }

        } else {
            nackreason = 2; 
            cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
            return;
        }

        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        // Process the configuration change as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
    }
}

void Orchestrator::handleCS_LRAS_emission_mode_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_emission_mode_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        return;
    }

    if(message.contains("LRAD ID")) {
        const uint16_t lradId = message.at("LRAD ID").get<uint16_t>();

        if(isLradControlledByCms(lradId)) {
            if(message["Laser Enable Validity"] == 1) {
                lras->ladEnabled = message["Laser on off"] == 0 ? false : true;
            }
            if(message["Light Enable Validity"] == 1) {
                lras->searchlightEnabled = message["Light on off"] == 0 ? false : true;
            }
            if(message["Laser Range Finder Enable Validity"] == 1) {
                lras->lrfEnabled = message["LRF on off"] == 0 ? false : true;
            }
            if(message["Audio Enable Validity"] == 1) {
                lras->audioEnabled = message["Audio on off"] == 0 ? false : true;
            }
            if(message["Audio Volume Levels Validity"] == 1) {
                lras->audioLvl1 = message["Audio Volume Level 1"];
                lras->audioLvl2 = message["Audio Volume Level 2"];
                lras->audioLvl3 = message["Audio Volume Level 3"];
            }

        } else {
            nackreason = 2; 
            cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
            return;
        }

        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        // Process the configuration change as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_emission_control_INS, nackreason, message);
    }
}

void Orchestrator::handleCS_LRAS_inhibition_sectors_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    float az1 = 0;
    float el1 = 0;
    float az2 = 0;
    float el2 = 0;
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_inhibition_sectors_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_inhibition_sectors_INS, nackreason, message);
        return;
    }
    if(message.contains("LRAD ID") ) {
        const uint16_t lradId = message.at("LRAD ID").get<uint16_t>();
        if(message.at("Sector 1")["On Off"] == 1) {
            az1 = message.at("Sector 1")["start"].get<float>();
            el1 = message.at("Sector 1")["start"].get<float>();
        }
        if(message.at("Sector 2")["On Off"] == 1) {
            az2 = message.at("Sector 2")["start"].get<float>();
            el2 = message.at("Sector 2")["start"].get<float>();
        }
        acsEntity_.createSHADOW(lradId, az1, el1, az2, el2);
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_inhibition_sectors_INS, nackreason, message);
        // Process the inhibition sectors as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        cmsEntity_.sendLRAS_CS_ack_INS(Topics::CS_LRAS_inhibition_sectors_INS, nackreason, message);
    }
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



