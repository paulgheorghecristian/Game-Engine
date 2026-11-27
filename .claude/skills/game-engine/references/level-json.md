# The level JSON

A level is one JSON file (`Game/*.json`, `Game/recursion/*.json`); its timelines are files in
`Game/sequences/`. The engine names no level content: sounds by file name in `res/sounds` (no `.wav`),
models and textures by path. Units: 24.8 world units ≈ 1 m (`M` in the tools); floors at y 0, ceilings at
71.92 in the recursion levels; the player's eye about 31.

## Contents

1. Top level · 2. Entities and materials · 3. Lights · 4. Environment · 5. Sounds · 6. Interaction ·
7. Timelines · 8. Streaming levels

## 1. Top level

`ConfigFilePath`, `gravity`, `PlayerStart` (`position`, `yaw`, `pitch`), `Environment`, `Entities`, `Lights`,
`ParticleEmitters`, `Audio`, `Footsteps`, `Sequences` (`[{"file": "Game/sequences/x.json"}]`),
`Interaction`, and per level type: `Waking` (waking-up scene), `RoomAlive`, `Water`, `Grass`, `BlackHoles`
(sky only: `"sky": true`).

## 2. Entities and materials

```json
{"Transform": {"position": [x, y, z], "rotation": [rx, ry, rz], "scale": [sx, sy, sz]},
 "Components": {
   "RenderComponent": {"Mesh": "res/models/cube4.obj", "castShadow": false, "Material": {...}},
   "PhysicsComponent": {"BoundingBodyType": ["cube"], "scale": [sx, sy, sz], "mass": 0},
   "Pressable": {...}, "Driven": {...}}}
```

- Rotation in degrees (or a quaternion `[x, y, z, w]`). `cube4.obj` is a unit cube: its scale is its size.
  Meshes exported from Blender sit at their world place with a zero transform.
- Material keys: `diffuse`, `ambient`, `specular`, `shininess`, `roughness`, `metallic`, `Texture`,
  `NormalMapTexture`, `normalMapStrength`, `RoughnessTexture`, `tileSize` (tile a texture every that many
  world units, by position: no UVs needed), `emissive` (0..1; glows by itself), `emissiveNightOnly`,
  `specularStrength`, `glass` (see-through), `disableCulling` (both sides drawn), `isBlackAlpha`, `Terrain`.
- Paint, plaster and plastic are not metal (`metallic` 0); a textured material's `diffuse` is divided by the
  texture's mean colour so it keeps its colour (`tools/surfaces.py` does this).
- Realism (CLAUDE.md): relief goes in normal and roughness maps, only truly flat print in textures.

## 3. Lights

```json
{"Transform": {"position": [0, 70.3, 0], "rotation": [0, 0, 0], "scale": [100, 100, 100]},
 "Attributes": {"type": "POINT", "color": [0.85, 0.95, 1.0], "cast_shadow": true, "shadowMapSize": 1024,
   "diffuse_attenuation": [800, 10, 1], "specular_attenuation": [420, 6, 1.3],
   "flicker": 0.05, "flickerSpeed": 7,
   "emission": {"axis": [0, -1, 0], "full": 75, "none": 120},
   "tube": {"axis": [0, 0, 1], "length": 29}}}
```

- `scale` (x) is the light's reach: its light fades to zero there.
- `diffuse_attenuation` [a, b, c] → 1 / (a/1000 + b·d/1e6 + c·d²/1e6): larger a = dimmer near the light.
- `emission`: how a fitting sends its light out — full within `full` degrees of `axis`, none by `none`
  (a ceiling fitting open below: down and to the sides, a little up past its housing).
- `tube`: a line light (`length` along `axis`): lit from along the tube, highlights stretched along it.
- Other types: `SPOT` (`dir`, `coneAngle`, `volumetric`, `beamDensity`, `beamFalloff`, `beamHaze`),
  `DIR` (`dir`). Also `orbit`, `spin`, `nightOnly`.
- A fitting is part of its light: its housing and tubes don't cast shadows (`castShadow: false`), the light
  sits inside it just above the tubes, the housing glows faintly (`emissive` 0.12) as its reflector would.

## 4. Environment

Look and atmosphere of the level (blended over 1.5 s when walking from one streamed level into another):

- Sky and moon: `sky` (`"night"`/`"dusk"`), `zenithColor`, `horizonColor`, `starIntensity`, `moon`,
  `moonDirection`, `moonColor`, `moonPhase`, `moonSize`, `milkyWay*`, `cloudCoverage`, `cloudSpeed`,
  `cloudHeight`, `wind`.
- Fog: `fogColor`, `fogDensity`, `fogHeightFalloff`, `fogBaseHeight`, `fogStart`, `fogEnd`, `fogLightGlow`
  (lights' halo in the fog: halo strength ≈ fogDensity × fogLightGlow), `fogMoonGlow`.
- Ambient: `ambient` (flat stand-in for the far world), `ambientSkyTint` (up-facing),
  `ambientGroundTint` (down-facing: stands for the lit floor's light on ceilings, which the screen-space
  bounce light can't see when looking up), `aoStrength`, `aoRadius`.
- `bounceLight` (1 = as light really bounces; 0 off) and `bounceRadius` (world units): one bounce between
  near surfaces, in screen space.
- Picture: `hdr`, `pbr`, `exposure`, `autoExposure`, `saturation`, `grade`, `bloomStrength`,
  `bloomThreshold`, `lensDirt`, `chromaticAberration`, `filmGrain`, `temporalAA`, `reflections`,
  `emissiveStrength`.
- Day cycle: `dayLength` (0: none), `timeOfDay`, `timePaused`, `day*` colours, `sunColor`, `sunsetColor`.

## 5. Sounds

- `Audio`: `[{"sound": "hum_loop", "position": [x, y, z], "volume": 0.4, "radius": 149, "loop": true,
  "pitch": 1, "follows": "wind" | "night"}]` (no position: everywhere).
- `Footsteps`: `{"sound": "step_sneaker", "variants": 10, "volume": 0.8, "stride": 30, "wade": "wade",
  "wadeVariants": 3, "splash": "splash", "dust": {...}}` (`step_sneaker_1..10`; `stride`: units walked between
  steps, 40 by default, half again running; `wade`/`splash` in water). A sound not named is not heard: every
  map names its own (grass outdoors, `step_sneaker` on hard floors, `step_sand`).
- The sky's Kugelblitz: `"blastSound"` and `"boomSounds": [...]` on its `BlackHoles` entry (played by
  `Audio/KugelblitzSounds`; silent while it is `silent` or driven by a timeline).
- New sound files: `tools/import_sounds.py` (recordings) or a generator script; credit them in
  `res/sounds/CREDITS.md`.

## 6. Interaction

What the player does, all as data (`Core/Interaction.h` has the full reference).

- On an entity: `"Pressable": {"reach": 32, "travel": [0, 0, 0.1], "pressTime": 0.16, "on": [actions]}`
  (aim ring + left click; it goes in by `travel` and back). `"Driven": {"param": "ward_door", "pivot":
  [x, y, z], "rotate": [0, 90, 0], "move": [0, 0, 0], "anchor": "ward_door"}` (moved by a timeline
  parameter 0..1; a physics body on it goes with it).
- The level's block:

```json
"Interaction": {
  "locks": [{"name": "ward_keypad", "code": "0314", "clear": "*", "enter": "#",
             "onSolved": [actions], "onWrong": [actions]}],
  "triggers": [{"box": [[x0, y0, z0], [x1, y1, z1]], "after": "event", "once": true, "on": [actions]}],
  "reactions": [{"event": "ward_door_shut", "on": [actions]}],
  "entries": {"from_ward": {"position": [x, y, z], "yaw": 0, "pitch": 0, "fadeIn": 1.4, "on": [actions]}}}
```

- Actions: `{"sound": ..., "volume", "pitch", "at": [x, y, z], "radius"}` · `{"lock": "name", "key": "0"}` ·
  `{"event": "name"}` (starts sequences triggered by it; arms triggers' `after`; runs reactions) ·
  `{"streamLevel": path, "attach": {...}, "standIns": [[lo, hi]]}` · `{"unloadLevel": "self"}` ·
  `{"loadLevel": path, "entry": name, "fade": 0.9}` (old way: fades and restarts the engine; for jumps).
- Events fired by a timeline (a sequence's `events`) run reactions too.

## 7. Timelines (`Game/sequences/*.json`)

```json
{"name": "the ward door opens", "trigger": "ward_door_unlocked", "length": 8,
 "params": {"ward_door": {"keys": [[0, 0], [0.9, 0], [4.4, 1]], "curve": "smooth"}},
 "sounds": [{"at": 0.55, "sound": "door_unlock", "volume": 0.8, "anchor": "ward_door", "radius": 400}],
 "events": [{"at": 4.2, "event": "ward_door_shut"}]}
```

- `trigger`: `"mapStart"`, `"kugelblitzFormed"`, or any other name = an event (starts when signalled; going
  back before that on the scene clock un-signals it).
- Keys `[t, v]`, `[t, v, lx, ly, rx, ry]` (bezier handles), `[t, v, "hold"]`; curves `smooth`, `linear`, `step`.
- A parameter set by several running sequences: the later one in the map's `Sequences` list wins.
- The user edits these in `tools/timeline_editor.py`: keep the files' shape, and check it isn't open on them.

## 8. Streaming levels

The next level is streamed in beside this one while the player plays (`Core/LevelStreaming.cpp`):

```json
{"streamLevel": "Game/recursion/02_corridor_subject4.json",
 "attach": {"here": [0, 0, 89.28], "there": [-31.74, 0, 198.4], "turn": -90},
 "standIns": [[[-33.6, -1, 186], [-29.9, 56, 211]]]}
```

- `there` (a point of the next level, in its own coordinates) goes to `here` (this level's coordinates),
  turned `turn` degrees round y about it: the two levels' shared door meets. Everything of the streamed
  level (entities, lights and their axes, sounds, triggers, `at`s) goes through this placement.
- `standIns`: boxes (the streamed level's coordinates) whose entities are built only once this level is
  taken out — its own door where this level's door stood.
- A streamed level brings its `ParticleEmitters`, `Footsteps` (the player's steps follow the level they are
  in) and `Water` (if the world has none yet; its box turned with the level), and takes them away again.
  `Grass`, `Waking`, `RoomAlive` and `BlackHoles` are the first map's only (the log says so if a streamed
  level has them). Every level has the same parts (`Core/Level.h`): the first's sounds, timeline and
  interaction are the engine's own; seeking moves a streamed level's clock too (from when it came).
- It is built a slice a frame; its textures and models are read on worker threads first, big textures
  uploaded in strips. A level that asks to go (`unloadLevel`) is taken out once the player is not inside its
  bounds; its stand-ins' level then builds them.
- Which level the player is in (for the environment): the smallest whose bounds (its meshes' real boxes)
  hold the eye.
- Level 1 → corridor is the worked example (`tools/ward_interaction.py`): the code streams the corridor in,
  a trigger out in it starts the door's closer (`01_ward_door_close.json`), its `ward_door_shut` event
  unloads the ward.
