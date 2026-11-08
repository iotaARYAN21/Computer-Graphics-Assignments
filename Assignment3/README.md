# CS5024 Assignment 3 – PSLV-C25 / Mars Orbiter Mission (Mangalyaan)

Team members: ARYAN 11230141
              SUJIT KUMAR CHOUDHARY -112301001

This folder extends the Assignment 2 project (see `../Assignment2`) with
a Bézier trajectory editor and keyframe animation. The Assignment 2 model
viewers are kept unchanged; the integrated scene is now `a3-scene`.

We recreate the launch of **PSLV-C25** carrying the **Mars Orbiter Mission
(Mangalyaan)** spacecraft, launched on **5 November 2013 at 14:38 IST
(09:08 UTC)** from the First Launch Pad, Satish Dhawan Space Centre,
Sriharikota (13.72° N, 80.23° E).

## Building

Requirements: a C++17 compiler, CMake ≥ 3.10, OpenGL 3.3, GLEW and GLFW 3.

Build:

```bash
cd Assignment2
mkdir -p build && cd build
cmake ..
make
```

## Running

The programs load `shaders/` and `textures/` with relative paths, so
**run them from the `Assignment3` folder**:

```bash
cd Assignment3
./build/a2-model-0
./build/a2-model-1
./build/a2-model-2
./build/a3-scene
```

`trajectory.txt` and `keyframes.txt` are read from (and written to) the
folder the program is run from. They are already included; to make them
again, run `./build/a3-plan --overwrite`.

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
| S | sequential stage separation (also releases the boosters) |
| D | deploy Mangalyaan (also opens the fairing) |
| Esc | exit |

### a3-scene: Integrated scene and animation
Earth, atmosphere, stars, the Sun, Mars, the launch site at Sriharikota
and the vehicle in one scene. The launch is played from `trajectory.txt`
and `keyframes.txt` (see *Assignment 3* below).

| Key | Action |
|---|---|
| P | start the animation (reloads both files) |
| Space | pause / resume |
| R | back to the first keyframe |
| C | switch to the manual cameras and cycle them: Launch site → Vehicle → Planets → Mars → Earth view. P returns to the keyframed camera |
| Arrow keys, [ / ] | orbit and zoom the manual camera (switches to it from the keyframed camera) |
| E | trajectory editor (below) |
| Esc | exit |

**Manual cameras**
- *Earth view*: distant camera around Earth; the flight path so far is
  drawn as a thin line.
- *Launch site*: fixed near the pad, always aimed at the vehicle.
- *Vehicle*: moves with the launch vehicle (Mangalyaan once separated).
- *Planets*: from behind Earth towards Mars, so both are in view (Mars was
  about 98° away from India in the sky on launch day).
- *Mars*: while Mangalyaan is in Mars orbit, follows it from the side away
  from Mars, so Mars is behind it and its motion around Mars is easy to
  see (`]` zooms out to the whole orbit). Before that, it shows Mars.

**Lighting.** The Sun direction at launch time (RA 220.5°, Dec −15.7°,
from the JPL approximate planetary elements) puts the Sun in the
south-west at about 40° elevation over Sriharikota, matching the 14:38 IST
launch. The sky is blue near the sunlit ground and fades to black with
altitude.

**Celestial bodies.** Mangalyaan is a Mars mission, so the scene includes:
- *Mars*: textured sphere, lit by the Sun. It is placed in its real direction on launch day (RA 164.1°,
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
separated parts are drawn about 140× larger. The vehicle path itself uses
real distances.

## Assignment 3

### Bézier trajectory editor (in a3-scene)

Press **E** to switch the editor on or off. The editor has its own camera,
which orbits a *focus point* (shown as a small green cross). It starts
just above the launch pad.

| Key / mouse | Action |
|---|---|
| E | editor on / off |
| Left click | add a control point |
| Right click, Backspace | remove the last point |
| Arrow keys | orbit the editor camera around the focus point |
| Shift + arrow keys | move the focus point across the screen |
| Page Up / Page Down | move the focus point into / out of the screen |
| [ / ] | zoom (works from launch-pad scale out to Mars) |
| F | move the focus to the last point |
| N | start a new trajectory |
| T | save (append) the current trajectory to `trajectory.txt` |
| Y | load every trajectory from `trajectory.txt` and show them |
| H | hide / show the loaded trajectories |

**Placing points.** A click casts a ray from the camera through the
mouse cursor. The point is placed where the ray meets the plane through
the focus point facing the camera. If the ray hits Earth's surface first,
the point snaps onto the surface, so points never end up inside Earth.
To place a point deeper or nearer, move the focus point (Shift + arrows,
Page Up / Page Down) and click again.

**What is shown.** Every control point of the current trajectory is a
small sphere (yellow, the last one white). The control polygon is grey
and the Bézier curve is yellow. The title bar shows the number of points,
the focus position and the last action. Each placed point's coordinates
are also printed in the terminal.

**Curves.** A trajectory with *n* control points is one Bézier curve of
degree *n − 1*, evaluated with de Casteljau's algorithm
(`src/model3/Bezier.cpp`).

**File format.** `T` appends one block per trajectory to `trajectory.txt`,
exactly as in the assignment:

```
number_of_control_points
x_1 y_1 z_1
...
x_n y_n z_n
```

`Y` reads all blocks and draws each curve, its control polygon and its
points in its own colour (8 colours, cycled). The loaded curves stay
visible from the normal cameras too; `H` hides them.

Coordinates are world units of the scene (Earth radius 30, Earth centred at
the origin).

### Keyframe animation

**Trajectories.** `trajectory.txt` holds 18 Bézier curves, in this order:

| # | Object | # | Object |
|---|---|---|---|
| 1 | launch vehicle                    | 10 | Mangalyaan: Earth orbit raising |
| 2 | strap-on boosters (all four)      | 11 | Mangalyaan: cruise to Mars |
| 3 | PS1                               | 12 | Mangalyaan: Mars orbit |
| 4 | payload fairing (both halves)     | 13, 14 | camera eye / target, orbit raising |
| 5 | PS2                               | 15, 16 | camera eye / target, cruise |
| 6 | PS3                               | 17, 18 | camera eye / target, Mars orbit |
| 7 | Mangalyaan after separation       |   |
| 8, 9 | camera eye / target, launch    |   |

Each separated part's curve starts on the vehicle curve at its separation,
and each of Mangalyaan's curves starts where the previous one ends. Blocks
added later with `T` (number 19 and up) are shown by `Y` but are not used
by the animation.

**Keyframes.** Each row of `keyframes.txt` is one keyframe:

```
active_1 u_1  active_2 u_2  ...  active_18 u_18   stage engine tilt
```

- `active_i` (0/1) shows or hides object *i*, and `u_i` is its position on
  its curve.
- `stage` (0–6) counts the parts dropped so far: 0 on the pad, then
  strap-ons, PS1, fairing, PS2, PS3, Mangalyaan. Parts still attached are
  drawn on the vehicle.
- `engine` (0/1) shows the exhaust of the stage that is firing (after the
  launch: Mangalyaan's own engine).
- `tilt` is the vehicle's angle from the local vertical, in degrees. Each
  separated part keeps the tilt it had at separation and tumbles slowly.

Lines starting with `#` are comments.

**Playback.** Every keyframe interval lasts a fixed 60 frames. Curve
parameters and `tilt` are interpolated linearly. `active`, `stage` and
`engine` keep the earlier keyframe's value and switch exactly at the next
keyframe, so they never take fractional values. Keyframes are closer
together around each separation, so those moments play slowly and the
long burns play quickly. Before P is pressed, the scene shows the first
keyframe (vehicle on the pad).

**Planning the paths: `a3-plan`.** The trajectories and keyframes were
made with `./build/a3-plan`, run in this folder. It flies the PSLV-C25
ascent with a point-mass model (`src/model3/Flight.cpp`):
- gravity always acts;
- thrust and a gravity-turn pitch program act while a stage burns;
- drag (exponential atmosphere) acts only **below 100 km**, so motion in
  the atmosphere and in space differ.

Separations are triggered at the altitudes of the PSLV-C25 flight profile.
The planner then fits a Bézier curve to the path of every part (least
squares with fixed end points, degree raised until the error is small;
for the single vehicle curve the worst case is about 6% of the rocket's
height) and writes both files.
It will not overwrite existing files unless run with `--overwrite`, in
which case the old files are kept as `.bak`.

| Event | Trigger | Time |
|---|---|---|
| Strap-on separation | 25 km | T+73 s |
| PS1 separation / PS2 ignition | 58 km | T+121 s |
| Payload fairing separation | 115 km | T+215 s |
| PS2 separation / PS3 ignition | 135 km | T+271 s |
| PS3 separation / PS4 ignition | 195 km | T+473 s |
| PS4 cut-off | orbital speed | T+821 s, ~258 km |
| Mangalyaan separation | 20 s after cut-off | T+841 s |

### The rest of the mission

After separating in Earth orbit, the real Mangalyaan raised its orbit
with six burns at perigee (November 2013). The Trans-Mars Injection burn
on 1 December 2013 sent it on a ~300-day cruise, and the Mars Orbit
Insertion burn on 24 September 2014 put it into an elliptical orbit around
Mars. The animation shows this in shortened form (distances and durations
are not to scale; Mars stays where it was on launch day):

1. **Earth orbit raising:** two orbits whose apogee grows while the
   perigee stays low, with a burn at each perigee.
2. **Cruise:** a cubic Bézier from the last perigee (Trans-Mars Injection
   burn) to a point beside Mars (Mars Orbit Insertion burn).
3. **Mars orbit:** most of one elliptical orbit around Mars (the animation
   stops just before the orbit closes).

Mangalyaan is drawn 150× larger in these phases so it stays visible, and
its path so far is drawn as a line. Each orbit takes about 12 seconds on
screen. Each phase has its own camera eye/target pair, a few units from
Mangalyaan:
- orbit raising: just outside the spacecraft's orbit, with Earth behind it;
- cruise: just behind it, with Mars ahead;
- Mars orbit: just outside its orbit, with Mars behind it.

These camera curves are the payload curve's control points scaled or
shifted, and scaling or moving a Bézier curve's control points scales or
moves the curve exactly, so the camera follows the spacecraft precisely.
The cut between phases is a planned camera change.

## Use of Blender
 All geometry (cylinders, cones, cubes,
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
src/model3/       a3-scene: main.cpp, Bezier.cpp (curves, trajectory.txt),
                  Keyframes.cpp (keyframes.txt, interpolation),
                  TrajectoryEditor.cpp (mouse editor), SceneConfig.h
                  a3-plan: Planner.cpp, Flight.cpp (flight model)
trajectory.txt, keyframes.txt   the animation data
```

## References and acknowledgements
- ISRO, *PSLV-C25 / Mars Orbiter Mission* brochure and launch press kit
  (vehicle configuration, flight sequence and event altitudes; add link).
- Earth texture: `textures/earth.jpg` ([NASA] https://science.nasa.gov/resource/blue-marble/?utm_source=chatgpt.com).
- Mars texture: `textures/Mars.jpg` ([NASA](https://science.nasa.gov/3d-resources/mars/)).
- Ground texture: `textures/groundTexture.jpg` ([Unsplash](https://unsplash.com/photos/green-grass-field-during-daytime-IkghfzzIC2s)).
- E. M. Standish, *Keplerian Elements for Approximate Positions of the
  Major Planets* (JPL), used for the Sun and Mars directions at launch.






