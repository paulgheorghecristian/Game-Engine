# Game-Engine2

## Levels are made without touching the code

A level (a map, its timelines, its sounds) must be creatable and changeable entirely in JSON. The
engine code never names a level's content: it is general and plays, shows or loads whatever the
data names.

- Content a level uses (its sounds, ambience, models, its own textures) is named in the level's
  JSON, where it is used: a sound by its file name in `res/sounds`, a model by its path.
  Nothing is looked up from a key whose meaning only the code knows.
  - Wrong: `"Audio": {"crickets": 0.25}`, where the code knows "crickets" means
    `crickets_loop.wav` and that it follows the night. Another level cannot have frogs instead,
    or two ambient beds, without changing the code.
  - Right: the JSON names `"sound": "crickets_loop"` and says what its volume follows.
- Behaviour that links content to something in the game (a volume that follows the wind or the
  night, a sound picked at random from a set) is a general option the data chooses, not a special
  case per resource. The same goes for the sounds an effect makes (the storm's booms, gusts,
  thuds, thunder): the data names them.
- Allowed in the code: textures and shaders that are part of the engine itself (the particle
  shader, the GUI shaders, the glow and smoke sprites the engine's own effects draw with).
- When touching code that still hard-codes a level's content, move it to JSON as part of the
  change, or say so if that is out of scope. Do not add new ones.
- The tools that generate levels (`tools/*.py`) may name content: that is data being written.

## Something new and particular is its own solution

When something new and particular is added (a one-time animation such as the Moon's destruction,
an effect for one scene, a level's special behaviour), build it as its own solution, in its own files: its own
shader, its own class in the right folder, its own timeline (sequence) file. Never add it inside
the general engine files, the ones everything shares (e.g. `res/shaders/skyShader.fs`, the scene
and lighting shaders, `Rendering/RenderingMaster.cpp`, `Core/EngineCore.cpp`,
`Core/Aftermath.cpp`). General improvements to the engine itself (a renderer fix, a faster pass)
still go where they belong.

- Wrong: a lopsided blast for one animation written into the sky shader's Moon destruction.
- Right: the blast as its own element, with its own shader and its own drawing class, driven by
  its own lanes in its own sequence file.
- The only change allowed in a general file is the smallest hook that lets the new element run (a
  call where it is drawn or updated, registering its timeline lanes). Keep it to that, and say in
  the reply which hooks were added.
- Particular things already inside general files (the Moon's destruction, the Kugelblitz, is
  drawn in `res/shaders/skyShader.fs` and driven from `Rendering/RenderingMaster.cpp`) are to be
  moved out into their own element when that code is next worked on, or flagged if out of scope.

## Realism

The game aims for realism. Things are built the way the real object is: a wall clock has a printed
dial, hands that stand off it and a rim; keys are raised buttons; a sign's letters stand out of
its plate; paper on a wall is creased. Never flatten an object into a picture of itself (a "paper
clock"), never trade realism for an easier fix.

- A texture is right only for what really is flat: print on paper, a painted dial's marks, the
  wires inside wired glass, joints in a floor. Relief and wear go in normal and roughness maps.
- When small detail misbehaves (thin geometry flickering or vanishing at a distance), fix it
  without losing the real shape: print only the truly flat part, keep the rest as geometry.
- Look at the result up close and from across the room before calling it done.

## Where things go

Respect the folders when adding features: generators in `tools/`, map JSON in `Game/`, shaders in
`res/shaders/`, game logic in `Core/`, renderers in `Rendering/`, audio in `Audio/`, tests in
`Tests/` (the timeline editor's suites in `Tests/editor/`, run by `run_all.py`; picture
comparisons between two builds in `Tests/render/`).

Test scripts belong in the repository, never only in a temporary or scratch folder (one is wiped
on a reboot, and with it the regression suite).
