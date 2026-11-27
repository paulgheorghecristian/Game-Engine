# Testing

Test scripts live in the repository (`Tests/`), never only in a scratch folder.

| Suite | Covers | Run |
|---|---|---|
| `Tests/game/ward_door.py` | level 1's way out as a player: aim, clicks on the keypad, wrong and right code, door, streaming the corridor in, walking into it, the door's closer, the ward taken out, sounds heard, no frame over 40 ms | `python3 Tests/game/ward_door.py --out <dir>` (half a minute, a window opens; pictures and `log.txt` in `<dir>`) |
| `Tests/game/walk_levels.py` | both connected levels walked as a player: ward, keypad, door, the whole corridor and back; streaming in and out, no fall, no stall over 40 ms, the floors' steps heard | `python3 Tests/game/walk_levels.py --out <dir>` (a minute) |
| `Tests/game/stream_features.py` | a streamed level's emitters, footsteps and water (placed, turned) brought in and taken away, on two small maps it writes | `python3 Tests/game/stream_features.py --out <dir>` (half a minute) |
| `Tests/editor/run_all.py [suite ...]` | the timeline editor through a browser (Playwright): `core`, `sounds`, `solo`, `mapsounds`, `noises` | `python3 Tests/editor/run_all.py core mapsounds` (a minute a suite) |
| `Tests/render/compare.py old new out [names]` | the same views from an old and a new build, against the noise between two runs | build the old one in a `git worktree` (see the script's docstring) |

## Which to run

- Anything the player does, interaction, level streaming, level 1's or the corridor's data:
  `Tests/game/ward_door.py`.
- `Core/Timeline`, sequence files, the editor, a map's `Sequences` or `Audio`: the editor suites (`core`, and
  `mapsounds` for sounds). First `pgrep -af timeline_editor`: if the user's editor is open, ask — the suites'
  guard restores map and timeline files, which would undo saves made meanwhile. The guard prints "all map
  and timeline files unchanged" when all is well.
- A renderer change that touches every level (a shared shader, the composite, lighting): `Tests/render/compare.py`
  on the affected views; for a small change, 1–3 targeted before/after pictures are enough.
- A level that must still start alone: `GE_SKIP_MENU=1 GE_BENCH=60 GE_SCENARIO=static GE_SIZE=960x540 timeout
  120 ./ge <map>` and look for the `BENCH` line.

## Writing a play-test

Follow `Tests/game/ward_door.py`: build the steps in Python (`look`, `click`, `hold`, `shot`, `print`), run
`./ge` with `GE_SCENARIO=script`, then check the log — `SCRIPT` lines for where the player is, what the aim
is on and what is heard; lines the engine prints for what happened (`Streamed in`, `In <level>`,
`Taking out`); `SPIKE` lines (`GE_BENCH_SPIKES=40`) for stalls. Print `OK` or each `FAIL`, exit non-zero on a
failure.
