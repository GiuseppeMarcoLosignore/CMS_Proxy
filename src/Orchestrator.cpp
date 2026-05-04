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
    if (topic == Topics::CS_LRAS_change_configuration_order_INS) return 1679949825;
    if (topic == Topics::CS_LRAS_cueing_order_cancellation_INS) return 1679949826;
    if (topic == Topics::CS_LRAS_cueing_order_INS) return 1679949827;
    if (topic == Topics::CS_LRAS_emission_control_INS) return 1679949828;
    if (topic == Topics::CS_LRAS_emission_mode_INS) return 1679949829;
    if (topic == Topics::CS_LRAS_inhibition_sectors_INS) return 1679949830;
    if (topic == Topics::CS_LRAS_joystick_control_lrad_1_INS) return 1679949831;
    if (topic == Topics::CS_LRAS_joystick_control_lrad_2_INS) return 1679949832;
    if (topic == Topics::CS_LRAS_recording_command_INS) return 1679949833;
    if (topic == Topics::CS_LRAS_request_engagement_capability_INS) return 1679949834;
    if (topic == Topics::CS_LRAS_request_full_status_INS) return 1679949835;
    if (topic == Topics::CS_LRAS_request_message_table_INS) return 1679949836;
    if (topic == Topics::CS_LRAS_request_software_version_INS) return 1679949837;
    if (topic == Topics::CS_LRAS_request_thresholds_INS) return 1679949838;
    if (topic == Topics::CS_LRAS_request_translation_INS) return 1679949839;
    if (topic == Topics::CS_LRAS_video_tracking_command_INS) return 1679949840;
    if (topic == Topics::CS_LRAS_request_emission_mode_INS) return 1679949841;
    if (topic == Topics::CS_LRAS_request_installation_data_INS) return 1679949842;
    if (topic == Topics::CS_MULTI_health_status_INS) return 1684229565;
    if (topic == Topics::CS_MULTI_update_cst_kinematics_INS) return 1684229569;
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



    eventBus_->subscribe(Topics::LRF_ON, [this](const std::string& topic, const nlohmann::json& message) {
        handleLRFon(message.get<int>()); 
    });

    eventBus_->subscribe(Topics::LRF_OFF, [this](const std::string& topic, const nlohmann::json& message) {
        handleLRFoff(message.get<int>()); 
    });


    eventBus_->subscribe(Topics::LRF_INFO, [this](const std::string& topic, const nlohmann::json& message) {
        extractLRFdata(message);
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


void Orchestrator::handleCS_LRAS_change_configuration_order_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_change_configuration_order_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_change_configuration_order_INS, nackreason, message);
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

        sendAckForTopic(Topics::CS_LRAS_change_configuration_order_INS, nackreason, message);
        // Process the configuration change as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        sendAckForTopic(Topics::CS_LRAS_change_configuration_order_INS, nackreason, message);
    }
}

void Orchestrator::handleCS_LRAS_cueing_order_cancellation_INS(const nlohmann::json& message) {

    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_cueing_order_cancellation_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_cueing_order_cancellation_INS, nackreason, message);
        return;
    }
    stop_cueing();
    sendAckForTopic(Topics::CS_LRAS_cueing_order_cancellation_INS, nackreason, message);
    // Process the cueing order cancellation as needed
}

void Orchestrator::handleCS_LRAS_cueing_order_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_cueing_order_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_cueing_order_INS, nackreason, message);
        return;
    }
    start_cueing();
    sendAckForTopic(Topics::CS_LRAS_cueing_order_INS, nackreason, message);
    // Process the cueing order as needed
}

void Orchestrator::handleCS_LRAS_emission_control_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_emission_control_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
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
            sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
            return;
        }

        sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        // Process the configuration change as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
    }
}

void Orchestrator::handleCS_LRAS_emission_mode_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_emission_mode_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        return;
    }

    if(message.contains("LRAD ID")) {
        const uint16_t lradId = message.at("LRAD ID").get<uint16_t>();

        if(isLradControlledByCms(lradId)) {
            const std::string lradName = lradId == 1 ? "LRAD 1" : "LRAD 2";
            lradStatus currentLrad = getLradFullStatus(lradName);

            if(message["Laser Enable Validity"] == 1) {
                currentLrad.ladEnabled = message["Laser on off"] != 0;
            }
            if(message["Light Enable Validity"] == 1) {
                currentLrad.searchlightEnabled = message["Light on off"] != 0;
            }
            if(message["Laser Range Finder Enable Validity"] == 1) {
                currentLrad.lrfEnabled = message["LRF on off"] != 0;
            }
            if(message["Audio Enable Validity"] == 1) {
                currentLrad.audioEnabled = message["Audio on off"] != 0;
            }
            if(message["Audio Volume Levels Validity"] == 1) {
                currentLrad.audioLvl1 = message["Audio Volume Level 1"];
                currentLrad.audioLvl2 = message["Audio Volume Level 2"];
                currentLrad.audioLvl3 = message["Audio Volume Level 3"];
            }

            setLradFullStatus(std::move(currentLrad), lradName);

        } else {
            nackreason = 2; 
            sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
            return;
        }

        sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
        // Process the configuration change as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        sendAckForTopic(Topics::CS_LRAS_emission_control_INS, nackreason, message);
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
        sendAckForTopic(Topics::CS_LRAS_inhibition_sectors_INS, nackreason, message);
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
        sendAckForTopic(Topics::CS_LRAS_inhibition_sectors_INS, nackreason, message);
        // Process the inhibition sectors as needed
        // For example, you might want to update internal state or send a command to the LRAD
    } else {
        nackreason = 2; // Invalid parameter
        sendAckForTopic(Topics::CS_LRAS_inhibition_sectors_INS, nackreason, message);
    }
}

void Orchestrator::handleCS_LRAS_joystick_control_lrad_1_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_joystick_control_lrad_1_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_1_INS, nackreason, message);
        return;
    }
    
    if(isLradControlledByCms(1)) {
        if(message.contains("Azimuth") && message.contains("Elevation")) {
            const float az = message.at("xPosition").get<float>()*0.5; // TODO: capire che sensibilita usare, per ora 0.5 e un valore di esempio
            const float el = message.at("yPosition").get<float>()*0.5;
            acsEntity_.createDELTA(1, az, el);
        } else {
            nackreason = 2; // Invalid parameter
            sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_1_INS, nackreason, message);
            return;
        }
    } else {
        nackreason = 2; 
        sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_1_INS, nackreason, message);
        return;
    }
    sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_1_INS, nackreason, message);
}

void Orchestrator::handleCS_LRAS_joystick_control_lrad_2_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_joystick_control_lrad_2_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_2_INS, nackreason, message);
        return;
    }
    if(isLradControlledByCms(2)) {
        if(message.contains("Azimuth") && message.contains("Elevation")) {
            const float az = message.at("xPosition").get<float>()*0.5; // TODO: capire che sensibilita usare, per ora 0.5 e un valore di esempio
            const float el = message.at("yPosition").get<float>()*0.5;
            acsEntity_.createDELTA(2, az, el);
        } else {
            nackreason = 2; // Invalid parameter
            sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_2_INS, nackreason, message);
            return;
        }
    } else {
        nackreason = 2; 
        sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_2_INS, nackreason, message);
        return;
    }
    sendAckForTopic(Topics::CS_LRAS_joystick_control_lrad_2_INS, nackreason, message);
}

void Orchestrator::handleCS_LRAS_recording_command_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_recording_command_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_recording_command_INS, nackreason, message);
        return;
    }
    manage_recording(message);
    sendAckForTopic(Topics::CS_LRAS_recording_command_INS, nackreason, message);
}

void Orchestrator::handleCS_LRAS_request_emission_mode_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_request_emission_mode_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_request_emission_mode_INS, nackreason, message);
        return;
    }

    const uint16_t lradId = message["LRAD ID"].get<uint16_t>();
    const std::string lradName = lradId == 1 ? "LRAD 1" : "LRAD 2";
    const lradStatus currentLrad = getLradFullStatus(lradName);

    cmsEntity_.sendLRAS_CS_emission_mode_feedback_INS(
        lradId,
        currentLrad.audioEnabled ? 1 : 0,
        currentLrad.audioLvl1,
        currentLrad.audioLvl2,
        currentLrad.audioLvl3,
        currentLrad.ladEnabled ? 1 : 0,
        static_cast<uint32_t>(std::max(0.0F, currentLrad.ladMinDistance)),
        currentLrad.searchlightEnabled ? 1 : 0,
        0,
        currentLrad.lrfEnabled ? 1 : 0
    );
    
    sendAckForTopic(Topics::CS_LRAS_request_emission_mode_INS, nackreason, message);
}

//TODO: implementare logica insieme a cueing
void Orchestrator::handleCS_LRAS_request_engagement_capability_INS(const nlohmann::json& message) {
    uint16_t nackreason = 0; // 0 means no error, 2 means invalid parameter
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle CS_LRAS_request_engagement_capability_INS: ACS not connected" << std::endl;
        nackreason = 3; // 3 means ACS not connected
        sendAckForTopic(Topics::CS_LRAS_request_engagement_capability_INS, nackreason, message);
        return;
    }

    sendAckForTopic(Topics::CS_LRAS_request_engagement_capability_INS, nackreason, message);
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

        lradStatus lrad = getLradFullStatus(name);
        lrad.alive.state = readStringField("state");
        lrad.alive.mode = readStringField("mode");
        lrad.alive.ipAddress = readStringField("ipAddress", "ip");

        setLradFullStatus(std::move(lrad), name);

        lrasStatus lrasStatus = getLrasFullStatus();
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

        lradStatus lrad = getLradFullStatus(name);
        lrad.alive.state = readStringField("state");
        lrad.alive.mode = readStringField("mode");
        lrad.alive.ipAddress = readStringField("ipAddress", "ip");

        setLradFullStatus(std::move(lrad), name);

        const lradStatus portLrad = getLradFullStatus("PORT");
        if (!portLrad.alive.name.empty() && portLrad.alive.mode != "Unknown") {
            lrasStatus lrasStatus = getLrasFullStatus();
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

    lradStatus currentLrad = getLradFullStatus(name);
    currentLrad.audio.gain = readFloatField("gain");
    currentLrad.audio.mute = readBoolField("mute");

    setLradFullStatus(std::move(currentLrad), name);
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

    lradStatus currentLrad = getLradFullStatus(name);

    if (param.contains("mode") && param.at("mode").is_string()) {
        currentLrad.lad.mode = param.at("mode").get<std::string>();
    }

    setLradFullStatus(std::move(currentLrad), name);
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

void Orchestrator::extractLRFdata(const nlohmann::json& payload) {
    if (!payload.contains("sender")) {
        return;
    }

    const std::string name = payload.at("destinationLradId").get<std::string>();
    if (!isKnownLradSender(name)) {
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

    if (param.contains("value") && param.at("value").is_string()) {
        currentLrad.lrf.value = std::stof(param.at("value").get<std::string>());
    }

    setLradFullStatus(std::move(currentLrad), name);
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

bool Orchestrator::isPayloadEnabled(PayoladType type) const {
    const std::shared_ptr<std::vector<lradStatus>> lradListPtr = std::atomic_load(&lradList_);
    if (!lradListPtr || lradListPtr->empty()) {
        return false;
    }
    for (const auto& lrad : *lradListPtr) {
        switch (type) {
            case PayoladType::AUDIO:       if (lrad.audioEnabled)       return true; break;
            case PayoladType::LAD:         if (lrad.ladEnabled)         return true; break;
            case PayoladType::SEARCHLIGHT: if (lrad.searchlightEnabled) return true; break;
            case PayoladType::LRF:         if (lrad.lrfEnabled)         return true; break;
        }
    }
    return false;
}

bool Orchestrator::isShadowEnabled() const {
    return false;
}

void Orchestrator::enablePayload(PayoladType /*type*/, std::string /*enable*/) {
    // TODO
}

void Orchestrator::extractMASTERdata(const nlohmann::json& payload) {
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
    lradStatus lrad = getLradFullStatus(name);
    if (param.contains("mode") && param.at("mode").is_string()) {
        const std::string mode = param.at("mode").get<std::string>();
        lrad.controlledByCms = (mode == "MASTER" || mode == "ACCEPT");
        lrad.alive.mode = mode;
    }
    setLradFullStatus(std::move(lrad), name);
}

void Orchestrator::handleCS_LRAS_request_full_status_INS(const nlohmann::json& message) {
    const lradStatus lrad1 = getLradFullStatus("PORT");
    const lradStatus lrad2 = getLradFullStatus("STARBOARD");
    const auto to_u16 = [](float value) -> uint16_t {
        return static_cast<uint16_t>(std::clamp(value, 0.0F, 65535.0F));
    };
    const auto to_mode = [](const std::string& mode) -> uint16_t {
        return mode == "ON" ? 1u : 0u;
    };
    const auto to_lrf_on = [](const std::string& mode) -> uint16_t {
        return mode == "ON" ? 1u : 0u;
    };
    const auto to_zoom = [](const lradStatus& lrad, const char* id) -> uint16_t {
        if (lrad.zoom.id == id) {
            return static_cast<uint16_t>(std::clamp(lrad.zoom.value, 0.0F, 65535.0F));
        }
        return 0u;
    };
    const auto has_imu = [](const lradStatus& lrad) -> uint16_t {
        return (!lrad.imu.roll.empty() || !lrad.imu.pitch.empty() || !lrad.imu.heading.empty()) ? 1u : 0u;
    };

    cmsEntity_.sendLRAS_MULTI_full_status_v2_INS(
        0,
        0,
        0,
        lrad1.audioEnabled ? 1u : 0u,
        lrad1.audio.mute ? 0u : 1u,
        to_mode(lrad1.searchlight.mode),
        lrad1.searchlightEnabled ? 1u : 0u,
        to_mode(lrad1.lad.mode),
        lrad1.ladEnabled ? 1u : 0u,
        to_u16(lrad1.lrf.value),
        to_lrf_on(lrad1.lrf.mode),
        0,
        lrad1.zoom.id == "HD" ? 1u : 0u,
        to_zoom(lrad1, "HD"),
        has_imu(lrad1),
        0,
        0,
        0,
        0,
        0,
        lrad1.zoom.id == "TH" ? 1u : 0u,
        to_zoom(lrad1, "TH"),
        0,
        0,
        0,
        lrad2.audioEnabled ? 1u : 0u,
        lrad2.audio.mute ? 0u : 1u,
        to_mode(lrad2.searchlight.mode),
        lrad2.searchlightEnabled ? 1u : 0u,
        to_mode(lrad2.lad.mode),
        lrad2.ladEnabled ? 1u : 0u,
        to_u16(lrad2.lrf.value),
        to_lrf_on(lrad2.lrf.mode),
        0,
        lrad2.zoom.id == "HD" ? 1u : 0u,
        to_zoom(lrad2, "HD"),
        has_imu(lrad2),
        0,
        0,
        0,
        0,
        0,
        lrad2.zoom.id == "TH" ? 1u : 0u,
        to_zoom(lrad2, "TH")
    );

    sendAckForTopic(Topics::CS_LRAS_request_full_status_INS, 0, message);
}

void Orchestrator::handleCS_LRAS_request_installation_data_INS(const nlohmann::json& message) {
    cmsEntity_.sendLRAS_CS_installation_data_INS(
        0.0F, 360.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 360.0F, 0.0F, 0.0F, 0.0F);
    sendAckForTopic(Topics::CS_LRAS_request_installation_data_INS, 0, message);
}

void Orchestrator::handleCS_LRAS_request_message_table_INS(const nlohmann::json& message) {
    sendAckForTopic(Topics::CS_LRAS_request_message_table_INS, 0, message);
}

void Orchestrator::handleCS_LRAS_request_software_version_INS(const nlohmann::json& message) {
    const lrasStatus currentLras = getLrasFullStatus();
    cmsEntity_.sendLRAS_CS_software_version_INS(
        "LRAS Server", currentLras.swVersion,
        "LRAD1 Master", "N/A", "LRAD1 Slave", "N/A", "LRAD1 Tracking", "N/A",
        "LRAD2 Master", "N/A", "LRAD2 Slave", "N/A", "LRAD2 Tracking", "N/A",
        "Console1", "N/A", "Console2", "N/A");
    sendAckForTopic(Topics::CS_LRAS_request_software_version_INS, 0, message);
}

void Orchestrator::handleCS_LRAS_request_thresholds_INS(const nlohmann::json& message) {
    cmsEntity_.sendLRAS_CS_thresholds_INS(
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0);
    sendAckForTopic(Topics::CS_LRAS_request_thresholds_INS, 0, message);
}

void Orchestrator::handleCS_LRAS_request_translation_INS(const nlohmann::json& message) {
    sendAckForTopic(Topics::CS_LRAS_request_translation_INS, 0, message);
}

void Orchestrator::handleLRFon(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LRF command: ACS not connected" << std::endl;
        return;
    }
    if(isPayloadEnabled(PayoladType::LRF)) { //trial
        acsEntity_.turnLRFon(destinationLradId);
    }
}

void Orchestrator::handleLRFoff(int destinationLradId) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LRF command: ACS not connected" << std::endl;
        return;
    }
    if(isPayloadEnabled(PayoladType::LRF)) { //trial
        acsEntity_.turnLRFoff(destinationLradId);
    }
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


