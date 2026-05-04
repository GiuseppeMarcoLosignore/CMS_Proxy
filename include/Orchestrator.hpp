#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "CmsEntity.hpp"
#include "AcsEntity.hpp"
#include "EventBus.hpp"
#include "IInterfaces.hpp"




enum class PayoladType {
    AUDIO,
    LAD,
    SEARCHLIGHT,
    LRF
};

struct ALIVE {
    std::string name;
    std::string ipAddress;
    std::string state;
    std::string mode;
    std::string swVersion;
};


struct DIAGNOSTIC {
    bool limitError = false;
    bool lad = false;
    bool lrf = false;
    bool dsp = false;
    bool searchlight = false;
    bool daq = false;
    bool psu12 = false;
    bool psu24 = false;
    bool psu48 = false;
    bool tempVbox = false;
    bool tempAhd = false;

};






//AUDIO
struct AUDIO {
    float gain = 0.0F;
    bool mute = false;
};


//LAD
struct LAD {
    std::string mode;
};


//SEARCHLIGHT
struct SEARCHLIGHT {
    std::string power;
    std::string focus;
    std::string mode;
};


//LRF
struct LRF {
    std::string mode;
    float value;
};


//SHADOW
struct SHADOW {
    bool enabled = false;
    std::string start;
    std::string stop;
};


//ZOOM
struct ZOOM {
    std::string id;
    float value;
};


//MASTER
struct MASTER {
    std::string mode;
};
;

//CONTEXT
struct Context {
    std::string master;
    std::string arcAz; //HW active arc
    std::string arcEl;
    std::string safetySwitch;
    std::string inShadow;
    std::string cms;
};


//POSITION
struct POSITION {
    std::string goTo;
    std::string az;
    std::string el;
};


//DELTA
struct DELTA {
    std::string start;
    std::string stop;
};


//TRACKING
struct TRACKING {
    std::string mode;
    std::string target;
    std::string classification;
};

struct CONFIG {
 std::string name;
 std::string direction;
 std::string hwAzLeft;
 std::string hwAzRight;
 std::string hwElLeft;
 std::string hwElRight;

};

//IMU
struct IMU {
    std::string roll;
std::string pitch;
std::string heading;
};


struct HOURS {
    std::string atom;
    std::string light;
    std::string lad;
    std::string lrf;
    std::string ahd;
    std::string logSession;
};
    
//[WIP]        
struct lradStatus {
    ALIVE alive;
    DIAGNOSTIC diagnostic;

    AUDIO audio;
    LAD lad;
    SEARCHLIGHT searchlight;
    LRF lrf;

    SHADOW az1;
    SHADOW az2;
 

    ZOOM zoom;

    Context context;
    POSITION position;
    bool  videotracking;
    CONFIG config;
    IMU imu;
    HOURS hours;
    TRACKING tracking;

    uint16_t lrad_id;
    bool controlledByCms = false;
    bool cueingActive = false;

    bool ladEnabled = false;
    bool searchlightEnabled = false;
    bool lrfEnabled = false;
    bool audioEnabled = false;
    bool isRecording = false;
    bool isCmsConnected = false;
    float audioLvl1 = 0.0F;
    float audioLvl2 = 0.0F; 
    float audioLvl3 = 0.0F;
    float ladMinDistance = 0.0F; //NOHD Distance
    float warningDistance = 0.0F; //THRESHHOLD distance
    float dissuasionDistance = 0.0F;
    float persuasionDistance = 0.0F;

};

struct lrasStatus {
    
    int totalMessagesNumber = 0;
    int messageNumber = 0;
    int dbItemNumber = 0;
    std::string swVersion;
    
};


class Orchestrator {
public:
    Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, std::shared_ptr<EventBus> eventBus);

    void start();
    void stop();

    void subscribeTopics();

    bool isDataUpdated() const;
    bool isAcsConnected() const;
    bool isCmsConnected() const;
    bool isLradControlledByCms(int lradIndex) const;
    bool isPayloadEnabled(PayoladType type) const;
    bool isShadowEnabled() const;

    void enablePayload(PayoladType type, std::string enable);

    void setLradFullStatus(lradStatus status, std::string name_);
    void setLrasFullStatus(lrasStatus status);
    lradStatus getLradFullStatus(const std::string& name_) const;
    lrasStatus getLrasFullStatus() const;

    void extractALIVEdata(const nlohmann::json& payload);
    void extractDIAGNOSTICdata(const nlohmann::json& payload);
    void extractAUDIOdata(const nlohmann::json& payload);
    void extractLADdata(const nlohmann::json& payload);
    void extractSEARCHLIGHTdata(const nlohmann::json& payload);
    void extractLRFdata(const nlohmann::json& payload);
    void extractSHADOWdata(const nlohmann::json& payload);
    void extractZOOMdata(const nlohmann::json& payload);
    void extractMASTERdata(const nlohmann::json& payload);
    void extractPOSITIONdata(const nlohmann::json& payload);

    void handleCS_LRAS_change_configuration_order_INS(const nlohmann::json& message);
    void handleCS_LRAS_cueing_order_cancellation_INS(const nlohmann::json& message);
    void handleCS_LRAS_cueing_order_INS(const nlohmann::json& message);
    void handleCS_LRAS_emission_control_INS(const nlohmann::json& message); 
    void handleCS_LRAS_emission_mode_INS(const nlohmann::json& message);
    void handleCS_LRAS_inhibition_sectors_INS(const nlohmann::json& message);
    void handleCS_LRAS_joystick_control_lrad_1_INS(const nlohmann   ::json& message);
    void handleCS_LRAS_joystick_control_lrad_2_INS(const nlohmann::json& message);
    void handleCS_LRAS_recording_command_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_emission_mode_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_engagement_capability_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_full_status_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_installation_data_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_message_table_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_software_version_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_thresholds_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_translation_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_version_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_status_INS(const nlohmann::json& message);
    void handleCS_LRAS_request_control_mode_INS(const nlohmann::json& message);

    void handleLRFon(int destinationLradId);
    void handleLRFoff(int destinationLradId);
    void handleLADon(int destinationLradId);
    void handleLADoff(int destinationLradId);
    void handleLADstrobe(int destinationLradId);
    void handleSearchlightOn(int destinationLradId);
    void handleSearchlightOff(int destinationLradId);
    void handleSearchlightStrobe(int destinationLradId);
    void handleAudioGain(int destinationLradId, float gain);
    void handleAudioMute(int destinationLradId, bool mute);

    void start_cueing();
    void stop_cueing();
    void manage_recording(nlohmann::json message);



private:
    void sendAckForTopic(const std::string& topic, uint16_t nackreason, const nlohmann::json& message) const;

    std::shared_ptr<std::vector<lradStatus>> lradList_; //TODO: capire come cestire atomic
    std::shared_ptr<lrasStatus> lras;
    mutable std::mutex lradMutex_;
    mutable std::mutex lrasMutex_;
    CmsEntity &cmsEntity_;
    AcsEntity &acsEntity_;
    std::thread updateThread_;
    std::shared_ptr<EventBus> eventBus_;
};