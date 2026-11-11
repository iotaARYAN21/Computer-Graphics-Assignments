# Viva Notes – Animation, Fire, Cameras, Sky and Atmosphere

Simple-language notes for the viva. File and function names are given so
you can open the code and check.

Files with comments added for this:

1. `src/model2/main.cpp` – key-press animations (B / F / S / D)
2. `src/model3/Flight.h` – overview of the physics simulation
3. `src/model3/Flight.cpp` – the physics (where everything is)
4. `src/model3/main.cpp` – draws everything where `Flight` says
5. `src/model0/main.cpp` – `processInput` (Earth spin + camera)
6. Shaders: `shaders/cube_fragment.glsl`, `shaders/atmosphere_vertex.glsl`,
   `shaders/atmosphere_fragment.glsl`, `shaders/star_vertex.glsl`,
   `shaders/star_fragment.glsl`

---

## 1. How animation works (basic idea)

Animation = **drawing the scene again every frame, with each object's
position calculated from the current time.** We never store "move 1 step";
we ask "where should this be right now?"

### Way 1: Time-based (model 2, started by a key)

Pressing B only does: `boosterSeparationStarted = true` and
`startTime = glfwGetTime()`. Then every frame (in `drawRocket()`):

```
elapsed  = now - startTime              // e.g. 0.5 s
progress = clamp(elapsed / 2.0, 0, 1)   // 0.25 → a quarter done
smooth   = p*p*(3 - 2p)                 // slow start, slow stop
position = original + outward * 2.2 * smooth
draw booster at position
```

Stage separation (S) is the same, but each stage has a delay
(0 s, 1.8 s, 3.6 s), so they fall one after another (`stageDrop` lambda).
Key presses are handled in `processInput()`.

### Way 2: Physics-based (full scene `a2-scene`)

- Every object is a `Body` with a position and velocity (`Flight.h`).
- Every 0.02 s, `stepBody()` adds **gravity** (towards Earth),
  **thrust** (engine) and **drag** (air, only below 100 km), then:
  `velocity += acceleration × dt`, `position += velocity × dt`.
- `Flight::update()` uses a fixed time step with an accumulator.
- `Flight::guidanceDirection()` tilts the rocket from vertical to
  horizontal as it climbs (gravity turn).
- At set **altitudes** (25, 58, 115, 135, 195 km), `Flight::checkEvents()`
  drops a part using `Flight::addDebris()`: a copy of the rocket's state
  plus a small push. It then flies on its own with no engine and falls behind.
- `main.cpp` reads these positions and builds a **model matrix** for each
  part (`placeFrame`, `separatedPos`, `drawComponent`), adding small spins:
  boosters tumble (12°/s), fairing halves swing open around a hinge (25°/s).
- Mangalyaan grows (35% → 100%) and turns 45° after release
  (clamp + smoothstep).

---

## 2. GLM / helper functions in simple words

| Function | What it does | Example |
|---|---|---|
| `glm::clamp(x, lo, hi)` | Keeps x inside a range: below lo → lo, above hi → hi | `clamp(1.7, 0, 1) = 1`, so the animation stops at "finished" |
| `glm::min(a,b)` / `glm::max(a,b)` | Smaller / larger of two values, for one-sided limits | `min(pitch + step, 80)` → pitch never above 80° |
| `glm::mix(a, b, t)` | Blend: `a + (b-a)*t` | `mix(black, blue, 0.5)` = dark blue |
| smoothstep `p*p*(3-2p)` / `smoothStep()` | Turns a straight 0→1 into an S-curve (eases in/out) | gentle start and stop |
| `glm::translate(m, v)` | Move by v | |
| `glm::rotate(m, angle, axis)` | Spin around an axis (angle in **radians**) | |
| `glm::radians(deg)` | Degrees → radians | `radians(180) = π` |
| `glm::scale(m, s)` | Resize | |
| `glm::mat4(1.0f)` | Identity matrix = "no change"; starting point | |
| `glm::length(v)` | Length of a vector √(x²+y²+z²) | distance from Earth's centre |
| `glm::normalize(v)` | Same direction, length 1 | velocity → direction of travel |
| `glm::dot(a, b)` | One number: how much a points along b | vertical part of speed |
| `glm::cross(a, b)` | New vector at 90° to both | building east/north, rocket axes |
| `glm::lookAt(eye, target, up)` | Camera matrix looking from eye to target | |
| `glfwGetTime()` | Seconds since program start (stopwatch) | |
| `deltaTime` | Seconds since last frame; multiply speeds by it so the speed is the same on any computer | |

**Transform order:** the code says `translate → rotate → scale`, but a vertex
gets them in reverse order: **scale first, then rotate, then move**. So
objects are resized and spun around their own centre before being placed.

---

## 3. Rocket fire (exhaust) effect

Code: `drawPlume` lambda in the "Exhaust" section of `main()` in
`src/model3/main.cpp`; shader `shaders/cube_fragment.glsl`.

**Q: How is the rocket flame made?**
No texture or particles. Each flame is **two upside-down cones** under the
nozzle: an outer **orange** cone (wider, alpha 0.55) and an inner
**yellow-white** core (thinner, alpha 0.8).

**Q: How do you turn the cone upside down?**
`flip = glm::rotate(identity, PI, X axis)` rotates it 180° around X, so the
tip points down.

**Q: Why does the flame glow even in shadow or at night?**
The shader has a uniform `emissive`. When it's 1.0, lighting is ignored and
the full colour is shown (`mix(litColour, objectColor, emissive)`). It's
"self-lit", like a bulb.

**Q: Why does it look glowing and not like a solid cone?**
**Additive blending**: `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`. The flame colour
is *added* to what's behind it, so overlaps get brighter, like real light.
`glDepthMask(GL_FALSE)` stops the flame from blocking other things. Both
settings are restored after drawing.

**Q: How does it flicker?**
`flicker = 1 + 0.06·sin(40t) + 0.04·sin(97t)` with t = `glfwGetTime()`.
sin() goes up and down, so the length wobbles about ±10%. Two different
speeds mixed together look random, not like a steady pulse.

**Q: Why does the flame get bigger higher up?**
Less air pressure up high, so the flame spreads.
`expansion = 1 + 1.2 · smoothStep(30, 120, altitude)`: normal below 30 km,
2.2× wider by 120 km. Function: `smoothStep()`.

**Q: How do you know which engine's flame to show?**
From `flight.phase`: strap-on phase → 4 small flames + main;
PS2 / PS3 / PS4 → one flame at that stage's nozzle; `PHASE_COAST` → no
flame (engine off).

---

## 4. Camera modes

Code: `OrbitCamera` struct and `cameras[4]` array (top of
`src/model3/main.cpp`), `processInput()`, and the "Cameras" section of
`main()`.

**Q: How many cameras and how do you switch?**
4: Earth view, Launch site, Vehicle, Planets. Pressing **C** runs
`cameraMode = (cameraMode + 1) % 4`; `%` wraps back to 0 after the last.

**Q: Why doesn't a camera forget its angle when you switch?**
Each camera has its own saved `OrbitCamera {yaw, pitch, distance, limits}`.
The arrow keys only change `cameras[cameraMode]`, the active one.

**Q: How do yaw / pitch / distance become a camera position?**
Spherical → x, y, z:

```
orbit = (cos(pitch)·sin(yaw), sin(pitch), cos(pitch)·cos(yaw))
cameraPos = target + orbit * distance
```

Then `glm::lookAt(cameraPos, target, up)` builds the view matrix.

**Q: What does each mode look at?**

| Mode | Camera sits… | Looks at | Up direction |
|---|---|---|---|
| 0 Earth view | around Earth's centre | Earth's centre (0,0,0) | world Y |
| 1 Launch site | fixed near the pad, using site east/up/north axes | rocket (or satellite once released) | site "up" |
| 2 Vehicle | around the rocket, using the **rocket's own axes** | rocket / satellite | rocket "up" |
| 3 Planets | behind Earth, on the side away from Mars | a point between Earth and Mars | computed with `cross` |

**Q: Why does the vehicle camera follow the rocket?**
Its target is `rocketCentre`, recalculated each frame from the rocket's
model matrix. Its offset uses `rocketRight / rocketUp / rocketForward`, so
it moves *and tilts* with the rocket.

**Q: What is `nearPlane` and why does it change?**
How close something can be and still be drawn. Close-up cameras use 0.0005
so the small rocket isn't cut off; far cameras use 0.5 for better depth
precision (less flickering of overlapping surfaces).

**Q: Why does zoom divide/multiply instead of add?**
`distance /= zoomFactor` zooms by a percentage, so it feels the same close
or far. `glm::max/min` keep it between `minDistance` and `maxDistance`.

---

## 5. Sky going from blue to black

Code: "Lighting and sky" section of `main()` in `src/model3/main.cpp`;
shaders `star_vertex.glsl`, `star_fragment.glsl`.

**Q: How does the sky change colour as the rocket climbs?**
The background (clear) colour is calculated every frame:

```
cameraAltKm = (length(cameraPos) - EARTH_RADIUS) / KM_TO_WORLD
daylight    = smoothStep(-0.05, 0.2, dot(normalize(cameraPos), sunDir))
skyAmount   = (1 - smoothStep(0, 60, cameraAltKm)) * daylight
skyColour   = glm::mix(black, blue, skyAmount)
glClearColor(skyColour)
```

- **Altitude part:** fully blue at 0 km, fading smoothly to black at 60 km.
- **Daylight part:** `dot(cameraDirection, sunDirection)` checks whether
  the camera is on the sunny side of Earth. If it's night there, the sky
  stays black even on the ground.
- `mix` blends black and blue by that amount.

Functions: `smoothStep()`, `glm::mix`, `glm::dot`, `glClearColor`.

**Q: Why the *camera's* altitude and not the rocket's?**
The sky colour depends on where *you* are. From the launch-site camera
(on the ground) the sky stays blue while the rocket goes up; from the
vehicle camera you climb with it and see the sky turn black.

**Q: Why do stars appear only when the sky is dark?**
The star shader gets `starVisibility = 1 - skyAmount`: 0 in daylight
(invisible), 1 in space.

**Q: Why don't stars move when the camera moves?**
In `star_vertex.glsl`, `mat4(mat3(view))` removes the camera's position and
keeps only rotation. Stars react when you turn but never get closer, as if
they were infinitely far away.

---

## 6. Glowing atmosphere

Code: "Atmosphere" section of `main()` in `src/model3/main.cpp`;
shaders `atmosphere_vertex.glsl`, `atmosphere_fragment.glsl`.

The glow is a **slightly bigger, see-through blue sphere around Earth**.
A shader decides how strong the blue is at each pixel.

### Step 1: The shell (C++ side)

```cpp
glDepthMask(GL_FALSE);                                   // don't write depth
setMatrices(atmosphereProgram, glm::scale(earthModel, glm::vec3(1.03f)), ...);
glUniform3fv(... "cameraPos" ...);                       // where the camera is
glUniform1f(... "earthRadius", 30);                      // ground level
glUniform1f(... "shellRadius", 1.03 * 30);               // top of atmosphere
earthMesh->draw();                                       // same mesh as Earth
glDepthMask(GL_TRUE);
```

- **No new mesh:** Earth's sphere is reused and scaled to **103%**.
- Blending is on (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`), so the shader's
  alpha makes it see-through.
- `glDepthMask(GL_FALSE)`: the shell doesn't write depth, so it doesn't act
  like a glass wall hiding things behind it.
- **Drawn before Earth.** Earth is drawn right after and covers it wherever
  Earth is in front, so the glow only stays visible **around Earth's
  edge**, and in the sky when you are inside the shell.

### Step 2: Vertex shader (`atmosphere_vertex.glsl`)

Passes on two things:
- `WorldPos`: where this point of the shell is in the world
- `Normal`: the direction pointing straight out of the sphere. It uses
  `transpose(inverse(model))` so it stays correct after scaling.

### Step 3: Fragment shader (`atmosphere_fragment.glsl`)

Main idea: **you see a strong glow when you look through a lot of air.**

```
 looking straight down      looking at the edge (horizon)
  → passes through a          → line of sight skims low over
    thin layer of air            the ground, through lots of air
        👁                              👁
        |                                 \______
   ~~~~~|~~~~~ atmosphere          ~~~~~~~~~~~~~~\~~~~~~
   ███████████ Earth               ████████████████████
      faint glow                       strong glow
```

**a) Use only the back half of the shell**

```glsl
if (dot(N, V) > 0.0) discard;
```

`V` points from the shell to the camera. If the normal faces the camera, it's
the front side, so the pixel is thrown away. Each line of sight then counts
the air **once** (otherwise twice → too bright).

**b) Find the lowest point of the line of sight**

```glsl
vec3 dir = -V;                                   // direction camera → pixel
float t = clamp(-dot(cameraPos, dir), 0.0, length(WorldPos - cameraPos));
float lowest = length(cameraPos + t * dir);
```

Imagine a straight line from your eye to this pixel. `t` is how far along it
you are when **closest to Earth's centre**. `clamp` keeps that point between
your eye (0) and the pixel. `lowest` = distance from Earth's centre there.

**c) Turn it into a height from 0 to 1**

```glsl
float height = clamp((lowest - earthRadius) / (shellRadius - earthRadius), 0.0, 1.0);
```

0 = line skims the ground, 1 = only touches the top of the atmosphere.

**d) Turn height into glow**

```glsl
float glow  = pow(1.0 - height, 3.0);   // low → strong, high → weak
float alpha = glow * 0.45;              // at most 45% visible
if (alpha < 0.005) discard;             // skip nearly invisible pixels
FragColor = vec4(0.18, 0.48, 1.0, alpha);   // fixed blue
```

`pow(..., 3)` makes the glow fade fast with height, giving a **thin bright
ring** instead of a thick blurry blob.

### Why this approach is good

It works **whether the camera is outside (space) or inside (ground)**:
- **From space:** a blue ring around Earth's edge.
- **From the ground / while climbing:** lines of sight towards the horizon
  pass low → blue haze near the horizon, fading smoothly as you go up, with
  no sudden pop.

### Simpler version in model 0

`a2-model-0` doesn't set `earthRadius` (stays 0), so the shader uses the
simple **rim glow** branch:

```glsl
float rim = 1.0 - max(dot(N, V), 0.0);   // 0 facing you, 1 at the edge
float alpha = pow(rim, 5.0) * 0.22;
```

`dot(N, V)` ≈ 1 where the surface faces you (no glow), ≈ 0 at the edges
(full glow). The shell is scaled to 1.04 there.

### Atmosphere one-liners

- **What is it made of?** Earth's sphere mesh scaled 1.03×, drawn
  see-through in blue.
- **Why doesn't it hide the rocket?** Depth writes off
  (`glDepthMask(GL_FALSE)`) and it's transparent.
- **Why brighter at the edge?** You look through more air. The shader finds
  the lowest point of the line of sight; lower → more glow
  (`pow(1 - height, 3)`).
- **Why discard front faces?** So each line of sight is counted only once.
- **What does `clamp` do here?** Keeps the closest point on the line between
  camera and pixel, and keeps height between 0 and 1.
- **Why `transpose(inverse(model))` for normals?** Keeps normals pointing
  correctly after scaling/rotation.

---

## 7. Quick extra questions

- **Why does Earth rotate?**
  `earthRot = EARTH_ROTATION_DEG_PER_S × missionTime`, applied with
  `glm::rotate` around Y. Real rate: 360° per 86164 s (one sidereal day).
- **What is time warp?** `flight.update(deltaTime × warp)` runs the
  simulation 1–50× faster. It drops to 1× for 6 s after each event so
  separations can be seen.
- **Why is the Sun fixed?** Its direction is its real position at launch
  time (`surfaceDirection(SUN_LATITUDE, SUN_LONGITUDE)`). Earth spins
  underneath it → day and night.
- **What draws the orange trail?** Every 2 s the rocket position is added
  to `trail` and drawn with `GL_LINE_STRIP` (Earth view only).
- **Why are Mars and the Sun drawn first, then
  `glClear(GL_DEPTH_BUFFER_BIT)`?** They're very far away and use their own
  projection; clearing depth afterwards keeps good depth precision for the
  nearby rocket.
- **Why multiply speeds by `deltaTime`?** So movement is the same per second
  on fast and slow computers.
- **How are separated parts pushed apart?** A small "kick" velocity is added
  in `addDebris()` (e.g. boosters 12 m/s outward, stages 2 m/s backward,
  fairing halves 6 m/s sideways).

---

## 8. More Assignment 2 questions

**Q: In `a2-model-2`, does pressing S use `Flight.cpp` to drop the stages?**
No. `a2-model-2` is built without `Flight.cpp` (see `CMakeLists.txt`). Pressing
S only saves the start time. Each frame, the `stageDrop(delay)` lambda in
`drawRocket()` does:

```
p = clamp((elapsed - delay) / 1.8, 0, 1);   // 0 → 1 over 1.8 s
p = p*p*(3 - 2p);                           // smoothstep
drop = 7 * p;                               // slide down up to 7 units
```

Stage 1 starts at 0 s, stage 2 at 1.8 s, stage 3 at 3.6 s. Each one slides
straight down and is hidden at the end. `Flight.cpp` (real physics, separations
triggered by altitude) is only used in `a2-scene` and in `a3-plan`.

**Q: Is the service tower made of cubes or cylinders?**
Cubes only. Every bar is one cube stretched with `glm::scale`:
- 4 corner columns: cube scaled to `(0.10, 11, 0.10)`
- horizontal rings: cubes scaled along X or Z (`drawXBeam`, `drawZBeam`)
- diagonal braces: `drawBeam(start, end)`. It puts a cube at the midpoint,
  turns its Y axis towards `end - start` with `glm::rotation` (a quaternion),
  and stretches it to the full length.

Cylinders and cones are used only for the rocket (and the flames).

---

# ASSIGNMENT 3

Files: `Assignment3/src/model3/SceneConfig.h`, `Bezier.cpp`, `Keyframes.cpp`,
`TrajectoryEditor.cpp`, `Planner.cpp`, `main.cpp`, plus `trajectory.txt` and
`keyframes.txt`.

## 9. What is different from Assignment 2?

The scene looks the same (Earth, launch site, rocket, Sun, Mars, sky,
atmosphere, flame). What changed is **how things move**.

| | Assignment 2 (`a2-scene`) | Assignment 3 (`a3-scene`) |
|---|---|---|
| Positions come from | live physics (`Flight.cpp`) every frame | **lookup** in `trajectory.txt` + `keyframes.txt` |
| Timing | real seconds × time warp | frame counter, 60 frames per keyframe interval |
| Start / pause / reset | L / – / R | P / Space / R |
| Earth rotation | spins | still |
| New | – | Bézier curves, keyframes, mouse trajectory editor (E), planner `a3-plan`, Mars camera, keyframed camera |

- **`a3-plan`** (`Planner.cpp`) runs the Assignment 2 physics **once**, records
  the motion, fits Bézier curves and writes both text files.
- **Bézier curves** (`Bezier.cpp`): `evaluateBezier(points, u)` uses **de
  Casteljau's algorithm**, which is repeated `glm::mix` between neighbouring
  control points until only one point is left.
- **Keyframes** (`Keyframes.cpp`): `interpolateKeyframes()` blends between rows.
- **Editor** (`TrajectoryEditor.cpp`): E turns it on, a left click adds a point,
  T saves, Y loads, N starts a new curve, Backspace or a right click removes the
  last point. A click becomes a 3D point by casting a ray: mouse → NDC (−1..1) →
  `inverse(projection * view)` → a ray from the camera. The point goes where the
  ray hits the plane through the focus point facing the camera, or onto Earth's
  surface if the ray hits Earth first.
- **Custom flight:** if you drew a curve in the editor, P flies the whole rocket
  along it (600 frames, no separations). The nose points along the curve
  (`frameAlong`).
- `cube_fragment.glsl`: when `emissive >= 1`, it skips the lighting, so lines
  with no normals don't produce invalid (NaN) colours.

**One line:** Assignment 2 moves things with live physics. Assignment 3 records
that physics once as Bézier curves and keyframes, then plays them back.

## 10. Format of `trajectory.txt`

```
n          ← one number: how many control points this curve has
x y z      ← control point 1
...
x y z      ← control point n
n          ← next curve starts
...
```

| Line | Column 1 | Column 2 | Column 3 |
|---|---|---|---|
| header | n (number of points) | – | – |
| point | x | y | z |

- World units: Earth's centre is at (0, 0, 0), Earth's radius is 30, +Y is the
  North Pole. The first vehicle point (−4.95, 7.12, 28.72) is about 30 from the
  centre, so it is on the launch pad.
- The file has **no names, no time and no `u`**. Which curve is which comes only
  from the **order** (`TrajectoryId` enum in `SceneConfig.h`).
- `appendTrajectory()` writes it in append mode, so T adds to the end.
  `loadTrajectories()` reads it.

**Q: Should the number of blocks equal the number of objects?**
No. There is one block per **curve**, and our file has **18**:

| # | Curve | # | Curve |
|---|---|---|---|
| 1 | vehicle (whatever is attached) | 10 | Mangalyaan orbit raising |
| 2 | all 4 boosters | 11 | Mangalyaan cruise to Mars |
| 3 | PS1 | 12 | Mangalyaan Mars orbit |
| 4 | both fairing halves | 13, 14 | camera eye / target (orbit raising) |
| 5 | PS2 | 15, 16 | camera eye / target (cruise) |
| 6 | PS3 | 17, 18 | camera eye / target (Mars orbit) |
| 7 | Mangalyaan after separation | | |
| 8, 9 | camera eye / target (launch) | | |

- Several objects share one curve (4 boosters, 2 fairing halves).
- One object uses several curves (Mangalyaan uses 7, 10, 11, 12).
- Some curves are not objects (8 camera curves).
- Some objects have no curve (PS4 moves with the vehicle; Earth, Mars, the Sun
  and the launch site don't move along a path).
- The timing (which object is where, and when) is in `keyframes.txt`.
- Blocks have different sizes: the vehicle curve has 23 points (long, bendy
  path), the stage curves have 4 (short).

## 11. Why do the 4 boosters share one curve?

The curve is the path of the **centre of the group**, not of one booster.

- **Planner:** for each moment it **averages** the 4 booster offsets. The
  boosters push out in opposite directions, so those pushes cancel and the
  average is the group's centre.
- **Scene:** each booster is drawn at
  `point on curve + its own mounting spot (0.92·cos a, 1.85, 0.92·sin a) + outDir × spread`,
  where `spread` grows with u (12 m/s outward push). Each one also tumbles
  outward (`70° × u`).
- **Why:** the 4 real paths differ by only about 120 m over a path many km
  long. The difference is simple (outward along their own angle), so it is
  computed instead of stored. This means fewer curves and a smaller file.
- The fairing works the same way: 2 halves on one curve, each pushed to its own
  side (`sign × 6 m/s`) and swung open (`sign × 120° × u`).

## 12. How the files were made (`a3-plan`, `Planner.cpp`)

### a) What is recorded, and when

- The physics steps **0.02 s** at a time. Every **5th step** a **Record** is
  saved, so there is one record every **0.1 s**, about 8800 in total.
- One Record holds:

| Value | Meaning |
|---|---|
| `t` | mission time |
| `vehicle` | rocket position (km → scene units) |
| `tilt` | nose angle from straight up: `acos(dot(vehicleAxis, up))` |
| `offsets[]` | each dropped part's distance from the rocket (magnified like the rocket) |
| `payloadOffset` | Mangalyaan's distance from the rocket |

- The separation times are saved too: **73.4, 120.9, 215.2, 270.7, 473.4,
  840.6 s** (boosters, PS1, fairing, PS2, PS3, Mangalyaan). Recording stops 40 s
  later, at **880.6 s**.

### b) Records → dots for each curve

| Curve | Records used | Dot position |
|---|---|---|
| vehicle | all (0 → 880.6 s) | `r.vehicle` |
| boosters / PS1 / fairing / PS2 / PS3 | 10 s after their separation | rocket curve at t + (average) offset |
| Mangalyaan | 840.6 → 880.6 s | rocket curve at t + `payloadOffset` |
| camera target | all | middle of the rocket |
| camera eye | all | slides from a pad spot to a chase spot (smoothstep, 5 → 45 s) |

Using "rocket **curve** + offset" makes every part start exactly on the drawn
rocket. The space paths are not recorded: orbit raising and the Mars orbit come
from the ellipse equation `r = r0(1+e)/(1+e·cos θ)`. The cruise curve is 4
hand-picked control points (the 2nd sets the starting direction, the 3rd the
arriving direction). The space cameras are the spacecraft's control points
scaled or shifted, because scaling the control points scales the curve exactly
(**affine invariance**).

### c) Dots → control points (least-squares fit)

This is like the **best-fit line** in a lab, but with a Bézier curve:

1. Give each dot `u = (t − first t) / (last t − first t)`. **u = fraction of
   time.**
2. Fix the **first and last control points** to the first and last dots.
3. A cubic is
   `B(u) = (1−u)³P0 + 3u(1−u)²P1 + 3u²(1−u)P2 + u³P3`
   (the weights are **Bernstein polynomials**, `bernstein()`). Every dot gives
   one equation, so there are about 100 equations but only P1 and P2 are
   unknown. **Least squares** picks the P1, P2 with the smallest total squared
   error. The code builds the normal equations and solves them with **Gaussian
   elimination** (`fitBezier`).
4. If the **worst** error is above the limit, try one more control point, from
   4 up to 23 (`fitPath`).

Real result: vehicle **23** points, boosters/stages **4**, Mangalyaan **5**.

### d) Records → `keyframes.txt`

**Which times:** 0, 0 (wait on the pad), 5, then every 10 s, then every 30 s,
plus **event, event+2, event+5, event+10 s** for every separation, plus the end.
Sorted, and times closer than 1.5 s are merged.

**Values for each time t:**

| Value | How |
|---|---|
| `active` | 1 if t is inside this curve's time window |
| `u` | `(t − curve start) / curve length` |
| `stage` | number of separations already passed |
| `engine` | 1 if after the first row and before PS4 cut-off |
| `tilt` | `tilt` of the last Record at or before t |

Real rows (vehicle `u = t / 880.6`; booster window 73.4 → 83.4 s):

```
t(s)   vehicle u   boosters      stage engine tilt
 0.0   0.00000     off            0     0     0
 5.0   0.00568     off            0     1     2.4°
73.4   0.08337     ON  u=0.000    1     1    33.5°   ← booster separation
75.0   0.08517     ON  u=0.158    1     1    34.2°
78.4   0.08905     ON  u=0.501    1     1    35.6°
83.4   0.09473     off            1     1    37.7°   ← 10 s later, hidden
```

After the launch: 24 + 12 + 13 evenly spaced rows for orbit raising, cruise and
the Mars orbit (engine on at the burns). That is **111 rows** in total.

Rows close together around events play in **slow motion**, because every gap
gets 60 frames no matter how many seconds it covers.

Run `./build/a3-plan --overwrite` to make the files again (the old ones are
kept as `.bak`).

## 13. How the files are used in the animation (`a3-scene`)

**Simple idea:** `trajectory.txt` = the **tracks**, `keyframes.txt` = the
**timetable**.

**Press P:** `loadAnimation()` loads both files, `frame = 0`. It also stores
`separationTilt`, the rocket's tilt when each part separated.

**Every frame:**
1. `frame++` (Space pauses, R resets). Every gap between rows = **60 frames**.
   111 rows → 6601 frames, about 110 s.
2. `interpolateKeyframes()`: take the two rows around this frame and blend them.
   - `u` and `tilt` are **blended**.
   - `active`, `stage` and `engine` are **taken from the earlier row**.
3. `onPath(id, p)`: if active, `p = evaluateBezier(trajectories[id], u)`.
4. Draw:
   - **rocket:** curve 1 position, leaning by `tilt` (`frameAt`)
   - **`stage`:** which parts are still drawn **on** the rocket, and which
     nozzle gets the flame
   - **dropped parts:** their own curve, leaning by `separationTilt`, plus
     drift and tumble that grow with u. They vanish when `active` = 0.
   - **Mangalyaan:** unfolds by u = 1/8. In space it is drawn 150× bigger and
     points along its path (two nearby curve points).
   - **flame:** drawn if `engine = 1`
   - **keyframed camera:** `eye = curve(u)`, `target = next curve(u)`, then
     `glm::lookAt`. C switches to the manual cameras, P goes back.
   - **path line:** 1001 points on the curve from 0 to the current u, drawn as
     `GL_LINE_STRIP`.

**Example, frame 570:** k = 570/60 = 9, s = 30/60 = 0.5, so halfway between the
rows at T+73.4 and T+75.0:
vehicle u = 0.08427, boosters u = 0.079, tilt = 33.87°, stage = 1, engine = 1.

**One line:** frame number → keyframes give u → curve gives position → draw.

## 14. Why do we interpolate between keyframes?

- There are only **111 stored rows**, but the screen shows **60 pictures per
  second** (6601 frames).
- Without interpolation the rocket would **jump** from row to row, or we would
  have to store all 6601 frames.
- With interpolation every frame gets a slightly different u, so the motion is
  **smooth**.
- Linear blending of u is enough because the **Bézier curve** already gives the
  bent path.
- `active`, `stage` and `engine` are not blended, because "half visible" or
  "stage 1.5" make no sense.

## 15. Is u calculated for only one point between two keyframes?

No. u is calculated for **every frame**, so **60 values per gap**:

```
keyframe A: u = 0.20                 keyframe B: u = 0.40
frame  0 → u = 0.200
frame  1 → u = 0.203
...
frame 30 → u = 0.300
...
frame 59 → u = 0.397
frame 60 → next keyframe, u = 0.400

s = (frames since A) / 60
u = uA + (uB − uA) × s
```

Each u goes into `evaluateBezier` to get a new position, so the object moves a
tiny step every frame. This is done separately for **every curve** (rocket,
boosters, camera, and the rest).

## 16. Assignment 3 one-liners

- **What is a Bézier curve?** A smooth curve set by control points. It starts at
  the first point, ends at the last, and is pulled towards the middle ones. n
  points give degree n − 1.
- **De Casteljau?** Repeated `mix(Pi, Pi+1, u)` between neighbours until one
  point is left.
- **What is a keyframe?** A stored snapshot: for each curve, active and u, plus
  stage, engine and tilt.
- **Why u = fraction of time?** Then `u = (t − start)/length` in the keyframes
  puts every object exactly where the physics had it at time t.
- **Why are the end control points fixed in the fit?** So each curve starts and
  ends exactly where it should (for example, a stage starts on the rocket).
- **Why do some curves need 23 points and others 4?** Points are added until the
  worst error is small; long, bendy paths need more.
- **How does the camera follow the spacecraft exactly in space?** Its eye curve
  is the spacecraft's control points scaled or shifted (affine invariance).
- **How does a mouse click become a 3D point?** Ray casting with
  `inverse(projection * view)`, then intersect with a plane or Earth's sphere.

---

# QUESTIONS WHILE WATCHING THE OUTPUT

Questions an examiner may ask while looking at the running programs.
Unless stated otherwise, A2 = `Assignment2/src/model3/main.cpp` (`a2-scene`)
and A3 = `Assignment3/src/model3/main.cpp` (`a3-scene`).

## 17. Earth, sky and space (both assignments)

**Q: How is Earth made? Is it a texture?**
It is a sphere built by `generateSphere(30, 256, 512)` (256 rings × 512 slices
of triangles), with `textures/earth.jpg` wrapped around it. The texture
coordinates come from latitude and longitude. Shader: `shaders/vertex.glsl` +
`shaders/fragment.glsl`.

**Q: Why is one side of Earth dark?**
`fragment.glsl` uses **diffuse lighting**: brightness =
`max(dot(normal, lightDir), 0)`. The side facing away from the Sun gets 0.
There is a small **ambient** value (0.15), so the night side is dark but not
fully black.

**Q: Is the Sun in the correct place?**
Yes. `SUN_LATITUDE` / `SUN_LONGITUDE` are the Sun's real position at the launch
time (5 Nov 2013, 09:08 UTC), so at Sriharikota it is in the south-west, about
40° high (afternoon). `surfaceDirection(lat, lon)` turns latitude/longitude
into a direction. The Sun is a small self-lit sphere (`emissive = 1`) at
distance 1500.

**Q: Why is the launch pad in the right place on the Earth texture?**
`surfaceDirection()` uses the same mapping as `generateSphere()`
(`phi = 180 − longitude`), so 13.72° N, 80.23° E lands on India's east coast.
`getSurfaceTransform()` then gives a matrix that stands objects upright there.

**Q: Why is Mars so close, and why can't we see it from the launch site?**
It is placed in its **real direction** for launch day, but at distance 800
instead of the real distance (which would be far too big to show). Its size is
real compared with Earth (0.532 × Earth's radius). That day Mars was a morning
planet, so at 14:38 IST it was **below the horizon**. Use the Planets (or Mars)
camera to see it.

**Q: How are the stars made?**
`Starfield.cpp` puts 4000 random points on a big sphere, each with a random
brightness (most are faint). They are drawn as `GL_POINTS`, with bigger points
for brighter stars (`star_vertex.glsl`). They are drawn first with the depth
test off, so they are always behind everything.

**Q: Why don't the stars move when the camera moves, only when it turns?**
`star_vertex.glsl` uses `mat4(mat3(view))`, which removes the camera position.
So the stars behave as if they are infinitely far away.

**Q: Why do the stars disappear on the ground in daytime?**
`starVisibility = 1 − skyAmount` (`star_fragment.glsl`). In the blue sky it is
0.

**Q: Why does the sky turn from blue to black?**
In the "Lighting and sky" section: `skyAmount = (1 − smoothStep(0, 60, camera
altitude)) × daylight`, then `glm::mix(black, blue, skyAmount)` gives the colour
passed to `glClearColor`. It is blue on the sunlit ground and black above
60 km or at night.

**Q: What is the blue glow around Earth's edge?**
Earth's sphere drawn again at 1.03× size with `atmosphere_fragment.glsl`. The
glow is stronger where your line of sight passes low over the ground
(`pow(1 − height, 3)`). It is transparent and does not write depth
(`glDepthMask(GL_FALSE)`).

**Q: Why doesn't the rocket cast a shadow?**
We only use simple diffuse + ambient lighting. Shadows would need **shadow
mapping** (drawing the scene from the Sun into a depth texture), which we did
not implement.

**Q: Why does Earth look round and smooth, not faceted?**
High tessellation (256 × 512) and smooth **per-vertex normals** (the normal is
the direction from the centre), which the lighting blends across each
triangle.

## 18. Launch site (model 1 and the scenes)

**Q: What objects are at the launch site, and what are they made of?**
All are **cubes** scaled to shape (`Cube.cpp`):
- ground: a textured `Plane` (`groundTexture.jpg`, shader `ground_fragment.glsl`;
  plain green if the texture is missing)
- pad base and raised pedestal: flat boxes
- 4 orange lightning masts: tall thin boxes (`towerPositions`)
- service tower: columns, rings and diagonal braces (`drawBeam`, a cube rotated
  with `glm::rotation`)

**Q: Why is the ground flat when Earth is round?**
The patch is tiny compared with Earth, so a flat `Plane` placed with
`getSurfaceTransform()` looks right. It is only drawn for the Launch site and
Vehicle cameras (`cameraMode == 1 || 2`).

**Q: Why is the ground raised a little (0.05)?**
If two surfaces are at the same height they flicker (**z-fighting**). Lifting
the ground slightly above the Earth sphere avoids this.

**Q: Why is the tower on the north side?**
The rocket flies towards 104° (east-south-east) and the Sun is in the
south-west, so the tower is placed where it does not block the flight path or
the light.

**Q: (A2) What are the arms between the tower and the rocket, and where do they go?**
Umbilical arms. They are only drawn `if (!flight.launched)`, so they vanish at
lift-off.

## 19. Rocket (model 2 and the scenes)

**Q: What shapes is the rocket made of?**
- stages and boosters: `Cylinder(64, false)`
- nose tips and flames: `Cylinder(64, true)`, the cone version
- payload fairing: two half shells from `Fairing.cpp` (`halfIndex` 0 = +Z
  half, 1 = −Z half), each covering 180°
- Mangalyaan: cubes (body and solar panels)

**Q: Why is the rocket so big compared with Earth?**
At real size the 44 m rocket would be invisible. It is drawn about **140×
bigger** (`ROCKET_SCALE`). The path itself uses real kilometres. Separated
parts' distances are magnified the same way (`SEPARATION_TO_WORLD`) so they
look right next to the rocket.

**Q: Why does the rocket lean over while climbing?**
The **gravity turn**: it goes straight up, then tilts towards horizontal to
build sideways speed for orbit.
- A2: `Flight::guidanceDirection()` (`tilt = 90·sqrt(clamp(h/180, 0, 1))`)
- A3: the `tilt` column of `keyframes.txt`, used by `frameAt(pos, tilt)`

**Q: (model 2) What happens with B, F, S, D?**
`processInput()` stores the start time and `drawRocket()` animates using
`clamp` + smoothstep:
- B: the 4 boosters move out and down (2 s)
- F: the fairing halves slide apart along ±Z (2 s), showing Mangalyaan
- S: stages 1, 2, 3 slide down one after another (1.8 s each) and vanish
- D: Mangalyaan moves sideways and grows from 28% to 100% (2.5 s)

**Q: (model 2) Why can't we see Mangalyaan before pressing F?**
It is only drawn `if (fairingSeparationStarted)`, because it is "inside" the
closed fairing.

## 20. Flame, separations and payload

**Q: How is the flame made, and why does it glow and flicker?**
Two upside-down cones (`drawPlume`): orange outer, yellow inner.
- glow: `emissive = 1` (lighting ignored) + additive blending
  `glBlendFunc(GL_SRC_ALPHA, GL_ONE)`
- flicker: length × `1 + 0.06·sin(40t) + 0.04·sin(97t)`
- gets wider with altitude: `smoothStep(30, 120, altitude)`

**Q: Why does the flame move up the rocket after each separation?**
The nozzle position depends on which stage is firing: A2 uses `flight.phase`,
A3 uses `stage`.

**Q: Why do the boosters tumble and the fairing halves swing open?**
An extra rotation that grows with time: A2 uses seconds since separation
(`12°·t` boosters, `25°·t` fairing); A3 uses `u` (`70°·u` boosters, `120°·u`
fairing, `25°·u` stages). The fairing rotates around its **bottom edge**
(translate to the hinge → rotate → translate back).

**Q: Why do dropped parts fall behind and then disappear?**
- A2: debris has no engine (`stepBody` with zero thrust), so gravity + drag slow
  it. It is hidden after 150 model units (`DEBRIS_HIDE_DISTANCE`) or below 0 km.
- A3: each part has a 10-second curve (`DEBRIS_WINDOW_S`), after which
  `active = 0`.

**Q: Why does Mangalyaan grow and turn after release?**
It "unfolds": scale 35% → 100% and a 45° turn, using clamp + smoothstep (A2:
3 s after release; A3: during the first 1/8 of its curve).

**Q: (A3) Why is Mangalyaan huge near Mars?**
After the launch it is drawn 150× bigger (`PAYLOAD_SPACE_SCALE`); otherwise it
would be invisible next to the planets.

**Q: (A3) Are the orbits and the trip to Mars to scale?**
No. They are shapes only (ellipses from `r = r0(1+e)/(1+e·cos θ)` and a cubic
cruise curve in `Planner.cpp`), shortened so each orbit takes about 12 s.

## 21. Cameras and lines on screen

**Q: How many cameras, and how do we switch?**
- A2: 4 (Earth view, Launch site, Vehicle, Planets). C cycles them.
- A3: 5 manual cameras (+ Mars) **plus** the keyframed camera that plays with
  the animation, plus the editor camera (E).

**Q: How does the camera orbit with the arrow keys?**
`yaw`, `pitch` and `distance` (struct `OrbitCamera`) are turned into x, y, z:
`(cos p·sin y, sin p, cos p·cos y) × distance`, then
`glm::lookAt(eye, target, up)`.

**Q: Why does the horizon stay level at the launch site?**
The launch-site camera uses the **local up** (`siteUp`, away from Earth's centre)
as the `up` vector of `lookAt`, not world Y.

**Q: (A3) How does the camera move by itself?**
The camera's eye and target each follow their own Bézier curves (curves 8 and 9
for the launch, then 13–18): `cameraPos = evaluateBezier(eye curve, u)`,
`target = evaluateBezier(target curve, u)`.

**Q: (A3) What does the Mars camera show?**
While Mangalyaan orbits Mars, it follows the spacecraft from the side away from
Mars, so Mars fills the background. Before that, it just looks at Mars
(`cameraMode == 4`).

**Q: What is the orange line in the Earth view?**
The flight path.
- A2: the rocket position is saved every 2 s into `trail`.
- A3: 1001 points on the curve from u = 0 to the current u.

Both are drawn as `GL_LINE_STRIP`.

**Q: (A3) What are the yellow curve, the grey lines and the small spheres in the editor?**
Yellow is the Bézier curve (`sampleBezier`), grey is the **control polygon**
(the control points joined in order), and the spheres are the control points
(the last one is white). Curves loaded with Y have 8 colours
(`trajectoryColour`).

**Q: (A3) Why does a clicked point never go inside Earth?**
`placePoint()` checks whether the mouse ray hits Earth's sphere before the
focus plane. If so, it snaps the point onto the surface (radius × 1.00002).

**Q: Why are close objects not cut off, and why are the Sun and Mars drawn separately?**
Close cameras use a tiny **near plane** (0.0005). The Sun and Mars are very far
away, so they are drawn first with their own projection (near 1, far 5000),
then `glClear(GL_DEPTH_BUFFER_BIT)`. This keeps depth precise for the nearby
rocket.

## 22. Timing and title bar

**Q: (A2) Why does the simulation suddenly slow down at a separation?**
The time warp drops to 1× for 6 seconds after every event
(`flight.missionTime − flight.lastEventTime < 6`), so you can see it.

**Q: (A3) Why does the animation slow down at separations?**
Keyframes are close together around events (event, +2, +5, +10 s), but every
gap is still 60 frames, so a few seconds are stretched over many frames.

**Q: (A2) What does the title bar show?**
Mission time (T+), altitude, speed, current phase, time warp and camera name.
It is updated every 0.2 s with `glfwSetWindowTitle`.

**Q: (A3) What does the title bar show?**
Whether it is playing, the frame number, which keyframe, altitude and camera
name (or the editor status when E is on).

**Q: Will the animation run at the same speed on another computer?**
- A2: **yes**, because movement uses real time (`deltaTime × warp`).
- A3: playback is **frame-based** (`frame++`), so a slower computer plays it
  more slowly. `glfwSwapInterval(1)` (vsync) usually keeps it at about
  60 fps.

**Q: (A3) Why doesn't Earth rotate in A3?**
The curves store positions in **Earth-fixed** coordinates (the planner records
`flight.vehicle.pos`). Keeping Earth still (`earthModel = identity`) means the
launch pad stays exactly where the curves start.

**Q: Is the result the same every run?**
Yes. A2's physics has no randomness, and A3 always plays the same files. Only
the flame flicker uses the real clock (`glfwGetTime()`).

## 23. Model 0 (Earth viewer)

**Q: How does the camera move in model 0?**
Only by **translation**: the arrow keys move along X/Y and [ ] along Z. It always
looks along −Z (`glm::lookAt(cameraPos, cameraPos + (0,0,−1), up)`).

**Q: How does Earth rotate in model 0?**
A / D change `earthRotation`, which is applied with `glm::rotate` around Y
(`processInput()`).

**Q: Why is the camera slower near Earth, and why can't we go inside?**
Speed = `1.5 × altitude × deltaTime`, so it is slower when close. The camera
position is pushed back if its distance is below `MIN_CAMERA_RADIUS` (1.015)
or above `MAX_CAMERA_RADIUS` (50).
