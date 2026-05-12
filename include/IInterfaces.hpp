#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

enum class StatusEventValue {
    NO_ERR,
    NETWORK_ERR,
    SYSTEM_ERR,
    UNKNOWN
};

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

class IActuator {
public:    
    virtual ~IActuator() = default;
    virtual void turnLRFon(uint16_t destinationLradId) = 0;
    virtual void turnLRFoff(uint16_t destinationLradId) = 0;
    virtual void turnLADon(uint16_t destinationLradId) = 0;
    virtual void turnLADoff(uint16_t destinationLradId) = 0;
    virtual void turnLADstrobe(uint16_t destinationLradId) = 0;
    virtual void turnSearchlightOn(uint16_t destinationLradId) = 0;
    virtual void turnSearchlightOff(uint16_t destinationLradId) = 0;
    virtual void turnSearchlightStrobe(uint16_t destinationLradId) = 0;
    virtual void setSearchlightFocus(uint16_t destinationLradId, float focus) = 0;
    virtual void setSearchlightPower(uint16_t destinationLradId, const uint8_t& power) = 0;
    virtual void setGain(uint16_t destinationLradId, float gain) = 0;
    virtual void setMute(uint16_t destinationLradId, bool mute) = 0;
    virtual void setHdZoom(uint16_t destinationLradId, const uint8_t zoomValue) = 0;
    virtual void setThZoom(uint16_t destinationLradId, const uint8_t zoomValue) = 0;
    virtual void setChangeRequest(uint16_t destinationLradId, const std::string& mode) = 0;
    virtual void setMoveDelta(uint16_t destinationLradId, float azDelta, float elDelta) = 0;
    virtual void setMoveAbsolute(uint16_t destinationLradId, float azimuth, float elevation) = 0;
    virtual void setAzShadow(uint16_t destinationLradId, float az1, float az2) = 0;
    virtual void setElShadow(uint16_t destinationLradId, float el1, float el2) = 0;

};

class IRemote {
public:
    virtual ~IRemote() = default;
    virtual void eventStatus(const std::string& topic, StatusEventValue value) = 0;
    virtual void sendControlReq(const uint16_t& lradId) = 0;
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

class IDatabase {
public:
    virtual ~IDatabase() = default;
    virtual int getDatabaseSize() = 0;
    virtual void getDatabaseItem(const Lrad_full& status, int itemId) = 0;
};
