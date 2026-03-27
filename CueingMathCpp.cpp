#include <cmath>

namespace cueing {

constexpr float kPi = 3.14159265358979323846f;

float mod360(float angleDeg) {
    while (angleDeg < 0.0f) {
        angleDeg += 360.0f;
    }
    while (angleDeg >= 360.0f) {
        angleDeg -= 360.0f;
    }
    return angleDeg;
}

float rad2deg(float angleRad) {
    return angleRad * 180.0f / kPi;
}

// Faithful port of VB cartesian2target(X,Y,Z).
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
    float heading) {

    float projectedRange;
    float tempAng;
    float goniometricAngle;

    // Correct target coordinates with CRP offset.
    x -= crpX;
    y -= crpY;
    z -= crpZ;

    // 3D distance to target.
    range = std::sqrt((x * x) + (y * y) + (z * z));

    // Azimuth in nautical convention.
    goniometricAngle = rad2deg(std::atan2(y, x));
    tempAng = mod360(90.0f - goniometricAngle);

    azAbs = tempAng;
    if (azAbs > 180.0f) {
        az = azAbs - 360.0f;
    } else if (azAbs < -180.0f) {
        azAbs = azAbs + 360.0f;
    }

    if (isTrueB) {
        az = mod360(tempAng - heading);
        if (az > 180.0f) {
            az -= 360.0f;
        } else if (az < -180.0f) {
            az += 360.0f;
        }
    } else {
        az = tempAng;
    }

    projectedRange = std::sqrt((x * x) + (y * y));
    el = rad2deg(std::atan2(z, projectedRange));
}

// Faithful port of VB Polar2D(TrueBearing, AngleofSight).
void Polar2D(
    float trueBearing,
    float angleOfSight,
    float& az,
    float& el,
    bool isTrueB,
    float& azAbs,
    float heading) {

    float tempAng = rad2deg(trueBearing);
    azAbs = tempAng;

    if (isTrueB) {
        az = mod360(tempAng - heading);
        if (az > 180.0f) {
            az -= 360.0f;
        } else if (az < -180.0f) {
            az += 360.0f;
        }
    } else {
        az = tempAng;
    }

    el = rad2deg(angleOfSight);
}

// Faithful port of VB polar2catesian(bearing, range).
void polar2catesian(
    float bearing,
    float range,
    float& x,
    float& y,
    float& z,
    bool isTrueB,
    float heading) {

    float tempAng;
    float relBearing;
    float goniometricAngle;

    // Surface target: no elevation.
    z = 0.0f;

    if (isTrueB) {
        relBearing = mod360(bearing - heading);
    } else {
        relBearing = bearing;
    }

    // relBearing is kept for consistency with the original VB logic.
    (void)relBearing;

    tempAng = 90.0f - bearing;

    if (tempAng < 0.0f) {
        goniometricAngle = (2.0f * kPi) + tempAng;
    } else if (tempAng >= (2.0f * kPi)) {
        goniometricAngle = tempAng - (2.0f * kPi);
    } else {
        goniometricAngle = tempAng;
    }

    x = range * std::cos(goniometricAngle);
    y = range * std::sin(goniometricAngle);
}

} // namespace cueing
