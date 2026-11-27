"""Walks the two connected levels as a player would, from the ward's own spawn: looks round the ward,
walks to the keypad, opens the door, goes out into the streamed-in corridor, walks it to the far end
(past Subject 4's window), comes back and tries the ward door (shut by then, the ward taken out: the
corridor's own door must stop them). Pictures at each stop, for looking at; the log checked for what
went wrong: a stall (a frame over 40 ms), a fall through the floor, a walk that got stuck where it
should not, the levels not streaming in and out, the frame rate.

    python3 Tests/game/walk_levels.py [--out DIR]

About a minute; a window opens. Pictures and log.txt in DIR (a new temporary folder if not given).
Exit code 0 when all checked out.
"""
import argparse, json, math, os, re, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
LEVEL = 'Game/recursion/01_beginning_astra.json'
CORRIDOR = 'Game/recursion/02_corridor_subject4.json'
SPAWN = (-33.0, -12.0)          # the ward's PlayerStart (x, z)
SPEED = 0.33                    # world units a frame, walking (a little under what it is)
KEY = {'0': (18.6, 30.71), '1': (19.53, 33.35), '3': (17.67, 33.35), '4': (19.53, 32.47)}
# the corridor, where it is put in the world (its middle line z 121; from x -100 to 498, the exit sign end)
CORRIDOR_Z = 121.0


class Route:
    def __init__(self, out):
        self.steps, self.frame, self.out, self.at = [], 40, out, SPAWN
        self.expected_stuck = []

    def step(self, **kw):
        kw['frame'] = self.frame
        self.steps.append(kw)

    def wait(self, frames):
        self.frame += frames

    def shot(self, name, look):
        self.step(look=list(look))
        self.wait(6)
        self.step(shot=os.path.join(self.out, name + '.png'), print=True)
        self.wait(4)

    def walk(self, x, z, stuck_ok=False):
        self.step(walkto=[x, z])
        distance = math.hypot(x - self.at[0], z - self.at[1])
        if stuck_ok:
            self.expected_stuck.append((x, z))
        for k in range(0, int(distance / SPEED) + 60, 30):
            self.steps.append({'frame': self.frame + k, 'print': True})
        self.wait(int(distance / SPEED) + 60)
        self.at = (x, z)


def route(out):
    r = Route(out)
    # the ward, all round, from where the player stands up
    for name, look in (('ward_north', (-33, 31, -112)), ('ward_east', (67, 31, -12)),
                       ('ward_south', (-33, 31, 88)), ('ward_west', (-133, 31, -12))):
        r.shot(name, look)
    r.walk(0, 30)
    r.walk(18.6, 66)
    r.shot('keypad', (18.6, 32.6, 84.6))
    for digit in '0314':
        x, y = KEY[digit]
        r.step(look=[x, y, 84.6])
        r.wait(9)
        r.step(click=True)
        r.wait(20)
    r.wait(60)
    r.shot('door_opening', (0, 35, 89))
    r.wait(240)
    r.shot('door_open', (0, 31, 120))
    # out, and both ways along the corridor
    r.walk(0, 108)
    r.shot('corridor_west', (-100, 31, CORRIDOR_Z))
    r.shot('corridor_east', (300, 31, CORRIDOR_Z))
    r.walk(150, CORRIDOR_Z)
    r.shot('corridor_150', (400, 31, CORRIDOR_Z))
    r.walk(255, CORRIDOR_Z)
    r.shot('subject4_window', (260, 40, 160))
    r.walk(470, CORRIDOR_Z)
    r.shot('corridor_end', (520, 31, CORRIDOR_Z))
    r.shot('corridor_back', (0, 31, CORRIDOR_Z))
    # back to the ward door (shut, the ward gone): it must stop them
    r.walk(10, CORRIDOR_Z)
    r.shot('ward_door_from_corridor', (0, 31, 89))
    r.walk(0, 70, stuck_ok=True)
    r.shot('at_the_door', (0, 31, 70))
    r.wait(20)
    return r


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', help='where the pictures go')
    args = parser.parse_args()
    out = os.path.abspath(args.out or tempfile.mkdtemp(prefix='walk_levels_'))
    os.makedirs(out, exist_ok=True)
    r = route(out)
    script_path = os.path.join(out, 'script.json')
    with open(script_path, 'w') as f:
        json.dump(r.steps, f, indent=1)

    env = dict(os.environ, GE_SKIP_MENU='1', GE_SKIP_ANIM='1', GE_BENCH=str(r.frame), GE_SCENARIO='script',
               GE_SCRIPT=script_path, GE_SIZE='1280x720', GE_BENCH_SPIKES='40',
               GE_AUDIO='1', SDL_AUDIODRIVER='dummy')
    for name in ('GE_START', 'GE_ENTRY'):
        env.pop(name, None)
    run = subprocess.run(['./ge', LEVEL], cwd=ROOT, env=env, capture_output=True, text=True, timeout=600)
    log = run.stdout + run.stderr
    with open(os.path.join(out, 'log.txt'), 'w') as f:
        f.write(log)

    failures = []
    if run.returncode != 0:
        failures.append('the engine ended with %d' % run.returncode)
    for said, what in (('Streamed in ' + CORRIDOR, 'the corridor did not stream in'),
                       ('In ' + CORRIDOR, 'the player was never in the corridor'),
                       ('Taking out ' + LEVEL, 'the ward was not taken out')):
        if said not in log:
            failures.append(what)
    heights = [float(m) for m in re.findall(r'SCRIPT frame \d+ player \S+ (\S+) \S+', log)]
    if heights and min(heights) < 20:
        failures.append('the player fell (lowest %.1f)' % min(heights))
    for x, z, px, pz in re.findall(r'STUCK (\S+) (\S+) at (\S+) (\S+)', log):
        if (float(x), float(z)) not in r.expected_stuck:
            failures.append('stuck walking to %s %s, at %s %s' % (x, z, px, pz))
    door = [m for m in re.findall(r'WALKED (\S+) (\S+)', log) if (float(m[0]), float(m[1])) in r.expected_stuck]
    if door:
        failures.append('walked through the shut ward door')
    # the steps heard: the ward's and the corridor's hard floors (the levels name them), never grass
    heard = set(','.join(re.findall(r'SCRIPT frame .* sounds (\S*)', log)).split(','))
    if not any(s.startswith('step_sneaker') for s in heard):
        failures.append('no steps heard on the floors (step_sneaker_*)')
    wrong = sorted(s for s in heard if s.startswith('step_') and not s.startswith('step_sneaker'))
    if wrong:
        failures.append('other steps heard: %s' % wrong)
    spikes = re.findall(r'SPIKE frame (\d+) ([\d.]+)ms', log)
    if spikes:
        failures.append('frames over 40 ms: %s' % spikes[:6])
    bench = re.search(r'BENCH .* avg=([\d.]+)ms \(([\d.]+) fps\).*p99=([\d.]+)ms', log)
    if bench:
        print('frame time avg %s ms (%s fps), p99 %s ms' % bench.groups())
    for step in r.steps:
        if 'shot' in step and not os.path.exists(step['shot']):
            failures.append('no picture %s' % os.path.basename(step['shot']))

    print('pictures and log in', out)
    for failure in failures:
        print('FAIL', failure)
    print('OK' if not failures else '%d failed' % len(failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
