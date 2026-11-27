"""What a streamed level brings beside its entities and lights, and takes away again: its particle
emitters, its footsteps (heard while the player is in it), its water (placed and turned with it).

Two small maps are written for the run (into DIR): A, where the player starts on a tiled floor, streams
in B beside it, turned 90 degrees. B has a sand floor with dusty steps, a smoke emitter and a shallow pool;
its far end asks for it to be taken out. The player walks from A into B, through the pool and to the far
end, then back into A. Checked from the log: B streamed in with its 2 emitters and its water, taken out
with them; the steps are A's in A and B's in B; wading only inside the pool as placed (turned: its box
is 100 wide along the walk, not 200 — the walk passes where the unturned box would be wet and must stay
dry there).

    python3 Tests/game/stream_features.py [--out DIR]

Half a minute; a window opens. Exit code 0 when all checked out.
"""
import argparse, json, math, os, re, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SPEED = 0.33


def slab(position, size, colour, solid=True):
    entity = {'Transform': {'position': position, 'rotation': [0, 0, 0], 'scale': size},
              'Components': {'RenderComponent': {'Mesh': 'res/models/cube4.obj',
                                                 'Material': {'diffuse': colour, 'roughness': 0.8}}}}
    if solid:
        entity['Components']['PhysicsComponent'] = {'BoundingBodyType': ['cube'], 'scale': size, 'mass': 0}
    return entity


def maps(out):
    a_path, b_path = os.path.join(out, 'a.json'), os.path.join(out, 'b.json')
    environment = {'ambient': [0.5, 0.5, 0.5], 'exposure': 1.0}
    a = {'ConfigFilePath': 'res/configs/config.json', 'gravity': -10.0,
         'PlayerStart': {'position': [0, 31, 0], 'yaw': -90, 'pitch': 0},
         'Environment': environment, 'Lights': [],
         'Entities': [slab([0, -1, 0], [400, 2, 400], [0.6, 0.6, 0.6]),
                      slab([0, 73, 0], [400, 2, 400], [0.5, 0.5, 0.5], solid=False)],
         'Footsteps': {'sound': 'step_sneaker', 'variants': 10, 'volume': 0.8},
         'Interaction': {'triggers': [{'box': [[-60, 0, -60], [60, 70, 60]], 'once': True,
                                       'on': [{'streamLevel': b_path,
                                               'attach': {'here': [200, 0, 0], 'there': [0, 0, 0], 'turn': 90}}]}]}}
    # B in its own coordinates: along +z (turned 90 degrees: along the world's +x, from x 200 to 600)
    b = {'ConfigFilePath': 'res/configs/config.json', 'gravity': -10.0,
         'PlayerStart': {'position': [0, 31, 20], 'yaw': 0, 'pitch': 0},
         'Environment': environment, 'Lights': [],
         'Entities': [slab([0, -1, 200], [300, 2, 400], [0.7, 0.6, 0.45]),
                      slab([0, 73, 200], [300, 2, 400], [0.5, 0.5, 0.5], solid=False)],
         'ParticleEmitters': [{'type': 'smoke', 'position': [60, 5, 150], 'count': 20, 'radius': 5}],
         'Footsteps': {'sound': 'step_sand', 'variants': 6, 'wade': 'wade', 'wadeVariants': 3, 'splash': 'splash',
                       'dust': {'type': 'spray', 'burst': True, 'count': 30, 'lifetimeMs': 800,
                                'position': [0, 0, 0], 'radius': 4, 'direction': [0, 1, 0], 'speed': [8, 20]}},
         # the pool: 200 across (its x), 100 along (its z) — along the walk, once turned: x 450..550
         'Water': {'height': 5, 'center': [0, 300], 'size': [200, 100]},
         'Interaction': {'triggers': [{'box': [[-150, 0, 360], [150, 70, 400]], 'once': True,
                                       'on': [{'unloadLevel': 'self'}]}]}}
    for path, game in ((a_path, a), (b_path, b)):
        with open(path, 'w') as f:
            json.dump(game, f, indent=1)
    return a_path, b_path


def route():
    steps, frame, at = [], 40, (0.0, 0.0)
    for x in (150, 420, 500, 585, -100):
        steps.append({'frame': frame, 'walkto': [x, 0]})
        length = int(abs(x - at[0]) / SPEED) + 60
        for k in range(0, length, 10):
            steps.append({'frame': frame + k, 'print': True})
        frame += length
        at = (x, 0)
    return steps, frame + 20


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out')
    args = parser.parse_args()
    out = os.path.abspath(args.out or tempfile.mkdtemp(prefix='stream_features_'))
    os.makedirs(out, exist_ok=True)
    a_path, b_path = maps(out)
    steps, frames = route()
    script = os.path.join(out, 'script.json')
    with open(script, 'w') as f:
        json.dump(steps, f)
    env = dict(os.environ, GE_SKIP_MENU='1', GE_BENCH=str(frames), GE_SCENARIO='script', GE_SCRIPT=script,
               GE_SIZE='960x540', GE_AUDIO='1', SDL_AUDIODRIVER='dummy', GE_BENCH_SPIKES='60')
    for name in ('GE_START', 'GE_ENTRY', 'GE_SKIP_ANIM'):
        env.pop(name, None)
    run = subprocess.run(['./ge', a_path], cwd=ROOT, env=env, capture_output=True, text=True, timeout=600)
    log = run.stdout + run.stderr
    with open(os.path.join(out, 'log.txt'), 'w') as f:
        f.write(log)

    failures = []
    if run.returncode != 0:
        failures.append('the engine ended with %d' % run.returncode)
    if not re.search(r'Streamed in %s: \d+ entities, \d+ lights, 2 emitters, water' % re.escape(b_path), log):
        failures.append('B did not stream in with its 2 emitters and its water')
    if not re.search(r'Taking out %s: \d+ entities, \d+ lights, 2 emitters, water' % re.escape(b_path), log):
        failures.append('B was not taken out with its 2 emitters and its water')
    if 'In ' + b_path not in log:
        failures.append('the player was never in B')

    # what is heard where (sounds go on a little after the step: margins)
    samples = []
    for x, z, sounds in re.findall(r'SCRIPT frame \d+ player (\S+) \S+ (\S+) .* sounds (\S*)', log):
        samples.append((float(x), float(z), set(sounds.split(',')) if sounds else set()))
    taken_out = log.find('Taking out ' + b_path)
    far = max(range(len(samples)), key=lambda i: samples[i][0]) if samples else 0   # the far end reached
    def heard(cond, prefix):
        return any(s.startswith(prefix) for x, z, sounds in samples if cond(x) for s in sounds)
    if not heard(lambda x: 280 < x < 420, 'step_sand'):
        failures.append("B's steps not heard in B")
    # before B (the walk in to x 150): A's steps only
    before = [sounds for x, z, sounds in samples[:far] if x < 120]
    if any(s.startswith('step_sand') for sounds in before for s in sounds):
        failures.append("B's steps heard in A")
    # the walk out (to the far end), where the pool is first met: wading starts inside it (x 450),
    # not where the unturned box would begin (x 400)
    outward = samples[:far + 1]
    first_wade = next((x for x, z, sounds in outward if any(s.startswith('wade') for s in sounds)), None)
    if first_wade is None:
        failures.append('no wading in the pool')
    elif not 445 <= first_wade <= 550:
        failures.append('wading began at x %.0f, the pool is x 450..550 (its box not turned with the level?)' % first_wade)
    # back in A, after B went: A's steps again
    back = [sounds for x, z, sounds in samples[len(samples) // 2:] if x < 60]
    if not any(s.startswith('step_sneaker') for sounds in back for s in sounds):
        failures.append("A's steps not heard back in A")
    if any(s.startswith('step_sand') for sounds in back[-5:] for s in sounds):
        failures.append("B's steps still heard in A after it went")
    if taken_out < 0:
        failures.append('B never taken out')
    for x, z, px, pz in re.findall(r'STUCK (\S+) (\S+) at (\S+) (\S+)', log):
        failures.append('stuck walking to %s %s, at %s %s' % (x, z, px, pz))

    print('maps and log in', out)
    for failure in failures:
        print('FAIL', failure)
    print('OK' if not failures else '%d failed' % len(failures))
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
