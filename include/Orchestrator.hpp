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
#include "NavsEntity.hpp"

struct Lrad_full {
    std::string name;
    uint16_t lrad_status;
    uint16_t lrad_mode;
    uint16_t cueing_status;
    uint16_t video_tracking_status;
    float lrf_value;
    bool lrf_on;
    int16_t lrf_temperature_c;
    uint16_t inhibition_sector_flag;
    uint16_t audio_emitter_status;
    uint16_t audio_emitter_mode;
    uint16_t searchlight_status;
    uint16_t searchlight_mode;
    uint16_t searchlight_power_level;
    uint16_t searchlight_focus;
    uint16_t laser_dazzler_status;
    uint16_t laser_dazzler_mode;

    uint16_t hd_camera_status;
    uint16_t hd_camera_zoom_level;
    uint16_t th_camera_status;
    uint16_t th_camera_zoom_level;

    float Azimuth_deg;
    float Elevation_deg;
    float AzShadowStart;
    float AzShadowEnd;
    float ElShadowStart;
    float ElShadowEnd;

    std::string state;
    std::string mode;
    std::string ipAddress;

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

    float gain = 0.0F;
    bool mute = false;

    bool audioEnabled = false;
    bool ladEnabled = false;
    bool searchlightEnabled = false;
    bool lrfEnabled = false;
    

    bool cmsControl = false; // Indicates if CMS is currently controlling this LRAD
    std::time_t last_update_time;
};

struct Lras_full {
    uint16_t lras_status;
    uint16_t lras_mode;

    std::string swVersion;

    std::time_t last_update_time;
};

enum class PayoladType {
    AUDIO,
    LAD,
    SEARCHLIGHT,
    LRF
};


class Orchestrator {
public:
    Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, NavsEntity &navsEntity, std::shared_ptr<EventBus> eventBus);

    void start();
    void stop();

    void subscribeTopics();

    bool isDataUpdated() const;
    bool isAcsConnected() const;
    bool isCmsConnected() const;
    bool isLradControlledByCms() const;
    bool isPayloadEnabled(PayoladType type) const;
    bool isShadowEnabled() const;

    void enablePayload(PayoladType type, std::string enable);

    void setLradFullStatus(Lrad_full status, std::string name_);
    void setLrasFullStatus(Lras_full status);
    Lrad_full getLradFullStatus(const std::string& name_) const;
    Lras_full getLrasFullStatus() const;

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

    void start_cueing();


private:
    std::shared_ptr<std::vector<Lrad_full>> lradList_; //TODO: capire come cestire atomic
    std::shared_ptr<Lras_full> lras;
    mutable std::mutex lradMutex_;
    mutable std::mutex lrasMutex_;
    CmsEntity &cmsEntity_;
    AcsEntity &acsEntity_;
    NavsEntity &navsEntity_;
    std::thread updateThread_;
    std::shared_ptr<EventBus> eventBus_;
};