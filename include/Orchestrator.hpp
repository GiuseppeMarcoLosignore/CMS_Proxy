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
#include "Timer.hpp"

#include "../utils/CueingUpdateJson.hpp"




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

    bool ladEnabled = true;
    bool searchlightEnabled = true;
    bool lrfEnabled = true;
    bool audioEnabled = true;
    bool isRecording = false;
    bool isCmsConnected = false;
    float ladMinDistance = 0.0F; //NOHD Distance
    float warningDistance = 0.0F; //THRESHHOLD distance
    float dissuasionDistance = 0.0F;
    float persuasionDistance = 0.0F;

    nlohmann::json cueingData; // Store the latest cueing data for this LRAD

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
    void initializeDefaultStatus();
    void initializeLradNetinfo(const std::string& ipAddress, const uint8_t& lradId);
    void setNetConfigCallback(std::function<void(const std::string&, const uint16_t&)> callback);
    void addNewLrad(const std::string& name, const uint8_t& lradId);

    bool isDataUpdated() const;
    bool isAcsConnected() const;
    bool isCmsConnected() const;
    bool isLradControlledByCms(int lradIndex) const;
    bool isPayloadEnabled(int lradId, PayoladType type) const;
    bool canLadFire(int lradId) const;
    bool inInShadow(int lradId) const;

    void enablePayload(int lradId, PayoladType type, bool enable);

    void setLradFullStatus(lradStatus status, std::string name_);
    void setLrasFullStatus(lrasStatus status);
    lradStatus getLradFullStatus(const std::string& name_) const;
    lrasStatus getLrasFullStatus() const;

    void extractALIVEdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractDIAGNOSTICdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractAUDIOdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractLADdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractSEARCHLIGHTdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractLRFdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractSHADOWdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractZOOMdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractMASTERdata(const uint8_t& lradId, const nlohmann::json& payload);
    void extractPOSITIONdata(const uint8_t& lradId, const nlohmann::json& payload);

    void handleLRFmode(int destinationLradId, const nlohmann::json& payload);
    void handleLADmode(int destinationLradId, const nlohmann::json& payload);
    void handleSearchlightMode(int destinationLradId, const nlohmann::json& payload);
    void handleAudioSettings(int destinationLradId, const nlohmann::json& payload);
    void handleSearchlightAdvanced(int destinationLradId, const nlohmann::json& payload);
    void handleZoomMode(int destinationLradId, const nlohmann::json& payload);
    void handleChangeRequest(int destinationLradId, const nlohmann::json& payload);
    void handleLADenable(int destinationLradId, const nlohmann::json& payload);
    void handleLRFenable(int destinationLradId, const nlohmann::json& payload);
    void handleSEARCHLIGHTenable(int destinationLradId, const nlohmann::json& payload);
    void handleAUDIOenable(int destinationLradId, const nlohmann::json& payload);
    void handleAbsoluteMove(int destinationLradId, const nlohmann::json& payload);
    void handleDeltaMove(int destinationLradId, const nlohmann::json& payload);
    void handleAzShadow(int destinationLradId, const nlohmann::json& payload);
    void handleElShadow(int destinationLradId, const nlohmann::json& payload);

    void start_cueing(int lradId, const nlohmann::json& message);
    void stop_cueing(int lradId);
    void update_cueing(int lradId, const nlohmann::json& message);
    void cueing_availability(int lradId, const nlohmann::json& message);
    void manage_recording(nlohmann::json message);



private:
    void sendAckForTopic(const std::string& topic, uint16_t nackreason, const nlohmann::json& message) const;

    std::shared_ptr<std::vector<lradStatus>> lradList_; //TODO: capire come cestire atomic
    std::shared_ptr<lrasStatus> lras;
    mutable std::mutex lradMutex_;
    mutable std::mutex lrasMutex_;
    std::shared_ptr<Timer> pendingCmsTimer_;
    std::shared_ptr<Timer> pendingAcsTimer_;
    CmsEntity &cmsEntity_;
    AcsEntity &acsEntity_;
    std::thread updateThread_;
    std::jthread cueingThread_;
    std::shared_ptr<EventBus> eventBus_;
    std::function<void(const std::string&, const uint16_t&)> netConfigCallback_;
};