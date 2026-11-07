#include "Flight.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
const double TIME_STEP = 0.02; // s, fixed integration step

// Thrust acceleration (m/s^2) for each phase. Tuned so that the event
// times come out close to the PSLV-C25 flight timeline.
const double PHASE_THRUST[] = {20.0, 16.0, 13.0, 16.0, 7.0, 0.0};

// Altitudes (km) at which each spent stage is dropped. Taken from the
// published PSLV-C25 flight profile (rounded).
const double STRAPON_SEPARATION_KM = 25.0;
const double PS1_SEPARATION_KM = 58.0;
const double FAIRING_SEPARATION_KM = 115.0;
const double PS2_SEPARATION_KM = 135.0;
const double PS3_SEPARATION_KM = 195.0;

// Spacecraft separation happens a short while after PS4 cut-off.
const double SATELLITE_DELAY_S = 20.0;

// Drag factors Cd * A / (2 m) in m^2/kg.
const double VEHICLE_DRAG = 8.3e-6;
const double BOOSTER_DRAG = 2.0e-4;
const double STAGE_DRAG = 1.2e-4;
const double FAIRING_DRAG = 4.0e-4;
}

double altitudeKm(const glm::dvec3& pos)
{
    return glm::length(pos) - EARTH_RADIUS_KM;
}

void stepBody(Body& body, double thrustAccel, const glm::dvec3& thrustDir,
              double dragFactor, double dt)
{
    double r = glm::length(body.pos);
    glm::dvec3 up = body.pos / r;

    // Gravity always acts.
    glm::dvec3 acc = -(EARTH_MU / (r * r)) * up;

    // Thrust, converted from m/s^2 to km/s^2.
    acc += thrustAccel * 1e-3 * thrustDir;

    // Drag only inside the atmosphere.
    double h = r - EARTH_RADIUS_KM;
    double speed = glm::length(body.vel);
    if (h < ATMOSPHERE_TOP_KM && speed > 1e-9)
    {
        double density = 1.225 * std::exp(-std::max(h, 0.0) / 8.5); // kg/m^3
        double speedMs = speed * 1000.0;
        double dragAccel = dragFactor * density * speedMs * speedMs * 1e-3;
        acc -= dragAccel * (body.vel / speed);
    }

    // Semi-implicit Euler.
    body.vel += acc * dt;
    body.pos += body.vel * dt;
}

void Flight::reset(const glm::dvec3& siteDirection, double launchAzimuthDeg)
{
    *this = Flight();

    glm::dvec3 up = glm::normalize(siteDirection);
    glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0.0, 1.0, 0.0), up));
    glm::dvec3 north = glm::cross(up, east);

    double az = glm::radians(launchAzimuthDeg);
    launchHeading = std::cos(az) * north + std::sin(az) * east;
    trajectoryNormal = glm::normalize(glm::cross(up, launchHeading));

    vehicle.pos = up * EARTH_RADIUS_KM;
    vehicle.vel = glm::dvec3(0.0);
    vehicleAxis = up;
}

void Flight::launch()
{
    if (launched)
        return;
    launched = true;
    logEvent("Lift-off from Sriharikota");
}

void Flight::update(double missionSeconds)
{
    if (!launched)
        return;

    // Fixed time step; leftover time is carried over to the next frame.
    timeAccumulator = std::min(timeAccumulator + missionSeconds, 100.0);
    while (timeAccumulator >= TIME_STEP)
    {
        step(TIME_STEP);
        timeAccumulator -= TIME_STEP;
    }
}

glm::dvec3 Flight::guidanceDirection() const
{
    double r = glm::length(vehicle.pos);
    double h = r - EARTH_RADIUS_KM;
    glm::dvec3 up = vehicle.pos / r;

    // Horizontal direction of flight: along the horizontal velocity once we
    // have some, otherwise the planned launch heading.
    glm::dvec3 horizontalVel = vehicle.vel - glm::dot(vehicle.vel, up) * up;
    glm::dvec3 horizontal;
    if (glm::length(horizontalVel) > 0.05)
        horizontal = glm::normalize(horizontalVel);
    else
        horizontal = glm::normalize(launchHeading - glm::dot(launchHeading, up) * up);

    // Gravity turn: tilt from vertical grows with altitude.
    double tilt = 90.0 * std::sqrt(glm::clamp(h / 180.0, 0.0, 1.0));

    // Keep climbing if the vertical speed is not enough to coast up to the
    // target altitude.
    double g = EARTH_MU / (r * r);
    double verticalSpeed = glm::dot(vehicle.vel, up);
    double neededSpeed = std::sqrt(2.0 * g * std::max(TARGET_ALTITUDE_KM - h, 0.0));
    if (neededSpeed > 1e-6 && verticalSpeed < neededSpeed)
        tilt = std::min(tilt, 90.0 - 35.0 * (1.0 - verticalSpeed / neededSpeed));
    tilt = glm::clamp(tilt, 0.0, 90.0);

    double t = glm::radians(tilt);
    return glm::normalize(std::cos(t) * up + std::sin(t) * horizontal);
}

void Flight::step(double dt)
{
    missionTime += dt;

    double thrust = PHASE_THRUST[phase];
    if (thrust > 0.0)
        vehicleAxis = guidanceDirection();
    else if (glm::length(vehicle.vel) > 1e-6)
        vehicleAxis = glm::normalize(vehicle.vel); // coast: point prograde

    stepBody(vehicle, thrust, vehicleAxis, VEHICLE_DRAG, dt);

    for (Debris& d : debris)
    {
        if (!d.active)
            continue;
        stepBody(d.body, 0.0, glm::dvec3(0.0), d.dragFactor, dt);
        if (altitudeKm(d.body.pos) < 0.0)
            d.active = false; // fell back to Earth (or into the sea)
    }

    if (satelliteReleased)
        stepBody(satellite, 0.0, glm::dvec3(0.0), 0.0, dt);

    checkEvents();
}

void Flight::checkEvents()
{
    double h = altitudeKm(vehicle.pos);
    glm::mat3 frame = vehicleFrame();
    glm::dvec3 axis = vehicleAxis;
    glm::dvec3 sideZ = glm::dvec3(frame[2]);

    if (phase == PHASE_PS1_STRAPONS && h >= STRAPON_SEPARATION_KM)
    {
        for (int i = 0; i < 4; ++i)
        {
            double a = glm::radians(45.0 + 90.0 * i);
            glm::dvec3 outward = glm::dvec3(frame * glm::vec3(std::cos(a), 0.0, std::sin(a)));
            addDebris(DEBRIS_BOOSTER, i, outward * 0.012, BOOSTER_DRAG);
        }
        phase = PHASE_PS1;
        logEvent("Strap-on booster separation");
    }
    else if (phase == PHASE_PS1 && h >= PS1_SEPARATION_KM)
    {
        addDebris(DEBRIS_STAGE1, 0, -axis * 0.002, STAGE_DRAG);
        phase = PHASE_PS2;
        logEvent("PS1 separation, PS2 ignition");
    }
    else if (phase == PHASE_PS2 && h >= PS2_SEPARATION_KM)
    {
        addDebris(DEBRIS_STAGE2, 0, -axis * 0.002, STAGE_DRAG);
        phase = PHASE_PS3;
        logEvent("PS2 separation, PS3 ignition");
    }
    else if (phase == PHASE_PS3 && h >= PS3_SEPARATION_KM)
    {
        addDebris(DEBRIS_STAGE3, 0, -axis * 0.002, STAGE_DRAG);
        phase = PHASE_PS4;
        logEvent("PS3 separation, PS4 ignition");
    }
    else if (phase == PHASE_PS4)
    {
        // Cut off PS4 once the horizontal speed reaches orbital speed.
        double r = glm::length(vehicle.pos);
        glm::dvec3 up = vehicle.pos / r;
        glm::dvec3 horizontalVel = vehicle.vel - glm::dot(vehicle.vel, up) * up;
        if (glm::length(horizontalVel) >= std::sqrt(EARTH_MU / r))
        {
            phase = PHASE_COAST;
            cutoffTime = missionTime;
            logEvent("PS4 cut-off, orbit reached");
        }
    }

    if (!fairingSeparated && h >= FAIRING_SEPARATION_KM)
    {
        // Above the dense atmosphere the heat shield is no longer needed.
        addDebris(DEBRIS_FAIRING_A, 0, sideZ * 0.006, FAIRING_DRAG);
        addDebris(DEBRIS_FAIRING_B, 0, -sideZ * 0.006, FAIRING_DRAG);
        fairingSeparated = true;
        logEvent("Payload fairing separation");
    }

    if (phase == PHASE_COAST && !satelliteReleased &&
        missionTime >= cutoffTime + SATELLITE_DELAY_S)
    {
        satellite = vehicle;
        satellite.vel += axis * 0.0008; // gentle spring push, ~0.8 m/s
        satelliteFrame = frame;
        satelliteReleased = true;
        satelliteReleaseTime = missionTime;
        logEvent("Mangalyaan separation from PS4");
    }
}

void Flight::addDebris(DebrisKind kind, int index, const glm::dvec3& kick,
                       double dragFactor)
{
    Debris d;
    d.kind = kind;
    d.index = index;
    d.body = vehicle;
    d.body.vel += kick;
    d.frame = vehicleFrame();
    d.releaseTime = missionTime;
    d.dragFactor = dragFactor;
    d.active = true;
    debris.push_back(d);
}

void Flight::logEvent(const std::string& text)
{
    char line[160];
    std::snprintf(line, sizeof(line), "T+%6.1f s  alt %6.1f km  v %5.2f km/s  %s",
                  missionTime, altitudeKm(vehicle.pos),
                  glm::length(vehicle.vel), text.c_str());
    messages.push_back(line);
    lastEventTime = missionTime;
}

glm::mat3 Flight::frameForAxis(const glm::dvec3& axis) const
{
    glm::dvec3 y = glm::normalize(axis);
    glm::dvec3 x = trajectoryNormal - glm::dot(trajectoryNormal, y) * y;
    x = glm::normalize(x);
    glm::dvec3 z = glm::cross(x, y);
    return glm::mat3(glm::vec3(x), glm::vec3(y), glm::vec3(z));
}

glm::mat3 Flight::vehicleFrame() const
{
    return frameForAxis(vehicleAxis);
}
