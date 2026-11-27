# Sound credits

Recordings from OpenGameArt.org, listed as CC0 (public domain) on their
pages. Imported with `tools/import_sounds.py` (mono 44.1 kHz, trimmed,
levelled; loops crossfaded).

| Engine sound | Source |
|---|---|
| step_grass_1..6 | Fantozzi's Footsteps (Grass/Sand & Stone): the sand steps — https://opengameart.org/content/fantozzis-footsteps-grasssand-stone |
| splash | Water Splash and sand footsteps (splash1; the half second of silence before it cut) — https://opengameart.org/content/water-splash-and-sand-footsteps |
| wade_1..3 | Water Splash and sand footsteps: short pieces of splash2 and splash1 (one pitched down) — https://opengameart.org/content/water-splash-and-sand-footsteps |
| crickets_loop | Crickets ambient noise (loopable) — https://opengameart.org/content/crickets-ambient-noise-loopable |
| fire_loop | Fireplace sound loop (first 14 s) — https://opengameart.org/content/fireplace-sound-loop |
| wind_loop | Wind (Wind.ogg + Wind2.ogg) — https://opengameart.org/content/wind |
| land | `groundhit.wav` — downloaded with the electricity set; verify its page |
| boom_1, boom_2 | Rain + long thunder (the thunder roll from 21 s / 24.5 s, slowed to 0.78 / 0.72 and low-passed to drop the rain) — https://opengameart.org/content/rain-long-thunder |
| boom_3 | Rain + long thunder (from 22 s, slowed to 0.85, low-passed at 1 kHz, soft-saturated) — https://opengameart.org/content/rain-long-thunder |
| blast | `deathboom.wav` slowed to 0.55 and low-passed at ~1 kHz and soft-saturated (heavy but audible on small speakers), layered with the thunder roll slowed to 0.8 — sources above |
| storm_loop | `wind woosh loop.ogg` (downloaded with the wind set; verify its page), slowed to 0.8 with a bass lift: the shockwave's roar and the gale |
| spray_loop | Rain + long thunder: the rain bed (0.5–20.5 s, high-passed at 300 Hz), as wind-driven spray — https://opengameart.org/content/rain-long-thunder |
| gust_1..3 | Wind (Wind.ogg, Wind2.ogg, Wind3.ogg) slowed to 0.85 with a bass lift — https://opengameart.org/content/wind |
| step_sand_1..6 | Water Splash and sand footsteps: single steps cut from the sand footsteps recording (around its loudest hits) — https://opengameart.org/content/water-splash-and-sand-footsteps |
| hum_loop | `shieldloop.wav` (downloaded with the electricity set; verify its page), looped, an octave down, low-passed: a failing fluorescent tube's buzz |
| door_unlock, door_open, door_close | qubodup, "Door open, door close" (a room's door recorded opening and closing, 2009): door-09 (two latch clicks), door-06 (the handle and latch as it opens), door-01 (the heaviest close) — https://opengameart.org/content/door-open-door-close |

## Voices

Not from OpenGameArt: generated with ElevenLabs, whose terms depend on the plan it was made on
(check them before the game is published).

| Engine sound | Source |
|---|---|
| voice_doctor | ElevenLabs, voice "Monique - Empathetic, Calm & Nurturing", made 2026-11-25: "Good morning! Please stay calm and remain in your room. A doctor will be with you shortly." |
| voice_doctor_1..3 | a PA chime (synthesized) and voice_doctor: friendly, as it is (1), cursed (2), more cursed (3): `tools/make_glitch_voice.py voice_doctor` |

## Generated

Made by a script in this repository (no recording): what a door keypad's piezo buzzer really makes.

| Engine sound | Source |
|---|---|
| keypad_beep, keypad_accept, keypad_deny | `tools/make_keypad_sounds.py`: a key's click and beep, the right code's two tones, a wrong code's three low beeps |
