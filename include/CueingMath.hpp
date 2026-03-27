#pragma once

namespace cueing {

float mod360(float angleDeg);
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

} // namespace cueing
