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

## Where things go

Respect the folders when adding features: generators in `tools/`, map JSON in `Game/`, shaders in
`res/shaders/`, game logic in `Core/`, renderers in `Rendering/`, audio in `Audio/`.
