"""solo.py <port>: the solo button on each lane, used like a person: only the soloed lanes play in the
engine (checked in what it applied, what it renders and what it plays), the timeline itself never
changes, and it survives an engine restart, clears on a page reload, follows sounds being removed."""
import guard
import sys, json, re, time, urllib.request, subprocess, signal, copy, os
from playwright.sync_api import sync_playwright
PORT=sys.argv[1]; B='http://localhost:%s' % PORT; OUT=os.environ.get('GE_TEST_OUT', '/tmp/ge-tests')+'/'; os.makedirs(OUT, exist_ok=True)
server=subprocess.Popen(['python3','tools/timeline_editor.py','Game/recursion/00_archive_ignition.json','--size','480x270','--port',PORT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def get(path): return json.load(urllib.request.urlopen(B+path, timeout=5))
results=[]
def check(name, ok, detail=''):
    results.append((name, bool(ok))); print(('PASS  ' if ok else 'FAIL  ')+name+('  — '+str(detail) if detail != '' else ''), flush=True)
MODIFIERS={'gain', 'gustiness', 'gustVolume', 'boomVolume', 'thudVolume', 'thunderVolume', 'frontDistance'}
try:
    for i in range(60):
        try:
            if get('/api/engine').get('alive') and 'running' in get('/api/engine'): break
        except Exception: pass
        time.sleep(1)
    st=get('/api/state'); schema={p['name']: p for p in st['schema']['parameters']}
    with sync_playwright() as p:
        browser=p.chromium.launch(channel='chrome', headless=True)
        page=browser.new_page(viewport={'width': 1860, 'height': 950})
        errors=[]; page.on('pageerror', lambda e: errors.append(str(e)[:160])); page.on('dialog', lambda d: d.accept())
        page.goto(B+'/'); page.wait_for_selector('canvas.lane'); time.sleep(2)
        seq=lambda: copy.deepcopy(page.evaluate('seq()'))
        orig=seq()
        lane=lambda name: page.locator('canvas.lane[data-name="%s"]' % name)
        def solo_button(name):
            lane(name).scroll_into_view_if_needed(); time.sleep(0.15)
            b=lane(name).bounding_box(); w=page.evaluate('laneWidth()')
            return b['x'] + (w - page.evaluate('SOLO_W') / 2 - 4) * b['width'] / w, b['y'] + 7 * b['height'] / page.evaluate('LANE_H')
        def click_solo(name):
            x, y = solo_button(name); page.mouse.click(x, y); time.sleep(1.2)
        def go(t):
            b=page.locator('#ruler').bounding_box(); w=page.evaluate('laneWidth()')
            page.mouse.click(b['x'] + page.evaluate('px(%f, %d)' % (t, w)) * b['width'] / w, b['y'] + 11); time.sleep(1.2)
        def engine(): time.sleep(0.4); return get('/api/engine')
        T=33.0
        go(T)
        base=engine(); A0=base['applied']
        print('   everything at %.1f s: %s' % (base['t'], {k: v for k, v in A0.items() if v}))
        on0=[k for k, v in A0.items() if k not in MODIFIERS and abs(v - schema.get(k, {}).get('standard', 0)) > 1e-3]
        check('at %g s many effects are on (the test means something)' % T, len(on0) >= 5, on0)
        def isolated(names, en):
            """every effect not in names at its standard; those in names as in the whole timeline"""
            A=en['applied']; bad=[]
            for k, v in A.items():
                if k in MODIFIERS: want=A0[k]
                elif k in names: want=A0[k]
                elif k == 'fires': want=0
                else: want=schema[k]['standard'] if k in schema else 0
                if abs(v - want) > 1e-3 * max(1, abs(want)): bad.append((k, v, want))
            return bad
        sel0, now0 = page.evaluate('selected'), page.evaluate('nowT')
        click_solo('dust')
        en=engine()
        check('solo on dust: the page shows it isolated', page.evaluate('isSolo("dust")') and page.is_visible('#unsolo') and 'only dust plays' in page.inner_text('#when'),
              page.inner_text('#when'))
        check('solo on dust: the engine applies dust only (the rest off, the modifiers as they were)', not isolated({'dust'}, en), isolated({'dust'}, en))
        check('solo on dust: the debris in the sky off too, the Moon whole', en.get('shell', -1) == 0 and en.get('ejecta', -1) == 0, (en.get('shell'), en.get('ejecta')))
        check('the solo click changed nothing else (time, selection)', page.evaluate('nowT') == now0 and page.evaluate('selected') == sel0)
        check('the timeline is not changed by it (nothing unsaved)', seq() == orig and not page.evaluate('changes()').__len__() and page.inner_text('#status') == '', page.inner_text('#status'))
        page.screenshot(path=OUT+'solo1_dust.png')
        click_solo('lightning')
        check('solo on lightning too: dust and lightning play', not isolated({'dust', 'lightning'}, engine()), isolated({'dust', 'lightning'}, engine()))
        click_solo('dust')
        check('solo on dust again: only lightning plays', not isolated({'lightning'}, engine()) and not page.evaluate('isSolo("dust")'), isolated({'lightning'}, engine()))
        page.click('#unsolo'); time.sleep(1.2)
        en=engine()
        check('"Play everything": all of it back as it was', not page.evaluate('solo.size') and page.is_hidden('#unsolo') and
              all(abs(en['applied'][k] - A0[k]) < 1e-3 * max(1, abs(A0[k])) for k in A0 if k != 'fires'),
              {k: (en['applied'][k], A0[k]) for k in A0 if abs(en['applied'][k] - A0[k]) > 1e-3})
        # double-click on the button: no dot added
        x, y = solo_button('dust'); page.mouse.dblclick(x, y); time.sleep(1)
        check('a double-click on solo adds no dot (and leaves it as it was)', seq() == orig and not page.evaluate('solo.size'))
        # sounds: with and without solo, what plays around a sound
        names=[s['sound'] for s in orig['sounds']]
        i=max(range(len(names)), key=lambda j: orig['sounds'][j].get('at', 0) if not orig['sounds'][j].get('loop') else -1)
        snd=orig['sounds'][i]; at=page.evaluate('seqStart()') + snd['at']
        # (the map's own ambience can play a sound the timeline also uses, wind_loop: not the timeline's then)
        own={a['sound'] for a in json.load(open('Game/recursion/00_archive_ignition.json')).get('Audio', [])}
        others=set(names) - {snd['sound']} - own
        def heard(t0, secs):
            go(t0); page.click('#play'); seen=set(); end=time.time() + secs
            while time.time() < end:
                seen |= set(filter(None, get('/api/engine').get('sounds', '').split(','))); time.sleep(0.1)
            page.click('#play'); time.sleep(0.8)
            return seen
        all_heard=heard(at - 1.0, 2.5)
        print('   playing everything from %.1f s: %s' % (at - 1.0, sorted(all_heard)))
        check('without solo, other timeline sounds are heard there too (the test means something)', bool(all_heard & others), sorted(all_heard))
        click_solo('dust')
        scene=heard(at - 1.0, 2.5)
        print('   the map own sounds there (only an effect isolated): %s' % sorted(scene))
        check('only an effect isolated: no timeline sound heard (the map own still are)', not (scene - own), sorted(scene))
        click_solo('dust'); click_solo('sound:%d' % i)
        one=heard(at - 1.0, 2.5) - scene
        print('   %s alone: %s' % (snd['sound'], sorted(one)))
        check('solo on the sound %s: it is heard, no other sound of the timeline, no storm noises' % snd['sound'],
              snd['sound'] in one and not (one & others) and not any(n.startswith(('gust', 'thunder', 'thud')) for n in one), sorted(one))
        # a sound before it removed: the solo stays on the same sound
        page.click('[data-tab="sounds"]'); time.sleep(0.3)
        page.locator('#sounds tr.sound').nth(0).locator('button.danger').click(); time.sleep(1)
        check('a sound before it removed: still that sound isolated', page.evaluate('soloNames()') == ['♪ ' + snd['sound']] and page.evaluate('isSolo("sound:%d")' % (i - 1)),
              page.evaluate('soloNames()'))
        page.click('#discard'); time.sleep(1.5)
        check('Discard changes: back to the timeline, nothing isolated', seq() == orig and not page.evaluate('solo.size'), page.evaluate('[...solo]'))
        # an effect cleared from the timeline: no longer isolated
        page.click('[data-tab="effects"]'); go(T); click_solo('fog')
        page.locator('.effect .name', has_text=re.compile('^fog$')).first.click(); time.sleep(0.3)
        page.click('#removeEffect'); time.sleep(1)
        check('an isolated effect cleared: still isolated while its lane is shown (selected), at its standard',
              page.evaluate('isSolo("fog")') and not isolated(set(), engine()), isolated(set(), engine()))
        page.locator('.effect .name', has_text=re.compile('^rain$')).first.click(); time.sleep(1)
        check('...its lane gone (another one selected): not isolated any more', not page.evaluate('solo.size') and page.is_hidden('#unsolo'))
        page.click('#undo'); time.sleep(1)
        check('...Undo puts it back', seq() == orig)
        # the engine restarted: the solo comes back with it
        go(T); click_solo('rain')
        kids=subprocess.run(['pgrep', '-P', str(server.pid), '-x', 'ge'], capture_output=True, text=True).stdout.split()
        if kids: os.kill(int(kids[0]), signal.SIGKILL)
        for k in range(40):
            time.sleep(1)
            try:
                e=get('/api/engine')
                if e.get('alive') and k > 3 and abs(e.get('t', -1) - T) < 0.2: break
            except Exception: pass
        time.sleep(2)
        check('the engine restarted (killed): still only rain plays', bool(kids) and not isolated({'rain'}, engine()), isolated({'rain'}, engine()))
        # the page reloaded: nothing isolated
        page.reload(); page.wait_for_selector('canvas.lane'); time.sleep(3); go(T)
        en=engine()
        check('the page reloaded: nothing isolated, all of it plays', not page.evaluate('solo.size') and
              all(abs(en['applied'][k] - A0[k]) < 1e-3 * max(1, abs(A0[k])) for k in A0 if k != 'fires'))
        check('no errors in the page', not errors, errors)
        browser.close()
finally:
    server.send_signal(signal.SIGTERM)
    try: server.wait(15)
    except subprocess.TimeoutExpired: server.kill()
check('no file on disk changed', guard.unchanged(), guard.changed())
bad=[r for r in results if not r[1]]
print('\n%d of %d checks passed' % (len(results)-len(bad), len(results)))
for r in bad: print('FAILED:', r[0])
