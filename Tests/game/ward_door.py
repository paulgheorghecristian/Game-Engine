"""Plays level 1's way out as a player would, and checks it: standing at the ward door's keypad, a
wrong code is typed (refused), then 0314: the door swings open and the corridor (02_corridor_subject4)
streams in beyond it, where it really is. The player walks out through the door and down the corridor;
the door's closer shuts it behind them, and the ward, out of reach, is taken out of the world (the
corridor's own door standing where the ward's did). All in one run of the engine: no loading.

    python3 Tests/game/ward_door.py [--out DIR]

The engine runs a scripted benchmark (GE_SCENARIO=script, the steps written below): it opens a window
for about half a minute. Pictures of the way go to DIR (a new temporary folder if not given):
aim_key.png (the aim on a key), door_opening.png, door_open.png (the corridor through it),
in_corridor.png, door_shut.png (looking back once the ward is gone). Exit code 0 when all checked out.
"""
import argparse, json, os, re, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LEVEL = 'Game/recursion/01_beginning_astra.json'
CORRIDOR = 'Game/recursion/02_corridor_subject4.json'

# the keys' middles (Game/recursion/astra/room_keypad_print_keypad_key_*.obj: the faces at z 84.6)
KEY = {'0': (18.6, 30.71), '1': (19.53, 33.35), '2': (18.6, 33.35), '3': (17.67, 33.35),
       '4': (19.53, 32.47), '5': (18.6, 32.47), '6': (17.67, 32.47), '7': (19.53, 31.59),
       '8': (18.6, 31.59), '9': (17.67, 31.59)}


def script(out):
    steps, frame = [], 40
    for code in ('1111', '0314'):
        for digit in code:
            x, y = KEY[digit]
            steps.append({'frame': frame, 'look': [x, y, 84.6]})
            steps.append({'frame': frame + 9, 'print': True})
            if frame == 40:
                steps.append({'frame': frame + 9, 'shot': os.path.join(out, 'aim_key.png')})
            steps.append({'frame': frame + 10, 'click': True})
            steps.append({'frame': frame + 13, 'print': True})
            frame += 20
        frame += 40
    for k in range(-30, 120, 5):            # the code's answer, the door unlocking and starting to move
        steps.append({'frame': frame + k, 'print': True})
    steps.append({'frame': frame + 60, 'shot': os.path.join(out, 'door_opening.png')})
    # out through the doorway (its middle, beyond the wall), into the corridor
    walk = frame + 320
    steps.append({'frame': walk - 20, 'look': [0, 31, 120]})
    steps.append({'frame': walk - 10, 'shot': os.path.join(out, 'door_open.png')})
    steps.append({'frame': walk, 'hold': 'w', 'frames': 150})
    # then down the corridor (it runs along x here: towards the exit sign at +x)
    steps.append({'frame': walk + 150, 'look': [300, 31, 120]})
    steps.append({'frame': walk + 152, 'hold': 'w', 'frames': 160})
    steps.append({'frame': walk + 250, 'shot': os.path.join(out, 'in_corridor.png')})
    for k in range(0, 700, 20):
        steps.append({'frame': walk + k, 'print': True})
    # back to where the door is (the ward gone by now)
    steps.append({'frame': walk + 600, 'look': [0, 31, 89]})
    steps.append({'frame': walk + 640, 'shot': os.path.join(out, 'door_shut.png')})
    return steps, walk + 700


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
               GE_BENCH_SPIKES='40',
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
    for said, what in (('Streamed in ' + CORRIDOR, 'the corridor did not stream in'),
                       ('In ' + CORRIDOR, 'the player was never in the corridor'),
                       ('Taking out ' + LEVEL, 'the ward was not taken out once its door shut')):
        if said not in log:
            failures.append(what)
    if 'To Game/' in log:
        failures.append('a level was loaded the old way (the engine restarted)')
    if prints:
        x, y, z = prints[-1][:3]
        if z < 95 or y < 20:
            failures.append('not in the corridor at the end (%.1f %.1f %.1f)' % (x, y, z))
    spikes = re.findall(r'SPIKE frame (\d+) ([\d.]+)ms', log)
    if spikes:
        failures.append('frames over 40 ms: %s' % spikes[:5])
    for name in ('aim_key', 'door_opening', 'door_open', 'in_corridor', 'door_shut'):
        if not os.path.exists(os.path.join(out, name + '.png')):
            failures.append('no picture %s.png' % name)

    print('pictures and log in', out)
    for failure in failures:
        print('FAIL', failure)
    print('OK' if not failures else '%d failed' % len(failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
