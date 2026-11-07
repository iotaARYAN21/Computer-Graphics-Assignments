#ifndef FLIGHT_H
#define FLIGHT_H

#include <glm/glm.hpp>

#include <string>
#include <vector>

// Simple point-mass flight model for the PSLV-C25 launch.
// Units: km, km/s, seconds. All positions are in the Earth-fixed frame
// (Earth centre at the origin, +Y = north pole), so the rendering code only
// has to apply Earth's rotation on top.

// Forces:
//   - gravity, mu / r^2 towards Earth's centre (always)
//   - thrust of the currently burning stage (while burning)
//   - aerodynamic drag with an exponential atmosphere, ONLY below the
//     Karman line (100 km). Above it the motion is pure gravity / thrust.

const double EARTH_RADIUS_KM = 6371.0;
const double EARTH_MU = 398600.4;          // km^3 / s^2
const double ATMOSPHERE_TOP_KM = 100.0;    // no drag above this altitude
const double TARGET_ALTITUDE_KM = 250.0;   // parking orbit perigee (approx.)

struct Body
{
    glm::dvec3 pos{0.0};
    glm::dvec3 vel{0.0};
};

double altitudeKm(const glm::dvec3& pos);

// Advances a body by dt seconds. thrustAccel is in m/s^2 along thrustDir.
// dragFactor = Cd * A / (2 m) in m^2/kg.
void stepBody(Body& body, double thrustAccel, const glm::dvec3& thrustDir,
              double dragFactor, double dt);

enum Phase
{
    PHASE_PS1_STRAPONS = 0, // PS1 + strap-on boosters
    PHASE_PS1,              // PS1 alone
    PHASE_PS2,
    PHASE_PS3,
    PHASE_PS4,
    PHASE_COAST             // PS4 burnt out, vehicle in orbit
};

enum DebrisKind
{
    DEBRIS_BOOSTER = 0,
    DEBRIS_STAGE1,
    DEBRIS_STAGE2,
    DEBRIS_STAGE3,
    DEBRIS_FAIRING_A,
    DEBRIS_FAIRING_B
};

// A separated component. It keeps the vehicle orientation it had at the
// moment of separation and then moves on its own (gravity + drag only).
struct Debris
{
    DebrisKind kind;
    int index;           // booster number, unused for the others
    Body body;
    glm::mat3 frame;     // vehicle axes at separation (Earth-fixed)
    double releaseTime;
    double dragFactor;
    bool active;
};

class Flight
{
public:
    void reset(const glm::dvec3& siteDirection, double launchAzimuthDeg);
    void launch();
    void update(double missionSeconds);

    // Vehicle axes in the Earth-fixed frame: columns are x, y (long axis), z.
    glm::mat3 vehicleFrame() const;
    glm::mat3 frameForAxis(const glm::dvec3& axis) const;

    bool launched = false;
    double missionTime = 0.0;
    Phase phase = PHASE_PS1_STRAPONS;

    Body vehicle;
    glm::dvec3 vehicleAxis{0.0, 1.0, 0.0};

    bool fairingSeparated = false;
    double cutoffTime = -1.0;

    bool satelliteReleased = false;
    double satelliteReleaseTime = 0.0;
    Body satellite;
    glm::mat3 satelliteFrame{1.0f};

    double lastEventTime = -100.0;
    std::vector<Debris> debris;
    std::vector<std::string> messages; // event log, consumed by the caller

private:
    void step(double dt);
    glm::dvec3 guidanceDirection() const;
    void checkEvents();
    void addDebris(DebrisKind kind, int index, const glm::dvec3& kick,
                   double dragFactor);
    void logEvent(const std::string& text);

    double timeAccumulator = 0.0;
    glm::dvec3 launchHeading{1.0, 0.0, 0.0};
    glm::dvec3 trajectoryNormal{0.0, 0.0, 1.0};
};

#endif
