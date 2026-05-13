#include <cstdint>
#include <nlohmann/json.hpp>

namespace cueing {
float rad2deg(float angleRad);
void cartesian2target(
    float x,
    float y,
    float z,
    float& az,
    float& el,
    float& range,
    bool isTrueB,
    float& azAbs,
    float crpX,
    float crpY,
    float crpZ,
    float heading);
void Polar2D(
    float trueBearing,
    float angleOfSight,
    float& az,
    float& el,
    bool isTrueB,
    float& azAbs,
    float heading);
void polar2catesian(
    float bearing,
    float range,
    float& x,
    float& y,
    float& z,
    bool isTrueB,
    float heading);
}  // namespace cueing

namespace {

using nlohmann::json;

constexpr float kDefaultOutputValue = 0.0f;

enum class KType : std::int32_t {
    Type3D_CARTESIAN_KINEMATICS = 1,
    Type3D_CARTESIAN_POSITION = 2,
    Type2D_CARTESIAN_KINEMATICS = 3,
    Type2D_CARTESIAN_POSITION = 4,
    Type2D_POLAR_KINEMATICS = 5,
    Type2D_POLAR_SURFACE_KINEMATICS = 6,
    Type2D_POLAR_POSITION = 7,
    Type2D_POLAR_SURFACE_POSITION = 8,
    Type1D_POLAR_POSITION = 9,
    TypeEW_1D_POLAR_POSITION = 10,
    TypeEW_2D_POLAR_POSITION = 11,
};

bool ReadNumber(const json& object, const char* key, float& value) {
    const json::const_iterator it = object.find(key);
    if (it == object.end() || !it->is_number()) {
        return false;
    }

    value = it->get<float>();
    return true;
}

bool ReadInt(const json& object, const char* key, std::int32_t& value) {
    const json::const_iterator it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return false;
    }

    value = it->get<std::int32_t>();
    return true;
}

bool ReadObject(const json& object, const char* key, const json*& value) {
    const json::const_iterator it = object.find(key);
    if (it == object.end() || !it->is_object()) {
        return false;
    }

    value = &(*it);
    return true;
}

void WriteOutput(json& output, float targetAz, float targetAzAbs, float targetEl, float targetRange) {
    output = {
        {"az", targetAz},
        {"azAbs", targetAzAbs},
        {"el", targetEl},
        {"range", targetRange},
    };
}

}  // namespace

// Expected payload schema:
// {
//   "kinematics": 1..11,
//   "headingDeg": number,
//   "crpX": number,
//   "crpY": number,
//   "crpZ": number,
//   "maxLaserRange": number,
//   "values": { ... semantic fields required by the selected kinematics ... }
// }
// On success, if output is not null, the function writes targetAz, targetAzAbs, targetEl and targetRange.
bool UpdateCueingFromJson(const nlohmann::json& payload, nlohmann::json* output = nullptr) {
    std::int32_t kinematicsValue = 0;
    float headingDeg = 0.0f;
    float crpX = 0.0f;
    float crpY = 0.0f;
    float crpZ = 0.0f;
    float maxLaserRange = 0.0f;
    const nlohmann::json* values = nullptr;

    if (!ReadInt(payload, "kinematics", kinematicsValue)) {
        return false;
    }
    if (!ReadNumber(payload, "headingDeg", headingDeg)) {
        return false;
    }
    if (!ReadNumber(payload, "crpX", crpX)) {
        return false;
    }
    if (!ReadNumber(payload, "crpY", crpY)) {
        return false;
    }
    if (!ReadNumber(payload, "crpZ", crpZ)) {
        return false;
    }
    if (!ReadNumber(payload, "maxLaserRange", maxLaserRange)) {
        return false;
    }
    if (!ReadObject(payload, "values", values)) {
        return false;
    }

    const KType kinematics = static_cast<KType>(kinematicsValue);
    float targetAz = kDefaultOutputValue;
    float targetAzAbs = kDefaultOutputValue;
    float targetEl = kDefaultOutputValue;
    float targetRange = kDefaultOutputValue;

    switch (kinematics) {
        case KType::Type3D_CARTESIAN_KINEMATICS:
        case KType::Type3D_CARTESIAN_POSITION: {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            if (!ReadNumber(*values, "x", x)) {
                return false;
            }
            if (!ReadNumber(*values, "y", y)) {
                return false;
            }
            if (!ReadNumber(*values, "z", z)) {
                return false;
            }

            cueing::cartesian2target(
                x,
                y,
                z,
                targetAz,
                targetEl,
                targetRange,
                true,
                targetAzAbs,
                crpX,
                crpY,
                crpZ,
                headingDeg);
            break;
        }

        case KType::Type2D_CARTESIAN_KINEMATICS:
        case KType::Type2D_CARTESIAN_POSITION: {
            float x = 0.0f;
            float y = 0.0f;

            if (!ReadNumber(*values, "x", x)) {
                return false;
            }
            if (!ReadNumber(*values, "y", y)) {
                return false;
            }

            cueing::cartesian2target(
                x,
                y,
                0.0f,
                targetAz,
                targetEl,
                targetRange,
                true,
                targetAzAbs,
                crpX,
                crpY,
                crpZ,
                headingDeg);
            break;
        }

        case KType::Type2D_POLAR_KINEMATICS:
        case KType::Type2D_POLAR_POSITION: {
            float trueBearing = 0.0f;
            float angleOfSight = 0.0f;

            if (!ReadNumber(*values, "trueBearing", trueBearing)) {
                return false;
            }
            if (!ReadNumber(*values, "angleOfSight", angleOfSight)) {
                return false;
            }

            cueing::Polar2D(trueBearing, angleOfSight, targetAz, targetEl, true, targetAzAbs, headingDeg);
            targetRange = maxLaserRange - 1.0f;
            break;
        }

        case KType::Type2D_POLAR_SURFACE_KINEMATICS:
        case KType::Type2D_POLAR_SURFACE_POSITION: {
            float trueBearing = 0.0f;
            float range = 0.0f;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;

            if (!ReadNumber(*values, "trueBearing", trueBearing)) {
                return false;
            }
            if (!ReadNumber(*values, "range", range)) {
                return false;
            }

            cueing::polar2catesian(trueBearing, range, x, y, z, true, headingDeg);
            cueing::cartesian2target(
                x,
                y,
                z,
                targetAz,
                targetEl,
                targetRange,
                true,
                targetAzAbs,
                crpX,
                crpY,
                crpZ,
                headingDeg);
            targetRange = maxLaserRange;
            break;
        }

        case KType::Type1D_POLAR_POSITION: {
            float trueBearing = 0.0f;

            if (!ReadNumber(*values, "trueBearing", trueBearing)) {
                return false;
            }

            targetAz = cueing::rad2deg(trueBearing);
            targetAzAbs = targetAz;
            targetEl = 0.0f;
            targetRange = maxLaserRange;
            break;
        }

        case KType::TypeEW_1D_POLAR_POSITION:
        case KType::TypeEW_2D_POLAR_POSITION:
        default:
            return false;
    }

    if (output != nullptr) {
        WriteOutput(*output, targetAz, targetAzAbs, targetEl, targetRange);
    }
    return true;
}