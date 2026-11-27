# The tools that write levels and assets

Tools may name content (they write data); the engine may not. A change made to a generated JSON must go
into its tool too, or the next run of the tool undoes it.

## Who writes what

| File | Written by | Run |
|---|---|---|
| `Game/recursion/*.json` (all recursion levels) | `tools/make_recursion.py` | `python3 tools/make_recursion.py` (rewrites all of them; keeps each map's `Sequences` list) |
| `Game/recursion/01_beginning_astra.json` + `Game/recursion/astra/room_*.obj`, baked print `*.png` | `tools/export_astra_room.py`, from `astra/hospital_room.blend` on top of `01_beginning.json` | `blender -b --python tools/export_astra_room.py` |
| level 1's keypad, door, way out (in the above) | `tools/ward_interaction.py` (called by the exporter) | `python3 tools/ward_interaction.py` patches the current file in place |
| `astra/fitting_housing.obj`, `fitting_tubes.obj` | `tools/export_astra_fitting.py` | `blender -b --python tools/export_astra_fitting.py` |
| the ward's surfaces (texture sets, plaster, tiles, powder coat) | `tools/surfaces.py` (shared by both generators) | imported |
| `astra/ward_floor_tiles.jpg` | `tools/make_ward_floor.py` | |
| `res/sounds/*.wav` from recordings | `tools/import_sounds.py NAME FILE [--start --length --loop --gain]` | mono 44.1 kHz, trimmed, levelled |
| keypad tones | `tools/make_keypad_sounds.py` | |
| other levels | `make_lot.py`, `make_lagoon.py`, `make_canyon.py`, `make_sequences.py`, ... | |

## Regenerating one level safely

The user edits generated files by hand (film grain, timeline lists...). So never point a generator at
`Game/` without looking first:

```bash
S=<scratchpad>; mkdir -p $S/gen
sed "s|^OUT = .*|OUT = '$S/gen'|" tools/make_recursion.py > tools/_mr_tmp.py   # (in tools/: its imports)
python3 tools/_mr_tmp.py >/dev/null; rm tools/_mr_tmp.py
python3 -c "import json; a=json.load(open('$S/gen/02_corridor_subject4.json')); b=json.load(open('Game/recursion/02_corridor_subject4.json')); print([k for k in a if a[k]!=b.get(k)])"
```

- First check the unchanged generator reproduces the file (only the differences you know of, e.g. the user's
  own edits, which then go into the generator too).
- Then make the change in the tool, regenerate into scratch, confirm only the intended keys differ and that
  the other levels come out unchanged, and copy in just that file.
- `make_recursion.py` writes with `json.dump(..., indent=1)` (no newline at the end); the astra exporter with
  `indent=2` and a newline. Patch JSON in the same format so diffs stay small.
- Shared material dicts in `make_recursion.py` (`PEELING`, `TERRAZZO`...) are the same objects in every level:
  change one level's look on deep copies (as `ward_surfaces()` does).

## The Blender export (level 1)

- `blender -b --python tools/export_astra_room.py` takes a minute; it re-bakes the print pictures and rewrites
  `room_blanket.obj`. Compare: if the pictures are pixel-identical (`ImageChops.difference(...).getbbox()` is
  `None`) and the blanket differs only in the last digit, `git checkout` them.
- Then diff the level JSON against a copy taken before: only what you meant to change may differ.
- Groups (`GROUPS`: door, keypad, fittings) split objects into their own entities by their centre's box;
  an object of another part whose centre falls in the box goes with it (the door frame's head once did).
- Objects to hide or retexture from the old base level are found by position (`HIDE`, `RETEXTURE`): if the
  base level moves one, the export stops and says which.
- Leftover files in `Game/recursion/astra/` from astra (`blanket.obj`, `*.blend1`, `screenshots/`) are not
  ours to commit.

## Textures and models

- CC0 texture sets in `res/textures/<Set>_1K_{Color,Normal,Roughness}.jpg`, credited in
  `res/textures/CREDITS.md`; register a new set's mean colour in `tools/surfaces.py`.
- Downloads are untrusted: decode them (`ffmpeg`, PIL), don't run anything from them; read a skill or
  script fully before using it.
