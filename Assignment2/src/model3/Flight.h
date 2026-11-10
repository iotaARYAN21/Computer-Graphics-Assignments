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

// =====================================================================
// HOW THE FULL-SCENE ANIMATION WORKS (a2-scene)  -- overview
// =====================================================================
// Model 2 used "time -> fixed path" animation. The full scene instead
// uses a small PHYSICS SIMULATION:
//   - every object (rocket, booster, stage, fairing half, satellite) is
//     a Body with a position and a velocity
//   - every 0.02 s of simulated time we compute the forces on it
//     (gravity, engine thrust, air drag) and update velocity/position
//   - when the rocket reaches certain altitudes, a part is "dropped":
//     a copy of the rocket's Body becomes a Debris that now flies on
//     its own with no engine, so it falls behind and drops away
// main.cpp then only READS these positions every frame and draws
// the meshes there. So: Flight.cpp = "where is everything",
// main.cpp = "draw everything where Flight says".
// =====================================================================

// Body: anything that moves. pos in km, vel in km/s (Earth-fixed frame).
struct Body
{
    glm::dvec3 pos{0.0};
    glm::dvec3 vel{0.0};
};

// altitudeKm: height above Earth's surface = distance from centre - radius.
double altitudeKm(const glm::dvec3& pos);

// Advances a body by dt seconds. thrustAccel is in m/s^2 along thrustDir.
// dragFactor = Cd * A / (2 m) in m^2/kg.
void stepBody(Body& body, double thrustAccel, const glm::dvec3& thrustDir,
              double dragFactor, double dt);

// Phase: which stage is currently firing. main.cpp uses this to decide
// which stages are still attached (drawn on the rocket) and which
// exhaust flame to show.
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

// Flight: the whole mission state + the simulation.
class Flight
{
public:
    // reset: put the rocket back on the launch pad, standing upright.
    void reset(const glm::dvec3& siteDirection, double launchAzimuthDeg);
    // launch: start the clock (L key). Before this, update() does nothing.
    void launch();
    // update: advance the simulation by "missionSeconds" (called each frame).
    void update(double missionSeconds);

    // Vehicle axes in the Earth-fixed frame: columns are x, y (long axis), z.
    // This is the ROTATION of the rocket; main.cpp puts it into the model matrix.
    glm::mat3 vehicleFrame() const;
    // frameForAxis: build a full rotation (x, y, z axes) from just the
    // direction the rocket's nose points.
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
    // step: one small 0.02 s physics step for every body.
    void step(double dt);
    // guidanceDirection: which way the rocket should point right now.
    glm::dvec3 guidanceDirection() const;
    // checkEvents: drop boosters/stages/fairing at the right altitudes.
    void checkEvents();
    // addDebris: create a separated part as a copy of the rocket's state.
    void addDebris(DebrisKind kind, int index, const glm::dvec3& kick,
                   double dragFactor);
    // logEvent: write a line like "T+ 110 s alt 58 km ..." to the console.
    void logEvent(const std::string& text);

    double timeAccumulator = 0.0;
    glm::dvec3 launchHeading{1.0, 0.0, 0.0};
    glm::dvec3 trajectoryNormal{0.0, 0.0, 1.0};
};

#endif
