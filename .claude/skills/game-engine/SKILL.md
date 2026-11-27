---
name: game-engine
description: How to work in this repository's C++/OpenGL engine and its game RECURSION — running and checking the engine (the GE_* switches, scripted play-tests, before/after pictures), the level JSON (entities, materials, lights with emission/tube, Environment, Interaction with locks, triggers and level streaming, timelines), the tools that write levels and assets (Blender exporters, generators, sound import), and the test suites. Use it for any change to the engine, a level, a shader, a sound or a tool here, for every "why does X look like this" question about a screenshot, and before rendering a check or running a test — even when the request doesn't name any of these.
---

# Game-Engine2: working in the engine and its levels

The project's rules are in `CLAUDE.md` (levels made in JSON only, new particular things in their own
files, realism, where files go) and the user's memory notes; this skill is the how-to that goes with them.
Read the reference file for what you are doing:

| Doing | Read |
|---|---|
| running the engine, a picture of a view, a benchmark, a scripted play-test | `references/running.md` |
| changing or reading a level (map JSON, materials, lights, Environment, Interaction, timelines, streaming) | `references/level-json.md` |
| a level or asset made by a tool (Blender export, generators, sounds) | `references/tools.md` |
| checking a change (which suite, how, what not to break) | `references/testing.md` |

## Build and run

```bash
make -j8                                   # ./ge; warnings about unused variables are old
GE_SKIP_MENU=1 ./ge Game/recursion/01_beginning_astra.json
```

Shaders are read when the engine starts: a shader change needs no build, only a new run.

## Things that cost time when forgotten

- **The user's screenshots** (F11) are `~/Pictures/Screenshots/ge-<date>-<time>.png`; they name them. Look
  at the picture before guessing.
- **A picture of a view**: `GE_SKIP_MENU=1 GE_SKIP_ANIM=1 GE_CLEAN_SHOTS=1 GE_SCREENSHOT=<png> GE_BENCH=90
  GE_SCENARIO=static GE_SIZE=1920x1080 GE_START="x,y,z,yaw,pitch" ./ge <map>`. Yaw 0 looks down −z, a
  negative yaw turns right (towards +x); pitch up is positive. Compare against the same view before the
  change, side by side (PIL), and keep it to the 1–3 views the change touches.
- **Engine windows open on the user's screen**: run them with a `timeout`, stop background ones promptly.
- **Generated levels are also hand-edited.** `tools/make_recursion.py` rewrites every recursion level and
  `tools/export_astra_room.py` rewrites level 1: never run them into `Game/` blindly. Regenerate into a
  scratch folder, diff against the file in `Game/`, and copy in only the level you changed (details in
  `references/tools.md`). Put every change into the tool as well as the JSON, or the next run undoes it.
- **The Blender export re-bakes pictures** that come out pixel-identical but rewritten: restore them with
  `git checkout` after checking (the same for `room_blanket.obj`, whose normals jitter in the last digit).
- **Light "size" is reach**: a point light's `scale` is its range, and its light fades to nothing there —
  a smaller size also dims what it still reaches.
- **The user's own uncommitted files** (they say which; e.g. `Game/canyon.json`, their timeline edits in
  `Game/sequences/01_beginning.json`) stay out of commits unless they ask.
- **Their timeline editor** may be open on port 8765: check `pgrep -af timeline_editor` before running the
  editor suites (its guard restores files the editor may be saving).
- **Commit and push only when asked**; push with
  `git push git@github.com:paulgheorghecristian/Game-Engine.git recursion_game`.

## How a change is usually checked

1. Build (`make -j8 2>&1 | grep error`).
2. A before/after picture of the view in question (`references/running.md`), looked at, not only measured.
3. The suite that covers it (`references/testing.md`): `Tests/game/ward_door.py` for anything the player
   does in level 1 or level streaming, the editor suites for timeline changes, `Tests/render/compare.py`
   for broad renderer changes.
4. Say which shared engine files got a hook (CLAUDE.md asks for it).
