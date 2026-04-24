#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <atomic>

#include "CmsEntity.hpp"
#include "AcsEntity.hpp"
#include "NavsEntity.hpp"

struct Lrad_full {
    std::string name;
    uint16_t lrad_status;
    uint16_t lrad_mode;
    uint16_t cueing_status;
    uint16_t video_tracking_status;
    float lrf_distance_m;
    float lrf_signal_strength;
    int16_t lrf_temperature_c;
    uint16_t inhibition_sector_flag;
    uint16_t audio_emitter_status;
    uint16_t audio_emitter_mode;
    uint16_t searchlight_status;
    uint16_t searchlight_mode;
    uint16_t laser_dazzler_status;
    uint16_t laser_dazzler_mode;
    uint16_t camera_status;
    uint16_t camera_zoom_level;
};

struct Lras_full {
    uint16_t lras_status;
    uint16_t lras_mode;
};


class Orchestrator {
public:
    Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, NavsEntity &navsEntity);

    void start();
    void stop();

    void subscribeTopics();
    bool isDataUpdated() const;

    void setLradFullStatus(Lrad_full status, std::string name_);
    void setLrasFullStatus(Lras_full status);
    Lrad_full getLradFullStatus() const;
    Lras_full getLrasFullStatus() const;


private:
    std::vector<Lrad_full> lradList_;
    Lras_full lrasList_;
    std::mutex lradMutex_;
    std::mutex lrasMutex_;
    CmsEntity &cmsEntity_;
    AcsEntity &acsEntity_;
    NavsEntity &navsEntity_;
    std::thread updateThread_;
};