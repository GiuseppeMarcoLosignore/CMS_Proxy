#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

struct MulticastEndpoint {
    std::string ip;
    uint16_t port = 0;
};

struct RawPacket {
    std::vector<uint8_t> data;
    std::chrono::steady_clock::time_point timestamp;
    uint16_t destinationLradId;
    int nackreason = 0;

    RawPacket() : timestamp(std::chrono::steady_clock::now()), destinationLradId(0) {}
    explicit RawPacket(std::vector<uint8_t> d)
        : data(std::move(d)), timestamp(std::chrono::steady_clock::now()), destinationLradId(0) {}
};

//struct AcsData {
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



//ERROR
struct ERROR {
    std::string message;
    std::string code;
    std::string reason;
};


//AUDIO
struct AUDIO {
    float gain = 0.0F;
    bool mute = false;
    std::string equal;
    std::string lobe;
};


//LAD
struct LAD {
    std::string mode;
    std::string override;
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
struct CONTEXT {
    std::string master;
    std::string arcAz;
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
    std::string az;
    std::string el;
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
    
        
struct Acs {
    ALIVE alive;
    DIAGNOSTIC diagnostic;
    ERROR error;
    AUDIO audio;
    LAD lad;
    SEARCHLIGHT searchlight;
    LRF lrf;

    SHADOW az1;
    SHADOW el1;
    SHADOW az2;
    SHADOW el2;

    ZOOM zoom;
    MASTER master;
    CONTEXT context;
    POSITION position;
    DELTA delta;
    TRACKING tracking;
    CONFIG config;
    IMU imu;
    HOURS hours;

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
    float ladMinDistance = 0.0F;
    //INSTALLATION installation;
    //DATABASE database;
    std::string swVersion;
    //THRESHHOLD threshold;
    
};


struct Lrad_full {
    std::string name;
    uint16_t lrad_status; //Communication status
    uint8_t motionAzState;
    uint8_t motionElState;
    bool controlledByCms = false;
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

    uint16_t IMU_status;

    bool tracking_board_status;
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

    bool audioEnabled = false;
    bool ladEnabled = false;
    bool searchlightEnabled = false;
    bool lrfEnabled = false;

    float gain = 0.0F;
    bool mute = false;


    

    bool cmsControl = false; // Indicates if CMS is currently controlling this LRAD
    std::time_t last_update_time;
};

struct Lras_full {
    uint16_t lras_status;
    uint16_t lras_mode;

    std::string swVersion;



    uint16_t ladMinDistance;

    float audioLvl1;
    float audioLvl2;
    float audioLvl3;

    std::time_t last_update_time;
};

class IEntity {
public:
    virtual ~IEntity() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class IEvent {
public:
    virtual ~IEvent() = default;
    virtual const std::string& topic() const = 0;
};

struct SendResult {
    bool success = false;
    int error_value = 0;
    std::string error_category;
    std::string error_message;
};

enum class TransportProtocol {
    Udp,
    Tcp
};

struct PacketSourceInfo {
    TransportProtocol protocol = TransportProtocol::Udp;
    std::string source_ip;
    uint16_t source_port = 0;
};

class IReceiver {
public:
    virtual ~IReceiver() = default;
    using MessageCallback = std::function<void(const RawPacket&, const PacketSourceInfo&)>;
    
    virtual void set_callback(MessageCallback cb) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class ISender {
public:
    virtual ~ISender() = default;
    virtual SendResult send(const RawPacket& packet, const std::string& target_host, uint16_t target_port) = 0;
};
