# Running and checking the engine

`./ge <map.json>` from the repository root (paths in maps are relative to it). The menu shows first unless
`GE_SKIP_MENU=1`.

## Switches (environment variables)

| Switch | What it does |
|---|---|
| `GE_SKIP_MENU=1` | straight into the map |
| `GE_SKIP_ANIM=1` | the map's animations skipped: the scene as at the end of its timeline (no waking-up scene) |
| `GE_START="x,y,z,yaw,pitch"` | the spawn (degrees; yaw 0 looks down −z, negative turns right) |
| `GE_ENTRY=name` | in by one of the map's `Interaction.entries` (what the old `loadLevel` used) |
| `GE_SIZE=WxH` | window / picture size |
| `GE_BENCH=n` | benchmark: n frames (after 30 of warm-up), prints `BENCH ... avg=..ms` and the slowest phases, then quits; no vsync, scripted input only |
| `GE_SCENARIO=` | with `GE_BENCH`: `static` (stand still), `walk` (default; also drops lights), `fireballs`, `boxes`, `jump`, `hills`, `path`, `script` |
| `GE_SCREENSHOT=file.png` | with `GE_BENCH`: a picture of the last frame |
| `GE_CLEAN_SHOTS=1` | no profiler bars, FPS text, editor windows — and no aim ring |
| `GE_BENCH_SYNC=1` | per-phase times include GPU time (find what the GPU spends) |
| `GE_BENCH_SPIKES=ms` | every frame slower than that: `SPIKE frame N ms at x y z` and its phases |
| `GE_SEEK=t`, `GE_HOLD=1` | the scene t seconds in (its timeline), held there |
| `GE_PATH="x,z;x,z"`, `GE_PATH_SHOTS=dir` | with `GE_SCENARIO=path`: walk waypoints, a picture at each |
| `GE_SCRIPT=file.json` | with `GE_SCENARIO=script`: the steps below |
| `GE_AUDIO=1` | sound in a benchmark (off otherwise); with `SDL_AUDIODRIVER=dummy` it is mixed but silent |
| `GE_MUTE=1` | no sound |
| `GE_TIME_OF_DAY=0.5` | start at that time of day (maps with a day cycle) |
| `GE_OUTPUT_TYPE=n` | start in a debug view: 1 surface colour, 2 normals, 3 depth, 4/6 the light passes, 5 the finished picture (normal), 8 the sun's shadow map, 9 ambient occlusion, 10 roughness (keys 1, 2, 5, 6, 8 and 0 switch while playing) |
| `GE_RENDER_SCALE=0.75` | render below the window's resolution |
| `GE_MAX_FPS=n` | another frame cap for this run |
| `GE_PACING=1` / `=look` | frame-pacing log once a second (smoothness work); `look` turns round by itself; `GE_PACING_FRAMES=dir` saves every frame |
| `GE_PRINT_FPS=1` | FPS once a second |
| `GE_RECORD=1` | record a video from the first frame (`~/Videos/ge-*.mp4`; F12 toggles it by hand) |
| `GE_GPU=integrated` | keep the default GPU (it picks the discrete one otherwise) |
| `GE_NO_CULL=1`, `GE_VOLUMETRIC_DIV=n`, `GE_FULLSCREEN=1`, `GE_BENCH_SAVE=path` | view culling off; volumetric resolution divisor; fullscreen benchmark; save the map at the end |
| `GE_REMOTE=port` | driven by `tools/timeline_editor.py` (not by hand) |

Keys while playing: F1 editor GUI, F5 save map (`<map>_saved.json`), F9 reload it, F7 frame-pacing mode,
F11 screenshot (`~/Pictures/Screenshots/ge-*.png`), F12 record, left click presses what the aim ring is on.

## A before/after picture

```bash
S=<scratchpad>
GE_SKIP_MENU=1 GE_SKIP_ANIM=1 GE_CLEAN_SHOTS=1 GE_SCREENSHOT=$S/after.png GE_BENCH=90 \
  GE_SCENARIO=static GE_SIZE=1920x1080 GE_START="38,31,8,28,42" timeout 90 ./ge Game/recursion/01_beginning_astra.json
```

- 90 frames lets the anti-aliasing and bounce light settle (they average over frames).
- The "before": a scratch copy of the map with the old values, or the old shader copied back for one run
  (shaders load at start-up). Restore anything copied into the repository, and check with `git diff`.
- Put the two side by side with PIL, `.resize((960,540))` each, and look; crop and enlarge to judge detail.
  Brighten a crop (`ImageEnhance.Brightness`) to see dark artefacts.
- A view not matching what the user saw: work the camera out from the screenshot (what is in frame) and
  try again; don't judge from a different spot.
- White/black strips on the top and right edge of a picture are the capture, not the game.

## Scripted play-tests (`GE_SCENARIO=script`)

`GE_SCRIPT` is a JSON list of steps, each at a frame of the run (from 0, warm-up included); the run is in
play mode (no editor panels). Steps:

```json
[{"frame": 40, "look": [18.6, 33.35, 84.6]},
 {"frame": 50, "click": true},
 {"frame": 50, "press": "e"},
 {"frame": 60, "hold": "w", "frames": 150},
 {"frame": 70, "shot": "dir/name.png"},
 {"frame": 80, "print": true}]
```

`print` writes `SCRIPT frame N player x y z aim a fade f sounds a,b,...` (what is heard right now: check
sounds with `GE_AUDIO=1 SDL_AUDIODRIVER=dummy`). A step with `"map": "02_corridor"` only runs on a map
whose path contains that. Benchmark frames run faster than real time while sounds play in real time: a
sound "lasts" more frames than its length suggests. `Tests/game/ward_door.py` is the worked example.

## Debugging a rendering artefact

- Find which pass it is in: render variants of the map in scratch copies (a light without shadows, without
  its `emission`, `bounceLight: 0`, `temporalAA: false`) and see which one removes it.
- Look at one pass alone: temporarily make `bufferCombinationShader.fs` return only that term (×4), render,
  then put the shader back (it is a shared file: restore it exactly, `git diff` to check).
- Anything averaged over frames (the bounce light's noise, film grain) is hidden by the anti-aliasing:
  switch `temporalAA` off in the scratch map to see a single frame.
- A dark or bright spot that spreads into a disc is often one bad value smeared by bloom.
