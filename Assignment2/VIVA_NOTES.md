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
