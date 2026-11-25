"""run_all.py [suite ...]: the timeline editor's test suites, used like a person through a browser
(Playwright) driving the engine. Run from the repository root:

    python3 Tests/editor/run_all.py              # every suite
    python3 Tests/editor/run_all.py solo noises  # some

Each suite starts tools/timeline_editor.py on its own port (8900 and up, away from an editor you
may have open on 8765), which opens a small engine window. guard.py keeps a copy of every map and
timeline file and puts back anything a suite changed. Screenshots and scratch files go to
$GE_TEST_OUT (default /tmp/ge-tests). Needs: python3 -m pip install playwright; playwright install."""
import glob, hashlib, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
SUITES = ['core', 'sounds', 'solo', 'mapsounds', 'noises']

def files():
    paths = sorted(glob.glob('Game/*.json') + glob.glob('Game/recursion/*.json') + glob.glob('Game/sequences/*.json'))
    return {p: hashlib.md5(open(p, 'rb').read()).hexdigest() for p in paths}

if not os.path.exists('tools/timeline_editor.py'):
    sys.exit('run from the repository root')
chosen = sys.argv[1:] or SUITES
before = files()
total_passed = total = 0
failed_suites = []
for i, suite in enumerate(chosen):
    port = 8900 + i
    start = time.time()
    run = subprocess.run([sys.executable, os.path.join(HERE, suite + '.py'), str(port)],
                         capture_output=True, text=True, timeout=1200,
                         env=dict(os.environ, PYTHONDONTWRITEBYTECODE='1'))   # (no __pycache__ in Tests/)
    out = run.stdout + run.stderr
    m = re.findall(r'(\d+) of (\d+) checks passed', out)
    if m:
        passed, count = map(int, m[-1])
        total_passed += passed; total += count
        print('%-10s %3d of %3d checks passed  (%.0f s)' % (suite, passed, count, time.time() - start), flush=True)
        if passed < count:
            failed_suites.append(suite)
            for line in out.splitlines():
                if line.startswith('FAIL'):
                    print('    ' + line[:200])
    else:
        failed_suites.append(suite)
        print('%-10s NO RESULT  (%.0f s)\n%s' % (suite, time.time() - start, '\n'.join('    ' + l for l in out.splitlines()[-12:])),
              flush=True)
after = files()
changed = sorted(p for p in set(before) | set(after) if before.get(p) != after.get(p))
print('\n%d of %d checks passed in %d suites%s' % (total_passed, total, len(chosen),
      '' if not failed_suites else '; failing: ' + ', '.join(failed_suites)))
print('all map and timeline files unchanged' if not changed else 'CHANGED FILES: ' + ', '.join(changed))
sys.exit(1 if failed_suites or changed else 0)
