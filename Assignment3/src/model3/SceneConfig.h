#ifndef SCENE_CONFIG_H
#define SCENE_CONFIG_H

// Constants shared by a3-scene (rendering / playback) and a3-plan (the
// trajectory planner), plus the meaning of every trajectory and keyframe
// field.

#include "Flight.h"

#include <glm/glm.hpp>

#include <cmath>

// Scene scale
// Earth is drawn with radius 30. At true scale the 44 m tall PSLV would be
// invisible, so the vehicle (and everything near it) is drawn about 140x
// larger than real. Paths come from the flight model in real kilometres.
const float EARTH_RADIUS = 30.0f;
const float KM_TO_WORLD = EARTH_RADIUS / static_cast<float>(EARTH_RADIUS_KM);
const float ROCKET_SCALE = 0.003f;                 // model units -> world units
const float MODEL_UNITS_PER_METRE = 9.83f / 44.4f; // model height / real height
const float PAD_TOP = 0.41f;                       // pad height in model units

// Separated parts are placed relative to the vehicle with the same
// magnification as the vehicle, so separations look right next to it.
const float SEPARATION_TO_WORLD = 1000.0f * MODEL_UNITS_PER_METRE * ROCKET_SCALE;
const float DEBRIS_HIDE_DISTANCE = 150.0f; // model units

//       
// Mission data: PSLV-C25 / Mars Orbiter Mission (Mangalyaan)
// Launch: 5 November 2013, 09:08 UTC (14:38 IST), First Launch Pad,
// Satish Dhawan Space Centre, Sriharikota.
//       
const float SITE_LATITUDE = 13.72f;
const float SITE_LONGITUDE = 80.23f;
const double LAUNCH_AZIMUTH = 104.0; // degrees from north, over the Bay of Bengal

// Directions of the Sun and Mars at launch time, written as the point on
// Earth where each one is overhead (latitude = declination, longitude =
// right ascension - Greenwich sidereal time). Computed from the JPL
// approximate planetary elements for 2013-11-05 09:08 UTC:
//   Sun : RA 220.5 deg, Dec -15.7 deg -> Sun in the south-west at
//         Sriharikota, about 40 deg high (afternoon)
//   Mars: RA 164.1 deg, Dec  +8.5 deg, 1.89 AU from Earth (in Leo, a
//         morning planet, below the horizon at launch time)
const float SUN_LATITUDE = -15.72f;
const float SUN_LONGITUDE = 38.71f;
const float MARS_LATITUDE = 8.51f;
const float MARS_LONGITUDE = -17.69f;

// Mars and the Sun are drawn in the correct directions but far closer
// than real. Mars has its real size relative to Earth (0.53 R_earth); the
// Sun is shrunk to a small disc.
const float MARS_DISTANCE = 800.0f;
const float MARS_RADIUS = 0.532f * EARTH_RADIUS;
const float SUN_DISTANCE = 1500.0f;
const float SUN_RADIUS = 20.0f;
const float MARS_ROTATION_DEG_PER_S = 360.0f / 88643.0f; // sidereal sol

const float EARTH_ROTATION_DEG_PER_S = 360.0f / 86164.0f; // sidereal day

// Unit vector from Earth's centre to a latitude/longitude, matching the
// texture mapping of the Earth sphere: u = 1 - phi / 2pi and the
// equirectangular texture has longitude -180 at u = 0, so phi = 180 - lon.
inline glm::vec3 surfaceDirection(float lat, float lon)
{
    float latRad = glm::radians(lat);
    float phi = glm::radians(180.0f - lon);
    return glm::vec3(std::cos(latRad) * std::cos(phi), std::sin(latRad),
                     std::cos(latRad) * std::sin(phi));
}

       
// trajectory.txt: which block is which object
// Coordinates are world units (Earth radius 30, Earth centre at the origin).
// Each separated part's path starts on the vehicle path at its separation,
// and each of Mangalyaan's later paths starts where the previous one ends.
enum TrajectoryId
{
    TRAJ_VEHICLE = 0, // launch vehicle (whatever is still attached)
    TRAJ_BOOSTERS,    // the four strap-ons, drawn around this path
    TRAJ_STAGE1,
    TRAJ_FAIRING,     // both halves, drawn either side of this path
    TRAJ_STAGE2,
    TRAJ_STAGE3,
    TRAJ_PAYLOAD,     // Mangalyaan after separation
    TRAJ_CAMERA_EYE,
    TRAJ_CAMERA_TARGET,
    // After the launch: Mangalyaan's own path, one curve per mission phase,
    // each followed by its own camera eye / target pair.
    TRAJ_ORBIT_RAISING, // growing Earth orbits, burns at each perigee
    TRAJ_CRUISE,        // Trans-Mars Injection -> Mars
    TRAJ_MARS_ORBIT,    // after Mars Orbit Insertion
    TRAJ_EYE_ORBIT, TRAJ_TARGET_ORBIT,
    TRAJ_EYE_CRUISE, TRAJ_TARGET_CRUISE,
    TRAJ_EYE_MARS, TRAJ_TARGET_MARS,
    TRAJ_COUNT
};

// Mangalyaan is drawn this much larger after the launch so it can still be
// seen next to Earth and Mars.
const float PAYLOAD_SPACE_SCALE = 150.0f;

// Separated parts are animated for this long after separation.
const float DEBRIS_WINDOW_S = 10.0f;

//       
// keyframes.txt: extra fields after the (active, u) pairs
//       
enum KeyframeField
{
    FIELD_STAGE = 0, // discrete: parts dropped so far, see VehicleStage
    FIELD_ENGINE,    // discrete: 1 while an engine is firing
    FIELD_TILT,      // continuous: vehicle angle from vertical (deg)
    FIELD_COUNT
};

inline bool fieldIsDiscrete(int field)
{
    return field == FIELD_STAGE || field == FIELD_ENGINE;
}

// FIELD_STAGE values: each one drops one more part.
enum VehicleStage
{
    STAGE_ON_PAD = 0,
    STAGE_NO_BOOSTERS,
    STAGE_NO_PS1,
    STAGE_NO_FAIRING,
    STAGE_NO_PS2,
    STAGE_NO_PS3,            // PS4 + Mangalyaan
    STAGE_PAYLOAD_SEPARATED  // PS4 alone
};

#endif
