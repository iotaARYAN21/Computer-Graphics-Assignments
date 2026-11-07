# CS5024 Assignment 2 – PSLV-C25 / Mars Orbiter Mission (Mangalyaan)

We recreate the launch of **PSLV-C25** carrying the **Mars Orbiter Mission
(Mangalyaan)** spacecraft, launched on **5 November 2013 at 14:38 IST
(09:08 UTC)** from the First Launch Pad, Satish Dhawan Space Centre,
Sriharikota (13.72° N, 80.23° E).

## Building

Requirements: a C++17 compiler, CMake ≥ 3.10, OpenGL 3.3, GLEW and GLFW 3.

On Ubuntu/Debian:

```bash
sudo apt install build-essential cmake libglew-dev libglfw3-dev
```

GLM is expected in `glm/` inside this folder (header-only; we used the
1.1.0 development version, any 1.x release works). If it is missing:

```bash
git clone --depth 1 https://github.com/g-truc/glm.git glm
```

Build:

```bash
cd Assignment2
mkdir -p build && cd build
cmake ..
make
```

## Running

The programs load `shaders/` and `textures/` with relative paths, so
**run them from the `Assignment2` folder**:

```bash
cd Assignment2
./build/a2-model-0
./build/a2-model-1
./build/a2-model-2
./build/a2-scene
```

`textures/groundTexture.jpg` is used as the launch-site ground texture. If it is
missing, a plain green ground is drawn and a note is printed.

## Executables and controls

### a2-model-0: Earth viewer
Textured Earth, atmosphere glow and a star background. The camera always
looks along −Z and is moved by translation only.

| Key | Action |
|---|---|
| A / D | rotate Earth about its axis |
| ← / → | move camera along X |
| ↑ / ↓ | move camera along Y |
| [ / ] | move camera along Z (towards / away from Earth) |
| Esc | exit |

The movement speed scales with the height above the surface, so you can
fly from a distant space view down to just above the ground.

### a2-model-1: Launch-site viewer
Launch platform, pedestal, lightning masts and a lattice service tower on a
textured ground plane.

| Key | Action |
|---|---|
| Arrow keys | orbit camera |
| [ / ] | zoom in / out |
| Esc | exit |

### a2-model-2: Vehicle and payload viewer
PSLV with the four main stages (PS1 to PS4), strap-on boosters, a two-piece
payload fairing and the Mangalyaan spacecraft.

| Key | Action |
|---|---|
| Arrow keys, [ / ] | orbit and zoom camera |
| B | separate strap-on boosters |
| F | separate payload fairing (reveals Mangalyaan) |
| S | sequential stage separation (after F) |
| D | deploy Mangalyaan (after S) |
| Esc | exit |

### a2-scene: Integrated launch
Earth, atmosphere, stars, the Sun, Mars, the launch site at Sriharikota
and the vehicle in one scene.

| Key | Action |
|---|---|
| L | lift-off |
| C | cycle cameras: Launch site → Vehicle → Planets → Earth view (starts on Launch site) |
| Arrow keys | orbit the current camera |
| [ / ] | zoom the current camera |
| + / − | time warp (1×, 2×, 5×, 10×, 20×, 50×) |
| R | reset to the pad |
| Esc | exit |

The window title shows mission time, altitude, speed, the current stage,
the time warp and the active camera. Events are printed in the terminal.

**Cameras**
- *Earth view*: distant camera around Earth. In this view the flight path
  is drawn as a thin line, so the whole launch can be followed.
- *Launch site*: fixed near the pad, always aimed at the vehicle.
- *Vehicle*: moves with the launch vehicle (and with Mangalyaan after
  separation).
- *Planets*: an extra camera, beyond the three required ones. It sits just
  behind Earth on the side away from Mars and looks out towards Mars, so
  Earth and Mars are both in view. Mars was about 98° away from India in
  the sky on launch day, so no single camera can show both India and
  Mars. That is why this view is separate.

**Motion.** `src/model3/Flight.cpp` integrates a point-mass model in real
units (km, km/s) with a fixed 0.02 s step:
- gravity `μ/r²` always acts;
- stage thrust acts while a stage is burning, with a gravity-turn pitch
  program;
- aerodynamic drag (exponential atmosphere, scale height 8.5 km) acts only
  **below 100 km**. Above that the motion is drag-free.

Separated boosters, stages and fairing halves become independent bodies
with gravity and drag only. They fall back to Earth on their own.

**Event sequence** (triggered by altitude, values from the PSLV-C25 flight
profile):

| Event | Trigger | Simulated |
|---|---|---|
| Strap-on separation | 25 km | T+73 s |
| PS1 separation / PS2 ignition | 58 km | T+121 s |
| Payload fairing separation | 115 km | T+215 s |
| PS2 separation / PS3 ignition | 135 km | T+271 s |
| PS3 separation / PS4 ignition | 195 km | T+473 s |
| PS4 cut-off | orbital speed reached | T+821 s, ~258 km |
| Mangalyaan separation | 20 s after cut-off | T+841 s |

After each event the simulation runs at 1× for a few seconds so the
separation can be seen, then returns to the selected time warp.

**Earth rotation and lighting.** Earth turns at the sidereal rate (scaled
by the time warp) during the mission. The Sun is fixed in space. Its
direction at launch time (RA 220.5°, Dec −15.7°, computed from the JPL
approximate planetary elements) puts the Sun in the south-west at about
40° elevation over Sriharikota, matching the 14:38 IST launch. The sky is
blue near the sunlit ground and fades to black space with camera
altitude.

**Celestial bodies.** Mangalyaan is a Mars mission, so the scene includes:
- *Mars*: textured sphere, lit by the Sun and slowly rotating (one sol =
  24.6 h). It is placed in its real direction on launch day (RA 164.1°,
  Dec +8.5°, in Leo, 1.89 AU away), so it is a morning planet and below
  the horizon at Sriharikota during the launch.
- *The Sun*: a self-lit disc in the same direction as the scene light,
  visible in the daytime sky from the launch-site camera.

The Moon is not needed for this mission and is not included. Mars and
the Sun are drawn in their correct directions but much closer than real
(Mars 800 units, Sun 1500 units, against an Earth radius of 30). Mars keeps
its real size relative to Earth; the Sun is shrunk to a small disc.

**Scale.** Earth has radius 30 units. At true scale the 44 m rocket would be
invisible, so the vehicle, the launch site and the relative motion of
separated parts are drawn about 140× larger. The trajectory itself uses
real distances.

## Use of Blender
No component was modelled in Blender. All geometry (cylinders, cones, cubes,
fairing halves, the Earth, Mars and Sun spheres) is generated in code under `src/models/`
and `src/common/`.

## Project layout

```
CMakeLists.txt
shaders/          GLSL shaders (Earth, atmosphere, ground, rocket/cube, stars)
textures/         earth.jpg, Mars.jpg, groundTexture.jpg
src/common/       Mesh, Starfield
src/models/       Cube, Plane, Cylinder (also cone), Fairing
src/model0/       a2-model-0
src/model1/       a2-model-1
src/model2/       a2-model-2
src/model3/       a2-scene (main.cpp + Flight.cpp flight model)
```

## References and acknowledgements
- ISRO, *PSLV-C25 / Mars Orbiter Mission* brochure and launch press kit
  (vehicle configuration, flight sequence and event altitudes; add link).
- Earth texture: `textures/earth.jpg` (equirectangular Earth image; add
  its source here).
- Mars texture: `textures/Mars.jpg` (add its source here).
- Ground texture: `textures/groundTexture.jpg` (add its source here).
- E. M. Standish, *Keplerian Elements for Approximate Positions of the
  Major Planets* (JPL), used for the Sun and Mars directions at launch.
- [stb_image](https://github.com/nothings/stb) by Sean Barrett (public domain / MIT).
- [GLM](https://github.com/g-truc/glm), [GLFW](https://www.glfw.org/),
  [GLEW](https://glew.sourceforge.net/).

## Originality declaration
We declare that the implementation and this report are our own work,
except for the third-party libraries and resources acknowledged above.

Team members: *(add names and roll numbers)*
