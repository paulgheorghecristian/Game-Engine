"""compare.py <old engine folder> <new engine folder> <out folder> [name part]: the same views
rendered by an old and a new engine build (each from its own folder, with its own shaders and maps),
the old one twice (the noise between two runs). A view fails when areas of the picture change beyond
that noise; look at <out>/<view>_old.png and _new.png to judge a change. [name part]: only the views
whose name has it. Run from anywhere; each build runs in its own folder. An old build to compare with:

    git worktree add --detach /tmp/ge-old <commit>
    rm -rf /tmp/ge-old/res/libraries && ln -s $PWD/res/libraries /tmp/ge-old/res/libraries
    (cd /tmp/ge-old && make -j8)
    python3 Tests/render/compare.py /tmp/ge-old . /tmp/ge-compare

(res/libraries holds untracked libraries the build needs: linked, never deleted from the repository.)"""
import sys, os, subprocess
from PIL import Image, ImageChops, ImageStat
OLD, NEW, OUT = sys.argv[1:4]
os.makedirs(OUT, exist_ok=True)
ARCHIVE = 'Game/recursion/00_archive_ignition.json'
CASES = [(ARCHIVE, {'GE_SEEK': t, 'GE_HOLD': '1'}, 'archive_t%s' % t) for t in ('2', '8', '12', '14', '16.5', '20', '33')] + [
    (ARCHIVE, {'GE_SEEK': '20', 'GE_HOLD': '1', 'GE_START': '500,25,-775,16.8,-33'}, 'archive_mirror'),
    ('Game/lagoon_bh.json', {'GE_START': '757,49.3,131.2,66,26.6'}, 'lagoon_bh_left'),
    ('Game/lagoon_bh.json', {'GE_START': '757,49.3,131.2,156,26.6'}, 'lagoon_bh_right'),
    ('Game/lagoon_bh.json', {'GE_START': '757,49.3,131.2,120,22', 'GE_TIME_OF_DAY': '0.0'}, 'lagoon_bh_night_hole'),
    ('Game/lagoon_bh.json', {'GE_START': '757,49.3,131.2,185,0', 'GE_TIME_OF_DAY': '0.0'}, 'lagoon_bh_night_edge'),
    ('Game/canyon.json', {}, 'canyon'), ('Game/lagoon.json', {}, 'lagoon'), ('Game/lot.json', {}, 'lot'),
    ('Game/recursion/06_dawn.json', {}, 'dawn'), ('Game/recursion/04_tunnels.json', {}, 'tunnels')]
def shot(folder, m, env, path):
    e = dict(os.environ, GE_CLEAN_SHOTS='1', GE_SCREENSHOT=path, GE_BENCH='60', GE_SCENARIO='static', GE_SIZE='1920x1080', **env)
    subprocess.run(['timeout', '200', './ge', m], cwd=folder, env=e, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def blocks(a, b):
    x = ImageChops.difference(Image.open(a).convert('RGB'), Image.open(b).convert('RGB')).convert('L')
    small = x.point(lambda v: 255 if v > 12 else 0).resize((120, 68), Image.BOX)
    return sum(1 for v in small.getdata() if v > 40), sum(1 for v in x.getdata() if v > 12)
ONLY = sys.argv[4] if len(sys.argv) > 4 else ''
CASES = [c for c in CASES if ONLY in c[2]]
bad = 0
for m, env, name in CASES:
    p = {k: os.path.join(OUT, '%s_%s.png' % (name, k)) for k in ('old', 'old2', 'new')}
    shot(OLD, m, env, p['old']); shot(OLD, m, env, p['old2']); shot(NEW, m, env, p['new'])
    if not all(os.path.exists(v) for v in p.values()):
        print('%-22s NO PICTURE' % name); bad += 1; continue
    nb, npx = blocks(p['old'], p['old2']); cb, cpx = blocks(p['old'], p['new'])
    ok = cb <= nb
    bad += not ok
    print('%-22s %s  noise: %d areas (%d px)   old vs new: %d areas (%d px)' % (name, 'same   ' if ok else 'CHANGED', nb, npx, cb, cpx), flush=True)
print('\n%d of %d views the same' % (len(CASES) - bad, len(CASES)))
