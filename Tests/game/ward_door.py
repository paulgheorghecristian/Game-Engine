"""Plays level 1's way out as a player would, and checks it: standing at the ward door's keypad, a
wrong code is typed (refused), then 0314 (the door swings open), then the player walks out through
the door and comes into the corridor (02_corridor_subject4) by its "from_ward" entry.

    python3 Tests/game/ward_door.py [--out DIR]

The engine runs a scripted benchmark (GE_SCENARIO=script, the steps written below): it opens a window
for about half a minute. Pictures of the way go to DIR (a new temporary folder if not given):
aim_key.png (the aim on a key), door_opening.png, door_open.png, walking_out.png, corridor.png.
Exit code 0 when everything checked out.
"""
import argparse, json, os, re, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LEVEL = 'Game/recursion/01_beginning_astra.json'

# the keys' middles (Game/recursion/astra/room_keypad_print_keypad_key_*.obj: the faces at z 84.6)
KEY = {'0': (18.6, 30.71), '1': (19.53, 33.35), '2': (18.6, 33.35), '3': (17.67, 33.35),
       '4': (19.53, 32.47), '5': (18.6, 32.47), '6': (17.67, 32.47), '7': (19.53, 31.59),
       '8': (18.6, 31.59), '9': (17.67, 31.59)}


def script(out):
    steps, frame = [], 40
    for code in ('1111', '0314'):
        for digit in code:
            x, y = KEY[digit]
            steps.append({'frame': frame, 'look': [x, y, 84.6], 'map': '01_beginning'})
            steps.append({'frame': frame + 9, 'print': True, 'map': '01_beginning'})
            if frame == 40:
                steps.append({'frame': frame + 9, 'shot': os.path.join(out, 'aim_key.png'), 'map': '01_beginning'})
            steps.append({'frame': frame + 10, 'click': True, 'map': '01_beginning'})
            steps.append({'frame': frame + 13, 'print': True, 'map': '01_beginning'})
            frame += 20
        frame += 40
    for k in range(-30, 120, 5):            # the code's answer, the door unlocking and starting to move
        steps.append({'frame': frame + k, 'print': True, 'map': '01_beginning'})
    steps.append({'frame': frame + 60, 'shot': os.path.join(out, 'door_opening.png'), 'map': '01_beginning'})
    # out through the doorway (its middle, beyond the wall)
    walk = frame + 320
    steps.append({'frame': walk - 20, 'look': [0, 31, 100], 'map': '01_beginning'})
    steps.append({'frame': walk - 10, 'shot': os.path.join(out, 'door_open.png'), 'map': '01_beginning'})
    steps.append({'frame': walk, 'hold': 'w', 'frames': 400, 'map': '01_beginning'})
    for k in range(0, 400, 20):
        steps.append({'frame': walk + k, 'print': True, 'map': '01_beginning'})
    steps.append({'frame': walk + 60, 'shot': os.path.join(out, 'walking_out.png'), 'map': '01_beginning'})
    # in the corridor: where the player is, after the fade in
    steps.append({'frame': 3, 'print': True, 'map': '02_corridor'})
    steps.append({'frame': 20, 'print': True, 'map': '02_corridor'})
    steps.append({'frame': 150, 'print': True, 'map': '02_corridor'})
    steps.append({'frame': 150, 'shot': os.path.join(out, 'corridor.png'), 'map': '02_corridor'})
    return steps, walk + 450


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', help='where the pictures go')
    args = parser.parse_args()
    out = os.path.abspath(args.out or tempfile.mkdtemp(prefix='ward_door_'))
    os.makedirs(out, exist_ok=True)
    steps, frames = script(out)
    script_path = os.path.join(out, 'script.json')
    with open(script_path, 'w') as f:
        json.dump(steps, f, indent=1)

    env = dict(os.environ, GE_SKIP_MENU='1', GE_SKIP_ANIM='1', GE_BENCH=str(frames), GE_SCENARIO='script',
               GE_SCRIPT=script_path, GE_START='18.6,31,64,180,0', GE_SIZE='1280x720',
               # the sounds mixed (to check what is heard) on SDL's silent device, not the speakers
               GE_AUDIO='1', SDL_AUDIODRIVER='dummy')
    env.pop('GE_ENTRY', None)
    run = subprocess.run(['./ge', LEVEL], cwd=ROOT, env=env, capture_output=True, text=True, timeout=300)
    log = run.stdout + run.stderr
    with open(os.path.join(out, 'log.txt'), 'w') as f:
        f.write(log)

    failures = []
    lines = re.findall(r'SCRIPT frame \d+ player (\S+) (\S+) (\S+) aim (\S+) fade (\S+) sounds (\S*)', log)
    prints = [tuple(map(float, m[:5])) for m in lines]
    before_press = prints[0:16:2]
    if len([p for p in before_press if p[3] >= 0.9]) < 8:
        failures.append('the aim was not on a key within reach before each press (%s)' % [p[3] for p in before_press])
    heard = ','.join(m[5] for m in lines)
    for sound in ('keypad_beep', 'keypad_deny', 'keypad_accept', 'door_unlock', 'door_open', 'door_close'):
        if sound not in heard:
            failures.append('%s not heard' % sound)
    if 'To Game/recursion/02_corridor_subject4.json by from_ward' not in log:
        failures.append('the player did not leave through the door to the corridor')
    corridor = prints[-2:] if len(prints) >= 2 else []
    if not corridor or abs(corridor[-1][0] - (-19.84)) > 6 or abs(corridor[-1][2] - 198.4) > 6:
        failures.append('not in the corridor by its from_ward entry (last: %s)' % (corridor[-1:] or 'nothing'))
    elif corridor[-1][4] >= 0:
        failures.append('still fading in at the end (%.2f)' % corridor[-1][4])
    for name in ('aim_key', 'door_opening', 'door_open', 'walking_out', 'corridor'):
        if not os.path.exists(os.path.join(out, name + '.png')):
            failures.append('no picture %s.png' % name)

    print('pictures and log in', out)
    for failure in failures:
        print('FAIL', failure)
    print('OK' if not failures else '%d failed' % len(failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
