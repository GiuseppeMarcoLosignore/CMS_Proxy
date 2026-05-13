#include "Orchestrator.hpp"

#include "AcsEntity.hpp"
#include "EventBus.hpp"
#include "Topics.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>

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

    // initializeDefaultStatus();
    for(size_t i = 0; i < extractAcsDestination("config/network_config.ini").size(); ++i) 
        addNewLrad("LRAD" + std::to_string(i+1), static_cast<uint8_t>(i+1));
        
    
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

void Orchestrator::initializeLradNetinfo(const std::string& ipAddress, const uint8_t& lradId) {
    if (ipAddress.empty()) {
        return;
    }

    const std::string configPath = "config/network_config.ini";
    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        std::cerr << "[Orchestrator] Impossibile aprire il file di configurazione: " << configPath << std::endl;
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    inFile.close();

    auto trim = [](std::string value) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    };

    const std::string idText = std::to_string(static_cast<unsigned int>(lradId));
    const std::string sectionHeader = "[acs.destination." + idText + "]";

    std::vector<std::string> destinationBlock;
    destinationBlock.push_back(sectionHeader);
    destinationBlock.push_back("name = ");
    destinationBlock.push_back("lrad = LRAD" + idText);
    destinationBlock.push_back("id   = " + idText);
    destinationBlock.push_back("ip   = " + ipAddress);
    destinationBlock.push_back("port = 9000");

    std::size_t sectionStart = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (trim(lines[i]) == sectionHeader) {
            sectionStart = i;
            break;
        }
    }

    if (sectionStart < lines.size()) {
        std::size_t sectionEnd = lines.size();
        for (std::size_t i = sectionStart + 1; i < lines.size(); ++i) {
            const std::string trimmed = trim(lines[i]);
            if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
                sectionEnd = i;
                break;
            }
        }

        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(sectionStart),
                    lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd));
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(sectionStart),
                     destinationBlock.begin(),
                     destinationBlock.end());
    } else {
        if (!lines.empty() && !lines.back().empty()) {
            lines.push_back("");
        }
        lines.insert(lines.end(), destinationBlock.begin(), destinationBlock.end());
    }

    const std::string tempPath = configPath + ".tmp";
    std::ofstream outFile(tempPath, std::ios::trunc);
    if (!outFile.is_open()) {
        std::cerr << "[Orchestrator] Impossibile scrivere il file temporaneo di configurazione: " << tempPath << std::endl;
        return;
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        if (i + 1 < lines.size()) {
            outFile << '\n';
        }
    }

    outFile.flush();
    outFile.close();

    std::error_code fsError;
    std::filesystem::remove(configPath, fsError);
    fsError.clear();
    std::filesystem::rename(tempPath, configPath, fsError);
    if (fsError) {
        std::cerr << "[Orchestrator] Impossibile sostituire il file di configurazione: " << fsError.message() << std::endl;
        std::filesystem::remove(tempPath, fsError);
        return;
    }

    if (netConfigCallback_) {
        netConfigCallback_(ipAddress, 9000);
    } else {
        std::cerr << "[Orchestrator] Callback di rete non impostata, skip notifica net config" << std::endl;
    }

    addNewLrad("LRAD" + idText, lradId);
}

void Orchestrator::setNetConfigCallback(std::function<void(const std::string&, const uint16_t&)> callback) {
    netConfigCallback_ = callback;
}

void Orchestrator::addNewLrad(const std::string& name, const uint8_t& lradId) {
    if (name.empty()) {
        return;
    }

    lradStatus newLrad{};
    newLrad.alive.name = name;
    newLrad.alive.state = "Unknown";
    newLrad.alive.mode = "Unknown";
    newLrad.alive.ipAddress = "0.0.0.0";
    newLrad.alive.swVersion = "Unknown";
    newLrad.lrad_id = lradId;
    newLrad.controlledByCms = false;
    newLrad.cueingActive = false;
    newLrad.videotracking = false;
    newLrad.ladEnabled = true;
    newLrad.searchlightEnabled = true;
    newLrad.lrfEnabled = true;
    newLrad.audioEnabled = true;
    newLrad.isRecording = false;
    newLrad.isCmsConnected = false;
    newLrad.audio.gain = 0.0F;
    newLrad.audio.mute = false;
    newLrad.lad.mode = "OFF";
    newLrad.searchlight.mode = "OFF";
    newLrad.searchlight.power = "OFF";
    newLrad.searchlight.focus = "0";
    newLrad.lrf.mode = "OFF";
    newLrad.lrf.value = 0.0F;
    newLrad.zoom.id = "HD";
    newLrad.zoom.value = 0.0F;

    {
        std::lock_guard<std::mutex> lock(lradMutex_);
        std::vector<lradStatus> lradList;
        if (const std::shared_ptr<std::vector<lradStatus>> lradListPtr = std::atomic_load(&lradList_); lradListPtr) {
            lradList = *lradListPtr;
        }

        const auto alreadyPresent = std::find_if(
            lradList.begin(),
            lradList.end(),
            [&name, lradId](const lradStatus& lrad) {
                return lrad.alive.name == name || lrad.lrad_id == lradId;
            }
        );

        if (alreadyPresent != lradList.end()) {
            return;
        }

        lradList.push_back(std::move(newLrad));
        std::atomic_store(&lradList_, std::make_shared<std::vector<lradStatus>>(std::move(lradList)));
    }
}

void Orchestrator::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    //new topics
    eventBus_->subscribe(Topics::LRF_MODE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLRFmode(lradId, message); 
    });

    eventBus_->subscribe(Topics::LRF_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractLRFdata(lradId, message);
    });




    eventBus_->subscribe(Topics::LAD_MODE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLADmode(lradId, message); 
    });

    eventBus_->subscribe(Topics::LAD_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractLADdata(lradId, message);
    });

    eventBus_->subscribe(Topics::ZOOM_SETTINGS, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleZoomMode(lradId, message);
    });

    eventBus_->subscribe(Topics::ZOOM_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractZOOMdata(lradId, message);
    });


    eventBus_->subscribe(Topics::SEARCHLIGHT_ADVANCED, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightAdvanced(lradId, message);
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_MODE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSearchlightMode(lradId, message);
    });

    eventBus_->subscribe(Topics::SEARCHLIGHT_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractSEARCHLIGHTdata(lradId, message);
    });



    eventBus_->subscribe(Topics::AUDIO_SETTINGS, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleAudioSettings(lradId, message);
    });

    eventBus_->subscribe(Topics::AUDIO_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractAUDIOdata(lradId, message);
    });




    eventBus_->subscribe(Topics::CHANGE_REQ, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleChangeRequest(lradId, message);
    });

    eventBus_->subscribe(Topics::MASTER_INFO, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractMASTERdata(lradId, message);
    });





    eventBus_->subscribe(Topics::LAD_ENABLE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLADenable(lradId, message);
    });

    eventBus_->subscribe(Topics::LRF_ENABLE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleLRFenable(lradId, message);
    });

    eventBus_->subscribe(Topics::LIGHT_ENABLE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleSEARCHLIGHTenable(lradId, message);
    });

    eventBus_->subscribe(Topics::AUDIO_ENABLE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleAUDIOenable(lradId, message);
    });



    eventBus_->subscribe(Topics::AcsAlive, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        extractALIVEdata(lradId, message);
    });


    eventBus_->subscribe(Topics::MOVE_ABSOLUTE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleAbsoluteMove(lradId, message);
    });

    eventBus_->subscribe(Topics::MOVE_DELTA, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleDeltaMove(lradId, message);
    });

    eventBus_->subscribe(Topics::AZ_SHADOW, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleAzShadow(lradId, message);
    });

    eventBus_->subscribe(Topics::EL_SHADOW, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        handleElShadow(lradId, message);
    });


    eventBus_->subscribe(Topics::CUEING_START, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        start_cueing(lradId, message);
    });

    eventBus_->subscribe(Topics::CUEING_STOP, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        stop_cueing(lradId);
    });

    eventBus_->subscribe(Topics::CUEING_UPDATE, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        update_cueing(lradId, message);
    });

    eventBus_->subscribe(Topics::CUEING_AVIABILITY, [this](const std::string& topic, const uint16_t lradId, const nlohmann::json& message) {
        cueing_availability(lradId, message);
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
    
    
    if (!payload.contains("param") || !payload.at("param").is_object()) {
        return;
    }

    const auto& param = payload.at("param");

    if(name == "PORT" || name == "STARBOARD") {
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
    else {
        std::size_t lradCount = lradId - 1; // Default to current ID minus one
        {
            std::lock_guard<std::mutex> lock(lradMutex_);
            if (const std::shared_ptr<std::vector<lradStatus>> lradListPtr = std::atomic_load(&lradList_); lradListPtr) {
                lradCount = lradListPtr->size();
            }
        }

        if (static_cast<std::size_t>(lradId) < lradCount + 1) {
            std::string discoveredIp;
            if (param.contains("ipAddress") && param.at("ipAddress").is_string()) {
                discoveredIp = param.at("ipAddress").get<std::string>();
            } else if (param.contains("ip") && param.at("ip").is_string()) {
                discoveredIp = param.at("ip").get<std::string>();
            }

            if (!discoveredIp.empty()) {
                initializeLradNetinfo(discoveredIp, lradCount + 1);
            }
        }
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

void Orchestrator::enablePayload(int lradId, PayoladType type, bool enable) {
    lradStatus lrad = getLradFullStatus((lradId == 1) ? "PORT" : "STARBOARD");
    switch (type) {
        case PayoladType::AUDIO:       lrad.audioEnabled = enable; break;
        case PayoladType::LAD:         lrad.ladEnabled = enable; break;
        case PayoladType::SEARCHLIGHT: lrad.searchlightEnabled = enable; break;
        case PayoladType::LRF:         lrad.lrfEnabled = enable; break;
    }
    setLradFullStatus(std::move(lrad), (lradId == 1) ? "PORT" : "STARBOARD");
}


void Orchestrator::extractMASTERdata(const uint8_t& lradId, const nlohmann::json& payload) {
    const std::string name = (lradId == 1) ? "PORT" : (lradId == 2) ? "STARBOARD" : "";
    if (name.empty()) return;
    if (!payload.contains("param")) {
        return;
    }


    if(pendingCmsTimer_ ) {
        pendingCmsTimer_->destroy();
        pendingCmsTimer_ = nullptr;
    }
    const auto& param = payload.at("param");
    lradStatus lrad = getLradFullStatus(name);
    if (param.contains("mode") && param.at("mode").is_string()) {
        const std::string mode = param.at("mode").get<std::string>();
        if(mode == "ACCEPT") 
        lrad.controlledByCms = true;
        if(mode == "REFUSE") 
        lrad.controlledByCms = false;
        if(mode == "REQUEST") {
        pendingAcsTimer_ = std::make_shared<Timer>(std::chrono::seconds(120), [this, lradId]() {
        eventBus_->publish(Topics::CHANGE_REQ, lradId, nlohmann::json{{"param", {{"mode", "ACCEPT"}}}});
        });
        cmsEntity_.sendControlReq(lradId);
        pendingAcsTimer_->start();
        }
    }
    setLradFullStatus(std::move(lrad), name);
}

void Orchestrator::handleLRFmode(int destinationLradId,  const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LRF command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_MODE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LRF)) { //trial
        std::cout << "[Orchestrator] Cannot handle LRF command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LRF command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!payload.contains("mode") || !payload.at("mode").is_string()) {
        std::cout << "[Orchestrator] Cannot handle LRF command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    const std::string mode = payload.at("mode").get<std::string>();
    if(mode == "ON") {
        cmsEntity_.eventStatus(Topics::LRF_MODE, StatusEventValue::NO_ERR);
        acsEntity_.turnLRFon(destinationLradId);
    } else if(mode == "OFF") {
        cmsEntity_.eventStatus(Topics::LRF_MODE, StatusEventValue::NO_ERR);
        acsEntity_.turnLRFoff(destinationLradId);
    }
}


void Orchestrator::handleLADmode(int destinationLradId,  const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LAD command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::LAD)) { //trial
        std::cout << "[Orchestrator] Cannot handle LAD command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LAD command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(inInShadow(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LAD command: LRAD in shadow" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!canLadFire(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LAD command: LRF conditions not met" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!payload.contains("mode") || !payload.at("mode").is_string()) {
        std::cout << "[Orchestrator] Cannot handle LAD command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    const std::string mode = payload.at("mode").get<std::string>();
    if(mode == "ON") {
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::NO_ERR);
        acsEntity_.turnLADon(destinationLradId);
    } else if(mode == "OFF") {
        cmsEntity_.eventStatus(Topics::LAD_MODE, StatusEventValue::NO_ERR);
        acsEntity_.turnLADoff(destinationLradId);
    }
}



void Orchestrator::handleSearchlightMode(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_MODE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight command: Payload not enabled" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle Searchlight command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(inInShadow(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle Searchlight command: LRAD in shadow" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_MODE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    const std::string mode = payload.at("mode").get<std::string>();
    if(mode == "ON") {
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_MODE, StatusEventValue::NO_ERR);
        acsEntity_.turnSearchlightOn(destinationLradId);
    } else if(mode == "OFF") {
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_MODE, StatusEventValue::NO_ERR);
        acsEntity_.turnSearchlightOff(destinationLradId);
    }
}



void Orchestrator::handleAudioSettings(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Audio gain command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle Audio gain command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(payload.contains("gain") && payload.at("gain").is_number()) {
        float gain = -1 * payload.at("gain").get<float>();
        if(gain < -128.0f || gain > 0.0f) {
            std::cout << "[Orchestrator] Cannot handle Audio gain command: Invalid gain value" << std::endl;
            cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::SYSTEM_ERR);
            return;
        }
        cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::NO_ERR);
        acsEntity_.setGain(destinationLradId, gain);
    } else {
        std::cout << "[Orchestrator] Cannot handle Audio gain command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(payload.contains("mute") && payload.at("mute").is_boolean()) {
        bool mute = payload.at("mute").get<bool>();
        cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::NO_ERR);
        acsEntity_.setMute(destinationLradId, mute);
    } else {
        std::cout << "[Orchestrator] Cannot handle Audio mute command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }



}

void Orchestrator::handleSearchlightAdvanced(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle Searchlight advanced command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isPayloadEnabled(destinationLradId, PayoladType::SEARCHLIGHT)) { //trial
        std::cout << "[Orchestrator] Cannot handle Searchlight advanced command: Payload not enabled" << std::endl;
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle Searchlight advanced command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(inInShadow(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle Searchlight advanced command: LRAD in shadow" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(payload.contains("power") && payload.at("power").is_string()) {
        std::string power = payload.at("power").get<std::string>();
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::NO_ERR);
        acsEntity_.setSearchlightPower(destinationLradId, power == "OFF" ? 0 : power == "LOW" ? 1 : power == "MID" ? 2 : 3);
    } else {
        std::cout << "[Orchestrator] Cannot handle Searchlight power command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::SYSTEM_ERR);
        return;
    }
     if(payload.contains("focus") && payload.at("focus").is_number_unsigned()) {
        uint16_t focus = payload.at("focus").get<uint16_t>();
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::NO_ERR);
        acsEntity_.setSearchlightFocus(destinationLradId, focus);
    } else {
        std::cout << "[Orchestrator] Cannot handle Searchlight focus command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::SEARCHLIGHT_ADVANCED, StatusEventValue::SYSTEM_ERR);
        return;
    }
}

void Orchestrator::handleZoomMode(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle HD zoom command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!payload.contains("target") || !payload["target"].is_string()) { 
        std::cout << "[Orchestrator] Cannot handle HD zoom command: Invalid zoom value" << std::endl;
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle zoom command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }

    std::string target = payload["target"].get<std::string>();

    if(!payload.contains("value")) { 
        std::cout << "[Orchestrator] Cannot handle HD zoom command: Invalid zoom value" << std::endl;
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }   
    uint16_t value = payload["value"].get<uint16_t>();

    if(target == "HD") {
        if(value < 0 || value > 29) {
            std::cout << "[Orchestrator] Cannot handle HD zoom command: Invalid zoom value" << std::endl;
            cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::SYSTEM_ERR);
            return;
        }
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::NO_ERR);
        acsEntity_.setHdZoom(destinationLradId, value);
    } else if(target == "TH") {
        if(value < 0 || value > 29) {
            std::cout << "[Orchestrator] Cannot handle TH zoom command: Invalid zoom value" << std::endl;
            cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::SYSTEM_ERR);
            return;
        }
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::NO_ERR);
        acsEntity_.setThZoom(destinationLradId, value);
    } else {
        std::cout << "[Orchestrator] Cannot handle zoom command: Invalid target" << std::endl;
        cmsEntity_.eventStatus(Topics::ZOOM_SETTINGS, StatusEventValue::SYSTEM_ERR);
        return;
    }
    
}

void Orchestrator::handleChangeRequest(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle change request command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::CHANGE_REQ, StatusEventValue::NETWORK_ERR);
        return;
    }
    std::string mode = payload.contains("mode") && payload["mode"].is_string() ? payload["mode"].get<std::string>() : "";
    std::string resolvedMode = mode;
    lradStatus lrad = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if (mode == "REQ" && isLradControlledByCms(destinationLradId)) {
        resolvedMode = "REFUSE";
    }
    if(mode == "RELEASE") {
        if(pendingAcsTimer_) {
            pendingAcsTimer_->destroy();
            pendingAcsTimer_ = nullptr;
        }
        lrad.controlledByCms = false;
        setLradFullStatus(std::move(lrad), (destinationLradId == 1) ? "PORT" : "STARBOARD");
        if(isLradControlledByCms(destinationLradId)) resolvedMode = "ACCEPT";

    }

    if(mode == "REQ") {
        pendingCmsTimer_ = std::make_shared<Timer>(std::chrono::seconds(5), [this, destinationLradId]() {
            eventBus_->publish(Topics::AcsMaster, destinationLradId, nlohmann::json{{"param", {{"mode", "ACCEPT"}}}});
        });
        pendingCmsTimer_->start();
    }


    cmsEntity_.eventStatus(Topics::CHANGE_REQ, StatusEventValue::NO_ERR);
    acsEntity_.setChangeRequest(destinationLradId, resolvedMode);
}

void Orchestrator::handleLADenable(int destinationLradId, const nlohmann::json& message) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LAD enable command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_ENABLE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LAD enable command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    lradStatus lrad = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if(message.contains("enable") && message["enable"].is_boolean()) {
        enablePayload(destinationLradId, PayoladType::LAD, message["enable"].get<bool>());
            cmsEntity_.eventStatus(Topics::LAD_ENABLE, StatusEventValue::NO_ERR);
            return;
        
    } else {
        std::cout << "[Orchestrator] Cannot handle LAD enable command: Invalid message format" << std::endl;
        cmsEntity_.eventStatus(Topics::LAD_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }

    
}

void Orchestrator::handleLRFenable(int destinationLradId, const nlohmann::json& message) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle LRF enable command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_ENABLE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle LRF enable command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    lradStatus lrad = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if(message.contains("enable") && message["enable"].is_boolean()) {
        enablePayload(destinationLradId, PayoladType::LRF, message["enable"].get<bool>());
        cmsEntity_.eventStatus(Topics::LRF_ENABLE, StatusEventValue::NO_ERR);
        return;
    } else {
        std::cout << "[Orchestrator] Cannot handle LRF enable command: Invalid message format" << std::endl;
        cmsEntity_.eventStatus(Topics::LRF_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
}

void Orchestrator::handleSEARCHLIGHTenable(int destinationLradId, const nlohmann::json& message) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle SEARCHLIGHT enable command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::LIGHT_ENABLE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle SEARCHLIGHT enable command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::LIGHT_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    lradStatus lrad = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if(message.contains("enable") && message["enable"].is_boolean()) {
        enablePayload(destinationLradId, PayoladType::SEARCHLIGHT, message["enable"].get<bool>());
        cmsEntity_.eventStatus(Topics::LIGHT_ENABLE, StatusEventValue::NO_ERR);
        return;
    } else {
        std::cout << "[Orchestrator] Cannot handle SEARCHLIGHT enable command: Invalid message format" << std::endl;
        cmsEntity_.eventStatus(Topics::LIGHT_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
}

void Orchestrator::handleAUDIOenable(int destinationLradId, const nlohmann::json& message) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle AUDIO enable command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_ENABLE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle AUDIO enable command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    lradStatus lrad = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if(message.contains("enable") && message["enable"].is_boolean()) {
        enablePayload(destinationLradId, PayoladType::AUDIO, message["enable"].get<bool>());
        cmsEntity_.eventStatus(Topics::AUDIO_ENABLE, StatusEventValue::NO_ERR);
        return;
    } else {
        std::cout << "[Orchestrator] Cannot handle AUDIO enable command: Invalid message format" << std::endl;
        cmsEntity_.eventStatus(Topics::AUDIO_ENABLE, StatusEventValue::SYSTEM_ERR);
        return;
    }
}

void Orchestrator::handleAbsoluteMove(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle absolute move command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::MOVE_ABSOLUTE, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle absolute move command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::MOVE_ABSOLUTE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!payload.contains("az") || !payload.contains("el") || !payload["az"].is_number() || !payload["el"].is_number()) {
        std::cout << "[Orchestrator] Cannot handle absolute move command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::MOVE_ABSOLUTE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    float az = payload["az"].get<float>();
    float el = payload["el"].get<float>();
    cmsEntity_.eventStatus(Topics::MOVE_ABSOLUTE, StatusEventValue::NO_ERR);
    acsEntity_.setMoveAbsolute(destinationLradId, az, el);
}

void Orchestrator::handleDeltaMove(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle delta move command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::MOVE_DELTA, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle delta move command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::MOVE_DELTA, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!payload.contains("xPosition") || !payload.contains("yPosition") || !payload["xPosition"].is_number() || !payload["yPosition"].is_number()) {
        std::cout << "[Orchestrator] Cannot handle delta move command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::MOVE_DELTA, StatusEventValue::SYSTEM_ERR);
        return;
    }
    float az = payload["xPosition"].get<float>();
    float el = payload["yPosition"].get<float>();
    cmsEntity_.eventStatus(Topics::MOVE_DELTA, StatusEventValue::NO_ERR);
    acsEntity_.setMoveDelta(destinationLradId, az, el);
}

void Orchestrator::handleAzShadow(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle azimuth shadow command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::AZ_SHADOW, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle azimuth shadow command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::AZ_SHADOW, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!payload.contains("az1") || !payload.contains("az2") || !payload["az1"].is_number() || !payload["az2"].is_number()) {
        std::cout << "[Orchestrator] Cannot handle azimuth shadow command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::AZ_SHADOW, StatusEventValue::SYSTEM_ERR);
        return;
    }
    float az1 = payload["az1"].get<float>();
    float az2 = payload["az2"].get<float>();
    cmsEntity_.eventStatus(Topics::AZ_SHADOW, StatusEventValue::NO_ERR);
    acsEntity_.setAzShadow(destinationLradId, az1, az2);
}

void Orchestrator::handleElShadow(int destinationLradId, const nlohmann::json& payload) {
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot handle elevation shadow command: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::EL_SHADOW, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot handle elevation shadow command: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::EL_SHADOW, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(!payload.contains("el1") || !payload.contains("el2") || !payload["el1"].is_number() || !payload["el2"].is_number()) {
        std::cout << "[Orchestrator] Cannot handle elevation shadow command: Invalid payload" << std::endl;
        cmsEntity_.eventStatus(Topics::EL_SHADOW, StatusEventValue::SYSTEM_ERR);
        return;
    }
    float el1 = payload["el1"].get<float>();
    float el2 = payload["el2"].get<float>();
    cmsEntity_.eventStatus(Topics::EL_SHADOW, StatusEventValue::NO_ERR);
    acsEntity_.setElShadow(destinationLradId, el1, el2);
}

void Orchestrator::start_cueing(int destinationLradId, const nlohmann::json& message) {
    std::cout << "[Orchestrator] start_cueing: TODO" << std::endl;
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot start cueing: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::CUEING_START, StatusEventValue::NETWORK_ERR);
        return;
    }
     if(!isLradControlledByCms(destinationLradId)) {
        std::cout << "[Orchestrator] Cannot start cueing: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::CUEING_START, StatusEventValue::SYSTEM_ERR);
        return;
    }
    cueingThread_ = std::jthread([this, destinationLradId, message](std::stop_token st) {
        // Simulate cueing process
        lradStatus lradStatus = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
        lradStatus.cueingActive = true;
        lradStatus.cueingData = message;
        nlohmann::json output;
        setLradFullStatus(lradStatus, (destinationLradId == 1) ? "PORT" : "STARBOARD");
        while(!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            if(!UpdateCueingFromJson(lradStatus.cueingData, &output)) {
                cmsEntity_.eventStatus(Topics::CUEING_UPDATE, StatusEventValue::SYSTEM_ERR);
                 return;
            }
            if(output != nullptr) {
                eventBus_->publish(Topics::MOVE_ABSOLUTE, destinationLradId, output);
                output = nullptr;
            }
            lradStatus = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
        }
    });
    cmsEntity_.eventStatus(Topics::CUEING_START, StatusEventValue::NO_ERR);   

}

void Orchestrator::stop_cueing(int lradId) {
    std::cout << "[Orchestrator] stop_cueing: TODO" << std::endl;
    if(!isAcsConnected()) {
        std::cout << "[Orchestrator] Cannot stop cueing: ACS not connected" << std::endl;
        cmsEntity_.eventStatus(Topics::CUEING_STOP, StatusEventValue::NETWORK_ERR);
        return;
    }
    if(!isLradControlledByCms(lradId)) {
        std::cout << "[Orchestrator] Cannot stop cueing: LRAD not controlled by CMS" << std::endl;
        cmsEntity_.eventStatus(Topics::CUEING_STOP, StatusEventValue::SYSTEM_ERR);
        return;
    }
    if(cueingThread_.joinable()) {
        cueingThread_.request_stop();
        cueingThread_.join();
    }
    lradStatus lradStatus = getLradFullStatus((lradId == 1) ? "PORT" : "STARBOARD");
    lradStatus.cueingActive = false;
    setLradFullStatus(lradStatus, (lradId == 1) ? "PORT" : "STARBOARD");
    cmsEntity_.eventStatus(Topics::CUEING_STOP, StatusEventValue::NO_ERR);
}

void Orchestrator::update_cueing(int destinationLradId, const nlohmann::json& message) {

    lradStatus lradStatus = getLradFullStatus((destinationLradId == 1) ? "PORT" : "STARBOARD");
    if(!lradStatus.cueingActive) {
        std::cout << "[Orchestrator] Cannot update cueing: Cueing not active" << std::endl;
        cmsEntity_.eventStatus(Topics::CUEING_UPDATE, StatusEventValue::SYSTEM_ERR);
        return;
    }
    lradStatus.cueingData = message;
    setLradFullStatus(lradStatus, (destinationLradId == 1) ? "PORT" : "STARBOARD");
    cmsEntity_.eventStatus(Topics::CUEING_UPDATE, StatusEventValue::NO_ERR);
}

void Orchestrator::manage_recording(nlohmann::json /*message*/) {
    std::cout << "[Orchestrator] manage_recording: TODO" << std::endl;
}


