"""core.py <port>: the timeline editor as it is, used like a person (mouse and keyboard, in Chrome)
while it drives the engine: time (the named moments, the ruler, the slider, Play, the space bar),
editing (an effect's slider, a dot added / dragged / removed, a sound moved, an effect cleared;
Undo, Ctrl+Z, Discard), what the engine applies and plays, zooming, the panel, the picture, the
camera, the engine killed and the page reloaded with a change not saved, and Save as.

What it expects is read from the timeline and the engine (the moments, the effects and their
ranges, the sounds), not written in here: it keeps up with the timeline being edited. Nothing on
disk is left changed (guard.py; Save as writes a file, which is taken away again)."""
import guard
import copy, glob, json, os, re, signal, subprocess, sys, time, urllib.request
from playwright.sync_api import sync_playwright

PORT = sys.argv[1]; B = 'http://localhost:%s' % PORT
OUT = os.environ.get('GE_TEST_OUT', '/tmp/ge-tests') + '/'; os.makedirs(OUT, exist_ok=True)
MAP = 'Game/recursion/00_archive_ignition.json'
SAVE_AS = 'zz_test_core'
server = subprocess.Popen(['python3', 'tools/timeline_editor.py', MAP, '--size', '480x270', '--port', PORT],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def get(path): return json.load(urllib.request.urlopen(B + path, timeout=5))
results = []
def check(name, ok, detail=''):
    results.append((name, bool(ok)))
    print(('PASS  ' if ok else 'FAIL  ') + name + ('  — ' + str(detail) if detail != '' else ''), flush=True)
def near(a, b, tolerance): return a is not None and b is not None and abs(a - b) <= tolerance
MODIFIERS = {'gain', 'gustiness', 'gustVolume', 'boomVolume', 'thudVolume', 'thunderVolume', 'frontDistance'}

try:
    for i in range(90):
        try:
            e = get('/api/engine')
            if e.get('alive') and e.get('running') and 't' in e: break
        except Exception: pass
        time.sleep(1)
    time.sleep(1)
    st = get('/api/state')
    schema = {p['name']: p for p in st['schema']['parameters']}
    with sync_playwright() as p:
        browser = p.chromium.launch(channel='chrome', headless=True)
        page = browser.new_page(viewport={'width': 1700, 'height': 1000})
        errors = []
        page.on('pageerror', lambda e: errors.append(str(e)[:160]))
        def on_dialog(d):
            if d.type == 'prompt': d.accept(SAVE_AS)
            else: d.accept()
        page.on('dialog', on_dialog)
        page.goto(B + '/'); page.wait_for_selector('canvas.lane'); time.sleep(2)

        seq = lambda: copy.deepcopy(page.evaluate('seq()'))
        saved = seq()
        params, sounds = saved.get('params', {}), saved.get('sounds', [])
        engine = lambda: (time.sleep(0.4), get('/api/engine'))[1]
        now = lambda: page.evaluate('nowT')
        start = page.evaluate('seqStart()')
        def wait_engine_at(t, tolerance=0.15, secs=4):
            end = time.time() + secs
            while time.time() < end:
                e = get('/api/engine')
                if near(e.get('t'), t, tolerance): return e
                time.sleep(0.2)
            return get('/api/engine')
        def ruler_x(t):
            b = page.locator('#ruler').bounding_box(); w = page.evaluate('laneWidth()')
            return b['x'] + page.evaluate('px(%f, %d)' % (t, w)) * b['width'] / w, b['y'] + b['height'] / 2
        def in_lane(name, t, value=None):
            """the point on the page over a lane at timeline time t (and value: its height there)"""
            lane = page.locator('canvas.lane[data-name="%s"]' % name)
            lane.scroll_into_view_if_needed(); time.sleep(0.1)
            b = lane.bounding_box()
            x, y, w, h = page.evaluate('([n, t, v]) => { const c = laneOf(n).canvas; '
                                       'return [px(t, c.width), v === null ? c.height * 0.6 : py(n, v), c.width, c.height]; }',
                                       [name, t, value])
            return b['x'] + x * b['width'] / w, b['y'] + y * b['height'] / h
        def value_now(name): return page.evaluate('(n) => valueAt(getValue(n), localNow())', name)
        def status(): return page.inner_text('#status')

        # ---- what the page shows comes from the engine and the timeline
        e = get('/api/engine')
        check('the engine started, plays the map and sends its picture', e.get('alive') and e.get('running') and e.get('frame', 0) > 0, e.get('frame'))
        want = set(params) | {'sound:%d' % i for i in range(len(sounds))}
        got = set(page.evaluate('[...document.querySelectorAll("canvas.lane")].map((c) => c.dataset.name)'))
        check('a lane for each effect the timeline sets and each of its %d sounds' % len(sounds), got == want, sorted(want ^ got))
        moments = e['moments']
        labels = [s.inner_text() for s in page.locator('#marks span').all()]
        check('the labels under the slider are the engine\'s named moments', labels == [m['label'] for m in moments], labels)
        ok = []
        for m in moments:
            page.click('#marks >> text="%s"' % m['label']); time.sleep(0.3)
            en = wait_engine_at(m['t'])
            ok.append(near(now(), m['t'], 0.01) and near(en['t'], m['t'], 0.15) and ('at ' + m['label']) in page.inner_text('#when'))
        check('clicking each label goes there, page and engine, and says "at" it', all(ok), ok)

        # ---- time: ruler, slider, Play and Pause
        x, y = ruler_x(10.0); page.mouse.click(x, y)
        en = wait_engine_at(10.0, 0.2)
        check('a click on the ruler at 10 s: the page and the engine go there', near(now(), 10.0, 0.15) and near(en['t'], 10.0, 0.2), (now(), en['t']))
        x0, y0 = ruler_x(5.0); x1, _ = ruler_x(15.0)
        page.mouse.move(x0, y0); page.mouse.down()
        for k in range(1, 11): page.mouse.move(x0 + (x1 - x0) * k / 10, y0); time.sleep(0.03)
        page.mouse.up()
        en = wait_engine_at(15.0, 0.2)
        check('dragged along the ruler from 5 s to 15 s: both follow to 15 s', near(now(), 15.0, 0.15) and near(en['t'], 15.0, 0.2), (now(), en['t']))
        page.focus('#scrub')
        for k in range(4): page.keyboard.press('ArrowRight')
        time.sleep(0.3)
        en = wait_engine_at(15.2, 0.1)
        check('the slider with the arrow keys: a step on, the engine with it', near(now(), 15.2, 0.03) and near(en['t'], 15.2, 0.1), (now(), en['t']))
        page.click('#play'); t0 = get('/api/engine')['t']; time.sleep(2.0)
        en = get('/api/engine')
        check('Play: the engine runs on in real time, the page follows it, the button says Pause',
              1.4 <= en['t'] - t0 <= 2.8 and not en['paused'] and near(now(), en['t'], 0.4) and page.inner_text('#play') == 'Pause',
              (round(en['t'] - t0, 2), en['paused'], now(), page.inner_text('#play')))
        page.mouse.click(5, 5)            # (away from the buttons: the space bar is the page's)
        page.keyboard.press('Space'); time.sleep(0.6)
        a = get('/api/engine'); time.sleep(1.0); b = get('/api/engine')
        check('the space bar pauses: the engine stands still there, the button says Play',
              a['paused'] and b['paused'] and near(a['t'], b['t'], 0.02) and page.inner_text('#play') == 'Play', (a['t'], b['t'], a['paused']))

        # ---- an effect's slider; Undo; Ctrl+Z
        T = moments[-1]['t'] if moments else 20.0
        page.click('#marks >> text="%s"' % moments[-1]['label']) if moments else None; time.sleep(1.0)
        # an effect the engine applies as the timeline has it (not one others change on the way, as
        # calm does wind): what it applies there is its value
        applied0 = get('/api/engine')['applied']
        def plain(n):
            lo_hi = page.evaluate('(n) => rangeOf(n)', n)[:2]
            return near(applied0.get(n), value_now(n), (lo_hi[1] - lo_hi[0]) * 0.005)
        name = next((n for n in params if n not in MODIFIERS and n in schema and n in applied0 and plain(n)), None)
        print('   the effect moved: %s' % name)
        row = page.locator('.effect', has=page.locator('.name', has_text=re.compile('^%s$' % re.escape(name)))).first
        page.click('[data-tab="effects"]')
        lo, hi = page.evaluate('(n) => rangeOf(n)', name)[:2]
        before = value_now(name)
        rng = row.locator('input[type=range]'); rng.scroll_into_view_if_needed(); rng.focus()
        key = 'ArrowRight' if before < (lo + hi) / 2 else 'ArrowLeft'
        for k in range(60): page.keyboard.press(key)
        time.sleep(1.2)
        after = value_now(name); applied = engine()['applied'][name]
        check('%s\'s slider moved with the keyboard: its value changes, the engine applies it' % name,
              abs(after - before) >= (hi - lo) * 0.1 and near(applied, after, (hi - lo) * 0.01), (before, after, applied))
        check('the change is named as not saved; Save, Discard and Undo offered; the row marked',
              ('not saved: ' + name) in status() and page.is_enabled('#save') and page.is_enabled('#discard') and page.is_enabled('#undo')
              and 'changed' in (row.get_attribute('class') or ''), status())
        page.click('#undo'); time.sleep(1.2)
        check('Undo: back as saved, the engine too, nothing left to save',
              seq() == saved and near(engine()['applied'][name], before, (hi - lo) * 0.01) and not page.is_enabled('#save'), status())
        rng.focus()
        for k in range(20): page.keyboard.press(key)
        time.sleep(0.8); page.mouse.click(5, 5); page.keyboard.press('Control+z'); time.sleep(1.2)
        check('Ctrl+Z undoes it the same way', seq() == saved and near(engine()['applied'][name], before, (hi - lo) * 0.01), status())

        # ---- dots on a curve: double-click adds, right-click removes, drag moves; Discard
        curve = next((n for n, v in params.items() if isinstance(v, dict) and len(v.get('keys', [])) >= 2 and n in schema), None)
        keys = params[curve]['keys']
        tmid = start + (keys[0][0] + keys[1][0]) / 2
        span = page.evaluate('(n) => span(n)', curve)
        x, y = in_lane(curve, tmid, (span[0] + span[1]) / 2)
        page.mouse.dblclick(x, y); time.sleep(0.6)
        added = page.evaluate('(n) => keysOf(getValue(n)).keys.length', curve)
        check('double-click on %s\'s lane adds a dot there' % curve, added == len(keys) + 1, (len(keys), added))
        page.mouse.click(x, y, button='right'); time.sleep(0.6)
        check('right-click on it takes it away again', page.evaluate('(n) => keysOf(getValue(n)).keys.length', curve) == len(keys))
        x, y = in_lane(curve, start + keys[0][0], keys[0][1])
        x2, _ = in_lane(curve, start + keys[0][0] + 1.0, keys[0][1])
        page.mouse.move(x, y); page.mouse.down()
        for k in range(1, 9): page.mouse.move(x + (x2 - x) * k / 8, y); time.sleep(0.03)
        page.mouse.up(); time.sleep(0.6)
        moved = page.evaluate('(n) => keysOf(getValue(n)).keys', curve)
        check('a dot dragged 1 s along: it moves, the other dots stay', near(moved[0][0], keys[0][0] + 1.0, 0.15) and moved[1:] == keys[1:],
              (keys[0], moved[0]))
        page.click('#discard'); time.sleep(1.5)
        check('Discard changes: back to the saved timeline, nothing to save', seq() == saved and not page.is_enabled('#save')
              and 'back to the saved timeline' in status(), status())

        # ---- an effect cleared, and back
        page.click('[data-tab="effects"]')
        row.locator('.name').click(); time.sleep(0.3)
        page.click('#removeEffect'); time.sleep(1.2)
        std = schema[name].get('standard', 0)
        check('Clear on %s: it leaves the timeline, the engine puts it at its standard' % name,
              name not in (seq().get('params') or {}) and near(engine()['applied'][name], std, max(1e-3, abs(std) * 1e-3)),
              (engine()['applied'][name], std))
        other = next(n for n in params if n != name)
        page.locator('.effect .name', has_text=re.compile('^%s$' % re.escape(other))).first.click(); time.sleep(0.4)
        check('...its lane stays while it is selected, and goes once another is',
              page.locator('canvas.lane[data-name="%s"]' % name).count() == 0)
        page.click('#undo'); time.sleep(1.2)
        check('...Undo brings it back', seq() == saved and page.locator('canvas.lane[data-name="%s"]' % name).count() == 1)

        # ---- sounds: listed, moved, heard
        page.click('[data-tab="sounds"]'); time.sleep(0.3)
        check('the Sounds tab lists the timeline\'s %d sounds' % len(sounds), page.locator('#sounds tr.sound').count() == len(sounds),
              page.locator('#sounds tr.sound').count())
        i = max((j for j in range(len(sounds)) if not sounds[j].get('loop')), key=lambda j: sounds[j].get('at', 0))
        snd = sounds[i]; at = snd.get('at', 0)
        x, y = in_lane('sound:%d' % i, start + at + 0.3)
        x2, _ = in_lane('sound:%d' % i, start + at + 2.3)
        page.mouse.move(x, y); page.mouse.down()
        for k in range(1, 9): page.mouse.move(x + (x2 - x) * k / 8, y); time.sleep(0.03)
        page.mouse.up(); time.sleep(0.6)
        check('the sound %s dragged 2 s later' % snd['sound'], near(seq()['sounds'][i].get('at', 0), at + 2.0, 0.15), seq()['sounds'][i].get('at'))
        page.click('#discard'); time.sleep(1.5)
        check('...and Discard puts it back', seq() == saved)
        x, y = ruler_x(start + at - 1.0); page.mouse.click(x, y); time.sleep(1.0)
        page.click('#play'); heard = set(); end = time.time() + 2.5
        while time.time() < end:
            heard |= set(filter(None, get('/api/engine').get('sounds', '').split(','))); time.sleep(0.1)
        page.click('#play'); time.sleep(0.5)
        check('played through %s\'s moment: the engine plays it' % snd['sound'], snd['sound'] in heard, sorted(heard))

        # ---- zoom, the panel, the picture, the camera
        page.click('#zoomIn'); time.sleep(0.3)
        v = page.evaluate('view')
        check('+ zooms in on the time line (All and − offered)', v is not None and page.is_enabled('#fit') and page.is_enabled('#zoomOut'), v)
        page.click('#fit'); time.sleep(0.3)
        check('All shows the whole time line again', page.evaluate('view') is None)
        b = page.locator('#lanes').bounding_box()
        page.mouse.move(b['x'] + b['width'] / 2, b['y'] + 20)
        page.keyboard.down('Control'); page.mouse.wheel(0, -300); page.keyboard.up('Control'); time.sleep(0.3)
        check('Ctrl + mouse wheel over the lanes zooms in', page.evaluate('view') is not None)
        page.click('#fit')
        page.click('#panelToggle'); time.sleep(0.3)
        hidden = page.evaluate('document.querySelector("main").classList.contains("wide")')
        page.click('#panelToggle'); time.sleep(0.3)
        check('Hide panel / Show panel', hidden and not page.evaluate('document.querySelector("main").classList.contains("wide")'))
        page.dblclick('#view'); time.sleep(0.3)
        big = page.evaluate('document.querySelector("main").classList.contains("bigpic")')
        page.dblclick('#view'); time.sleep(0.3)
        check('double-click on the picture: as big as it can be, and back', big and not page.evaluate('document.querySelector("main").classList.contains("bigpic")'))
        f0 = get('/api/engine')['frame']; time.sleep(1.0)
        check('the picture is live (new frames from the engine)', get('/api/engine')['frame'] > f0 and (page.get_attribute('#view', 'src') or '') != '')
        yaw0 = get('/api/engine')['camera'][3]
        page.click('summary:has-text("Camera")'); time.sleep(0.3)
        page.focus('#yaw')
        for k in range(15): page.keyboard.press('ArrowRight')
        time.sleep(1.0)
        turned = (get('/api/engine')['camera'][3] - yaw0 + 540) % 360 - 180
        check('the "turn" slider turns the engine\'s camera (15 steps: about 15 degrees)', 10 <= abs(turned) <= 20, round(turned, 1))

        # ---- the engine killed with a change not saved; the page reloaded
        page.click('[data-tab="effects"]')
        rng.focus()
        for k in range(40): page.keyboard.press(key)
        time.sleep(1.2)
        changed_value = value_now(name); page_t = now(); changed_curve = page.evaluate('(n) => getValue(n)', name)
        kids = subprocess.run(['pgrep', '-P', str(server.pid), '-x', 'ge'], capture_output=True, text=True).stdout.split()
        if kids: os.kill(int(kids[0]), signal.SIGKILL)
        time.sleep(3)
        for k in range(40):
            try:
                en = get('/api/engine')
                if en.get('alive') and near(en.get('t'), page_t, 0.2) and near(en['applied'].get(name), changed_value, (hi - lo) * 0.01): break
            except Exception: pass
            time.sleep(1)
        en = get('/api/engine')
        check('the engine killed: it comes back at the page\'s moment, with the change not saved',
              bool(kids) and en.get('alive') and near(en.get('t'), page_t, 0.2) and near(en['applied'].get(name), changed_value, (hi - lo) * 0.01),
              (en.get('t'), page_t, en.get('applied', {}).get(name), changed_value))
        page.reload(); page.wait_for_selector('canvas.lane'); time.sleep(2.5)
        check('the page reloaded: the change not saved is still there, and named',
              ('not saved: ' + name) in status() and page.evaluate('(n) => getValue(n)', name) == changed_curve, status())
        page.click('#discard'); time.sleep(1.5)
        check('...Discard: back as saved', seq() == saved and not page.is_enabled('#save'))
        check('nothing on disk changed so far', guard.unchanged(), guard.changed())

        # ---- Save as: a new file the map plays
        page.click('#saveas'); time.sleep(1.5)
        new = 'Game/sequences/%s.json' % SAVE_AS
        written = json.load(open(new)) if os.path.exists(new) else None
        written = written if not isinstance(written, list) else written[0]
        plays = json.load(open(MAP)).get('Sequences')
        check('Save as: the new file holds the timeline, the map plays it now',
              written is not None and written.get('params') == saved.get('params') and plays == [{"file": new}]
              and ('saved as ' + SAVE_AS) in status(), (plays, status()))
        check('Save as wrote only those (the file it came from as it was)', sorted(guard.changed()) == sorted([MAP, new]), guard.changed())
        check('no errors in the page', not errors, errors[:3])
        browser.close()
finally:
    server.send_signal(signal.SIGTERM)
    try: server.wait(15)
    except subprocess.TimeoutExpired: server.kill()
bad = [r for r in results if not r[1]]
print('\n%d of %d checks passed' % (len(results) - len(bad), len(results)))
for r in bad: print('FAILED:', r[0])
