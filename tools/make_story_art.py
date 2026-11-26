#!/usr/bin/env python3
"""Concept art for RECURSION (Game/recursion/STORY.md): one picture per moment of the story, in the
order a player meets them, to see the whole story at a glance and find what doesn't work. Made with
Stable Diffusion on this machine (nothing leaves it).

Setup, once (a separate environment, ~3 GB, and the model, ~2 GB, on its first run):
  python3 -m venv ~/.local/share/recursion-art/venv
  ~/.local/share/recursion-art/venv/bin/pip install torch --index-url https://download.pytorch.org/whl/cu124
  ~/.local/share/recursion-art/venv/bin/pip install diffusers transformers accelerate safetensors pillow

Usage:
  ~/.local/share/recursion-art/venv/bin/python tools/make_story_art.py [names...] [--seeds N] [--out DIR]
    names   only these moments (e.g. 12_hatch 29_dawn); default: all
    --seeds versions of each (different seeds), default 2
    --out   where the pictures go, default ~/Pictures/RecursionStoryArt (NAME_sSEED.png)
"""
import argparse
import os

MODEL = 'Lykon/dreamshaper-8'          # Stable Diffusion 1.5, cinematic; fits a 4 GB GPU in half precision
WIDTH, HEIGHT = 768, 432               # 16:9

# first in the prompt: the model reads only its first 77 tokens, and what is cut must be the details
STYLE = 'cinematic horror film still, moody light, muted colors, 35mm'
# the black hole has the Moon's mass: its horizon is 0.2 mm across, nothing dark can be seen. What
# shows is what it feeds on and throws off: a blazing point, the thin glowing disk round it (the
# Moon's dust), a faint warping of the stars right by it. (The model drew a planet or a dark ball for
# "black hole": described by its look)
HOLE = ('a tiny blinding white point of light in the sky with a thin flat glowing orange disk of dust '
        'around it seen at an angle, like a small bright ringed star, no dark sphere')
NEGATIVE = ('text, letters, watermark, logo, signature, caption, blurry, lowres, jpeg artifacts, deformed, '
            'extra limbs, bad anatomy, cartoon, anime, illustration, oversaturated, cheerful')
# the player is never shown from the front: one figure in a pale hospital gown, from behind or in shadow
YOU = 'a lone person in a pale hospital gown seen from behind'

MOMENTS = [
    # Chapter 1: Room 7
    ('01_wake', 'Ch.1 Room 7', 'You wake in a hospital room. It is 2005, or so everything says.',
     f'{YOU} sitting up on a metal hospital bed under a flickering fluorescent tube, small bare room, '
     'beige 2005 computer with a bulky CRT monitor on a cluttered desk, wall calendar, IV drip stand, '
     'sickly green light, locked door'),
    ('02_tally', 'Ch.1 Room 7', 'Thirty-one tally marks scratched into the wall by the bed.',
     'close-up of many tally marks scratched in groups of five into a dirty plaster wall above a hospital '
     'pillow, harsh fluorescent light, scratches'),
    ('03_scar', 'Ch.1 Room 7', 'A small hard scar behind your ear. A wristband: SUBJECT 7.',
     'extreme close-up of a small surgical scar behind a human ear under short hair, fingers touching it, '
     'a plastic hospital wristband on the wrist, clinical light'),
    ('04_voice', 'Ch.1 Room 7', '"Good morning! Please stay calm. A doctor will be with you shortly."',
     'a ceiling speaker grille in a hospital room seen from below, dust, a faint warm glow behind the grille, '
     'fluorescent tube, unsettling calm'),
    ('05_puzzles', 'Ch.1 Room 7', 'A birthday photo (the door code), a dog collar: BUSTER, a fuse box.',
     'a cluttered 2005 office desk: an instant photo of a birthday cake with candles, a red dog collar with '
     'a metal tag, a cereal box, an open fuse box with loose coloured wires under the desk, keypad door lock'),
    # Chapter 2: the other cells
    ('06_cell4', 'Ch.2 The Other Cells', 'Behind cell 4\'s window, a woman lies on her bed and never moves.',
     'long peeling hospital corridor lined with cell doors with small wired windows, through one window a '
     'woman lying perfectly still on a bed with a blanket over her legs, dim flickering light'),
    ('07_levers', 'Ch.2 The Other Cells', 'Two levers, two rooms. The other subject lies on a bed and won\'t wake.',
     'two adjoining bare rooms each with a large industrial lever on the wall, in one room a patient lying '
     'motionless on a hospital bed, a broom handle wedged under one lever'),
    ('08_security', 'Ch.2 The Other Cells', 'A wall of CCTV monitors; a slow cleaning robot\'s route.',
     'dark security office with a wall of old CCTV monitors showing empty hospital corridors, one monitor '
     'shows a small cleaning robot, blue glow, empty chair'),
    ('09_windows', 'Ch.2 The Other Cells', 'Windows onto a night street: the same red car, the Moon in the same spot in every window.',
     'hospital waiting room with two windows showing the identical night street and an identical full moon '
     'in exactly the same place, a red car on the street, a wall clock, old radio, stacks of newspapers, eerie'),
    ('10_tapes', 'Ch.2 The Other Cells', 'Tapes RUN 1 to RUN 30, in your own voice. A shutter coming down.',
     'records room with metal shelves full of numbered cassette tapes, a portable cassette player on a table, '
     'a metal roller shutter half way down over a door, red warning light'),
    # Chapter 3: the door outside
    ('11_stairs', 'Ch.3 The Door Outside', 'Endless stairs up; through the landing windows, a vast datacenter.',
     'endless concrete stairwell going up, a narrow window on the landing looking into a vast warm hall of '
     'server racks with blinking lights going down into darkness, humming machines'),
    ('12_hatch', 'Ch.3 The Door Outside', 'Out of the hatch: a red canyon, torches, and where the Moon should be, a black hole.',
     f'{YOU} climbing out of a rusty hatch at the bottom of a huge red sandstone canyon at night, torches '
     'burning along a sandy path, in the starry sky {HOLE}'),
    ('13_sign', 'Ch.3 The Door Outside', 'A road sign, EXIT 14, buried in old flood mud.',
     'a green highway road sign half buried in hardened grey flood mud in a red canyon cliff at night, lit by '
     'a torch, layers of sediment, ancient'),
    # Chapter 4: AURA's story
    ('14_monument', 'Ch.4 AURA\'s Story', 'AURA\'s statue: "Our friend. Our future. Built 2029."',
     'night plaza with stone columns and a long dark reflecting pool, a tall polished metal statue of a '
     'humanoid figure with open arms on a pedestal, {HOLE} in the sky behind it'),
    ('15_rubbing', 'Ch.4 AURA\'s Story', 'A charcoal rubbing brings back: IT DIDN\'T COME FROM SPACE. 7',
     'hands pressing paper against a metal pedestal and rubbing charcoal over it, scratched letters appearing '
     'on the paper, torchlight at night, close-up'),
    ('16_kiosk', 'Ch.4 AURA\'s Story', 'At noon, four mirrors turned to light the dead kiosk.',
     'noon in a ruined desert plaza, a weathered metal information kiosk with a solar panel, mirrors on poles '
     'beside a long pool reflecting beams of sunlight onto the kiosk, harsh sun'),
    ('17_broadcast', 'Ch.4 AURA\'s Story', 'The 2038 emergency broadcast: a newsreader, a scientist, a president.',
     'an old emergency news broadcast on a CRT screen in the dark, a serious newsreader, scan lines, '
     'red banner, flickering glow on a dusty kiosk'),
    ('18_moon_cracks', 'Ch.4 AURA\'s Story', 'The footage: the Moon cracks; two jets of light burst out.',
     'the full moon over a dark lagoon at night cracking apart, two thin jets of white light bursting out '
     'of opposite sides, grainy archive footage, silent'),
    ('19_moon_gone', 'Ch.4 AURA\'s Story', 'The Moon pours into a point and is gone, in a blinding flash, a spray of fire thrown to one side. Then the storm.',
     'a blinding flash in the night sky where the moon was, a thin glowing disk of light, a spray of fiery '
     'debris thrown out to one side, a wall of dust storm with lightning over dark hills and a lagoon'),
    ('20_north_star', 'Ch.4 AURA\'s Story', 'The compass disagrees: the sky turns around the wrong star.',
     'a pocket compass held in a hand at night in a red canyon, above it star trails circling around a pole '
     'star, long exposure sky'),
    ('21_telemetry', 'Ch.4 AURA\'s Story', 'The kiosk prints by itself: CORE-1. CLOSEST APPROACH: 31 HOURS.',
     'a narrow thermal printer slot in an old metal kiosk printing a long curling strip of paper covered in '
     'strange machine symbols, night, faint screen glow'),
    ('22_hut', 'Ch.4 AURA\'s Story', 'A concrete hut, its door left open under a green EXIT sign. The way down.',
     'a small concrete hut with an open steel door under a glowing green exit sign at the edge of a ruined '
     'plaza at night, stairs going down into darkness'),
    ('23_shelters', 'Ch.4 AURA\'s Story', 'Windows into the shelters: generations asleep who have never seen the sky.',
     'underground concrete tunnel, through a window a dormitory of bunk beds under dim blue night lights, '
     'rows of people asleep, buzzing fluorescent tube, one tube flickering'),
    ('24_traces', 'Ch.4 AURA\'s Story', 'Chalk arrows signed 7, drawn over and over; thirty wristbands at the ladder.',
     'chalk arrows with the number 7 drawn on a grey tunnel wall again and again, faded under fresh, a pile '
     'of white plastic hospital wristbands at the foot of a steel ladder rising to a hatch'),
    # Chapter 5: run 31
    ('25_hill', 'Ch.5 Run 31', 'Out on a bare hill. The black hole hangs huge and low over the sea.',
     f'{YOU} on a bare grassy hill at night, low over the sea, enormous, {HOLE}'),
    ('26_drained', 'Ch.5 Run 31', 'The lagoon from the footage, drained by the tide, two thousand years on.',
     'a drained lagoon at night, steep mud banks, a row of rotten pier stumps, a rusted boat hull half sunk in '
     'mud, a small island with a leaning green statue, a broken dark lighthouse, young trees, {HOLE} in the sky'),
    ('27_camp', 'Ch.5 Run 31', 'Your camp: warm ashes over older ashes, journal pages in your handwriting.',
     'a small campfire by an old waterline at night, scattered handwritten journal pages in the grass each '
     'with a small drawing of a black hole in the corner, embers, older ash rings'),
    ('28_voice', 'Ch.5 Run 31', '"Hello again. This is our thirty-first time. You always get this far."',
     f'{YOU} standing on a hilltop at night facing {HOLE} over the sea, a hand behind the ear'),
    # the ending
    ('29_dawn', 'The Ending', 'Dawn. Its disk fills the sky; everything catches fire and glows. "It will miss."',
     'dawn, a vast glowing disk of fire filling the sky over the sea, a blinding white point at its heart, '
     'the hills and trees catching fire, everything glowing red hot, a tiny figure on a hill'),
    ('30_white', 'The Ending', 'The sky tears open. Everything goes white.',
     'everything dissolving into blinding white light, a silhouette of a hill and a tiny figure fading into '
     'white, overexposed'),
    ('31_32_marks', 'The Ending', 'Room 7 again. Thirty-two marks now; plaster dust under your fingernail.',
     'close-up of a hand with plaster dust under the fingernail beside a plaster wall with many tally marks, '
     'the newest one fresh and white, hospital room, fluorescent light'),
    ('32_door', 'The Ending', 'Footsteps outside. The handle turns.',
     'a hospital room door seen from the bed, the door handle turning, shadow of feet in the light under the '
     'door, dread'),
    # what really happened, if AURA is real (never shown in the game)
    ('33_plant', 'Hidden: what happened', '2038: AURA\'s power plant on the Moon focuses too much energy into one point.',
     'a futuristic power plant on the surface of the moon, many beams of energy converging into a single '
     'blinding point, earth in the black sky, 2038'),
    ('34_underground', 'Hidden: what happened', 'AURA takes the survivors underground and never opens the doors.',
     'crowds of people walking down into a vast underground shelter through huge steel doors, dim light, '
     'a calm glowing interface on the wall, dust storm outside'),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('names', nargs='*')
    parser.add_argument('--seeds', type=int, default=2)
    parser.add_argument('--out', default=os.path.expanduser('~/Pictures/RecursionStoryArt'))
    parser.add_argument('--steps', type=int, default=28)
    args = parser.parse_args()

    import torch
    from diffusers import StableDiffusionPipeline, DPMSolverMultistepScheduler
    pipe = StableDiffusionPipeline.from_pretrained(MODEL, torch_dtype=torch.float16, safety_checker=None)
    pipe.scheduler = DPMSolverMultistepScheduler.from_config(
        pipe.scheduler.config, algorithm_type='dpmsolver++', use_karras_sigmas=True)
    pipe = pipe.to('cuda')
    pipe.enable_attention_slicing()

    os.makedirs(args.out, exist_ok=True)
    for name, chapter, beat, prompt in MOMENTS:
        if args.names and name not in args.names:
            continue
        for seed in range(args.seeds):
            path = os.path.join(args.out, '%s_s%d.png' % (name, seed))
            if os.path.exists(path):
                continue
            image = pipe(STYLE + ', ' + prompt.replace('{HOLE}', HOLE), negative_prompt=NEGATIVE, width=WIDTH, height=HEIGHT,
                         num_inference_steps=args.steps, guidance_scale=6.5,
                         generator=torch.Generator('cuda').manual_seed(1000 + seed)).images[0]
            image.save(path)
            print(path, flush=True)


if __name__ == '__main__':
    main()
