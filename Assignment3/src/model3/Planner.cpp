// a3-plan: writes trajectory.txt and keyframes.txt for the launch.
//
// It flies the ascent with the flight model, fits a Bezier curve to the
// path of every part (least squares, end points fixed) and samples the
// keyframes. a3-scene then animates from these two files only.
//
// Usage: a3-plan [--overwrite]   (run in the Assignment3 folder)
// Existing files are only replaced with --overwrite (old ones kept as .bak).

#include "Bezier.h"
#include "Flight.h"
#include "Keyframes.h"
#include "SceneConfig.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

struct Sample { double t; glm::dvec3 p; };

static double bernstein(int n, int i, double u)
{
    double c = 1.0;
    for (int k = 1; k <= i; ++k) c = c * (n - i + k) / k;
    return c * pow(u, i) * pow(1.0 - u, n - i);
}

// Least-squares Bezier fit of the given degree with fixed end points. The
// parameter of each sample is its normalised time.
static ControlPoints fitBezier(const vector<Sample>& s, int degree)
{
    double t0 = s.front().t, t1 = s.back().t;
    int n = degree - 1; // unknown inner control points
    vector<vector<double>> A(n, vector<double>(n + 3, 0.0)); // [normal matrix | rhs x y z]
    for (const Sample& sm : s)
    {
        double u = (sm.t - t0) / (t1 - t0);
        glm::dvec3 r = sm.p - bernstein(degree, 0, u) * s.front().p - bernstein(degree, degree, u) * s.back().p;
        for (int i = 0; i < n; ++i)
        {
            double bi = bernstein(degree, i + 1, u);
            for (int j = 0; j < n; ++j) A[i][j] += bi * bernstein(degree, j + 1, u);
            for (int k = 0; k < 3; ++k) A[i][n + k] += bi * r[k];
        }
    }
    // Gaussian elimination (small symmetric positive definite system).
    for (int c = 0; c < n; ++c)
        for (int r = c + 1; r < n; ++r)
        {
            double f = A[r][c] / A[c][c];
            for (int k = c; k < n + 3; ++k) A[r][k] -= f * A[c][k];
        }
    vector<glm::dvec3> inner(n);
    for (int r = n - 1; r >= 0; --r)
        for (int k = 0; k < 3; ++k)
        {
            double v = A[r][n + k];
            for (int c = r + 1; c < n; ++c) v -= A[r][c] * inner[c][k];
            inner[r][k] = v / A[r][r];
        }
    ControlPoints ctrl = {glm::vec3(s.front().p)};
    for (const glm::dvec3& p : inner) ctrl.push_back(glm::vec3(p));
    ctrl.push_back(glm::vec3(s.back().p));
    return ctrl;
}

// Raises the degree until the error is below tol (world units).
static ControlPoints fitPath(const char* name, const vector<Sample>& s, double tol)
{
    ControlPoints best;
    double bestErr = 1e30;
    for (int degree = 3; degree <= 22 && bestErr > tol; ++degree)
    {
        ControlPoints c = fitBezier(s, degree);
        double err = 0.0;
        for (const Sample& sm : s)
        {
            float u = float((sm.t - s.front().t) / (s.back().t - s.front().t));
            err = max(err, glm::length(glm::dvec3(evaluateBezier(c, u)) - sm.p));
        }
        if (err < bestErr) { bestErr = err; best = c; }
    }
    printf("  %-14s %6.1f .. %6.1f s  %2zu control points  max error %.3f model units\n",
           name, s.front().t, s.back().t, best.size(), bestErr / ROCKET_SCALE);
    return best;
}

int main(int argc, char** argv)
{
    bool overwrite = argc > 1 && string(argv[1]) == "--overwrite";
    for (const char* f : {"trajectory.txt", "keyframes.txt"})
        if (filesystem::exists(f))
        {
            if (!overwrite) { cerr << f << " exists; run with --overwrite to replace it\n"; return 1; }
            filesystem::rename(f, string(f) + ".bak");
        }

    // ---- Fly the mission, recording every 0.1 s ----
    glm::dvec3 siteUp = glm::dvec3(surfaceDirection(SITE_LATITUDE, SITE_LONGITUDE));
    Flight flight;
    flight.reset(siteUp, LAUNCH_AZIMUTH);
    flight.launch();

    struct Record { double t; glm::dvec3 vehicle; double tilt; vector<glm::dvec3> offsets; glm::dvec3 payloadOffset; };
    vector<Record> rec;
    vector<double> events; // boosters, PS1, fairing, PS2, PS3, Mangalyaan
    size_t debrisSeen = 0;
    double tEnd = 1e9;

    for (int step = 0; flight.missionTime < tEnd; ++step)
    {
        if (step > 0) flight.update(0.02);
        if (flight.debris.size() != debrisSeen) { events.push_back(flight.missionTime); debrisSeen = flight.debris.size(); }
        if (flight.satelliteReleased && events.size() == 5) { events.push_back(flight.missionTime); tEnd = flight.missionTime + 40.0; }
        if (flight.missionTime > 4000) { cerr << "flight did not reach orbit\n"; return 1; }
        if (step % 5 != 0) continue;

        Record r;
        r.t = flight.missionTime;
        r.vehicle = flight.vehicle.pos * double(KM_TO_WORLD) + siteUp * double(PAD_TOP * ROCKET_SCALE);
        glm::dvec3 up = glm::normalize(flight.vehicle.pos);
        r.tilt = glm::degrees(acos(glm::clamp(glm::dot(flight.vehicleAxis, up), -1.0, 1.0)));
        for (const Debris& d : flight.debris) r.offsets.push_back((d.body.pos - flight.vehicle.pos) * double(SEPARATION_TO_WORLD));
        r.payloadOffset = flight.satelliteReleased ? (flight.satellite.pos - flight.vehicle.pos) * double(SEPARATION_TO_WORLD) : glm::dvec3(0.0);
        rec.push_back(r);
    }
    tEnd = rec.back().t;
    printf("Separations at T+ %.1f %.1f %.1f %.1f %.1f %.1f s\n", events[0], events[1], events[2], events[3], events[4], events[5]);

    // ---- Fit the curves ----
    printf("Fitting Bezier curves:\n");
    vector<ControlPoints> traj(TRAJ_COUNT);
    vector<double> start(TRAJ_COUNT, 0.0), length(TRAJ_COUNT, tEnd);

    vector<Sample> s;
    for (const Record& r : rec) s.push_back({r.t, r.vehicle});
    traj[TRAJ_VEHICLE] = fitPath("vehicle", s, 0.1 * ROCKET_SCALE);
    auto vehicleCurve = [&](double t) { return glm::dvec3(evaluateBezier(traj[TRAJ_VEHICLE], float(t / tEnd))); };

    // Separated parts start on the fitted vehicle curve, then drift away.
    // Debris are created in order: 4 boosters, PS1, 2 fairing halves, PS2, PS3.
    struct Part { TrajectoryId id; const char* name; vector<int> debris; int event; };
    vector<Part> parts = {{TRAJ_BOOSTERS, "boosters", {0, 1, 2, 3}, 0}, {TRAJ_STAGE1, "PS1", {4}, 1},
                          {TRAJ_FAIRING, "fairing", {5, 6}, 2}, {TRAJ_STAGE2, "PS2", {7}, 3},
                          {TRAJ_STAGE3, "PS3", {8}, 4}};
    for (const Part& part : parts)
    {
        double t0 = events[part.event];
        s.clear();
        for (const Record& r : rec)
        {
            if (r.t < t0 - 1e-6 || r.t > t0 + DEBRIS_WINDOW_S + 1e-6) continue;
            glm::dvec3 off(0.0);
            for (int i : part.debris) off += r.offsets[i];
            s.push_back({r.t, vehicleCurve(r.t) + off / double(part.debris.size())});
        }
        traj[part.id] = fitPath(part.name, s, 0.3 * ROCKET_SCALE);
        start[part.id] = s.front().t;
        length[part.id] = s.back().t - s.front().t;
    }
    s.clear();
    for (const Record& r : rec)
        if (r.t >= events[5] - 1e-6) s.push_back({r.t, vehicleCurve(r.t) + r.payloadOffset});
    traj[TRAJ_PAYLOAD] = fitPath("Mangalyaan", s, 0.1 * ROCKET_SCALE);
    start[TRAJ_PAYLOAD] = s.front().t;
    length[TRAJ_PAYLOAD] = tEnd - s.front().t;

    // Camera: starts beside the pad, then swings round to chase the vehicle
    // (offset in the local east/north/up frame, in model units).
    auto localOffset = [](glm::dvec3 pos, double e, double n, double u) {
        glm::dvec3 up = glm::normalize(pos);
        glm::dvec3 east = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), up));
        glm::dvec3 north = glm::cross(up, east);
        return (e * east + n * north + u * up) * double(ROCKET_SCALE);
    };
    glm::dvec3 padCam = siteUp * double(EARTH_RADIUS) + localOffset(siteUp, -14.0, -27.0, 4.5);
    vector<Sample> eye, target;
    for (const Record& r : rec)
    {
        glm::dvec3 centre = vehicleCurve(r.t) + glm::normalize(r.vehicle) * double(5.0 * ROCKET_SCALE);
        centre += r.payloadOffset * 0.5; // after separation: between PS4 and Mangalyaan
        double b = glm::clamp((r.t - 5.0) / 40.0, 0.0, 1.0);
        b = b * b * (3.0 - 2.0 * b);
        glm::dvec3 chase = centre + localOffset(r.vehicle, -10.0, -18.0, 6.0);
        eye.push_back({r.t, padCam + (chase - padCam) * b});
        target.push_back({r.t, centre});
    }
    traj[TRAJ_CAMERA_EYE] = fitPath("camera eye", eye, 0.5 * ROCKET_SCALE);
    traj[TRAJ_CAMERA_TARGET] = fitPath("camera target", target, 0.3 * ROCKET_SCALE);

    for (const ControlPoints& c : traj) appendTrajectory("trajectory.txt", c);

    // ---- Keyframes, with extra ones around each separation (slow motion) ----
    vector<double> times = {5.0};
    for (double t = 15.0; t < tEnd; t += (t < 130.0 ? 10.0 : 30.0)) times.push_back(t);
    for (double e : events)
        for (double d : {0.0, 2.0, 5.0, 10.0}) times.push_back(e + d);
    times.push_back(tEnd);
    sort(times.begin(), times.end());
    times.erase(unique(times.begin(), times.end(), [](double a, double b) { return b - a < 1.5; }), times.end());
    times.insert(times.begin(), {0.0, 0.0}); // first interval holds on the pad

    vector<Keyframe> keys;
    for (size_t k = 0; k < times.size(); ++k)
    {
        double t = times[k];
        Keyframe key;
        for (int i = 0; i < TRAJ_COUNT; ++i)
        {
            bool active = t >= start[i] - 1e-6 && t <= start[i] + length[i] + 1e-6;
            bool isDebris = i >= TRAJ_BOOSTERS && i <= TRAJ_STAGE3;
            if (isDebris && t >= start[i] + length[i] - 1e-6) active = false; // gone at the window end
            key.active.push_back(active);
            key.u.push_back(float(glm::clamp((t - start[i]) / length[i], 0.0, 1.0)));
        }
        int stage = 0;
        for (double e : events) if (t >= e - 1e-6) ++stage;
        const Record* r = &rec.front();
        for (const Record& rr : rec) { if (rr.t > t + 1e-6) break; r = &rr; }
        bool engine = k > 0 && t < flight.cutoffTime;
        key.extra = {float(stage), float(engine), k == 0 ? 0.0f : float(r->tilt)};
        keys.push_back(key);
    }
    saveKeyframes("keyframes.txt", keys,
                  "# a3-scene keyframes (written by a3-plan). Each row: 'active u' for the 9\n"
                  "# trajectories (vehicle, boosters, PS1, fairing, PS2, PS3, Mangalyaan,\n"
                  "# camera eye, camera target), then: stage engine tilt(deg)\n");
    printf("Wrote %d trajectories and %zu keyframes\n", int(TRAJ_COUNT), keys.size());
    return 0;
}
