"""mapsounds.py <port>: each map plays the sounds its "Audio" list names (and nothing named by the code);
an old-shape block is refused with a message; "loop": false plays once; an unknown "follows" is reported."""
import guard
import sys, json, time, urllib.request, subprocess, signal, os, copy
PORT=sys.argv[1]; B='http://localhost:%s' % PORT; S=os.environ.get('GE_TEST_OUT', '/tmp/ge-tests')+'/'; os.makedirs(S, exist_ok=True)
LOG='/tmp/timeline_editor_engine.log'
results=[]
def check(name, ok, detail=''):
    results.append((name, bool(ok))); print(('PASS  ' if ok else 'FAIL  ')+name+('  — '+str(detail) if detail != '' else ''), flush=True)
def get(path): return json.load(urllib.request.urlopen(B+path, timeout=5))
def post(path, body):
    r=urllib.request.Request(B+path, json.dumps(body).encode(), {'Content-Type':'application/json'}); return json.load(urllib.request.urlopen(r, timeout=10))
def heard(map_path, secs=3.0):
    server=subprocess.Popen(['python3','tools/timeline_editor.py',map_path,'--size','320x180','--port',PORT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for i in range(90):
            try:
                if get('/api/engine').get('alive') and 'running' in get('/api/engine'): break
            except Exception: pass
            time.sleep(1)
        post('/api/mute', {'muted': False}); post('/api/pause', {'paused': False})
        seen={}; end=time.time()+secs
        while time.time() < end:
            for n in filter(None, get('/api/engine').get('sounds', '').split(',')): seen[n]=seen.get(n, 0)+1
            time.sleep(0.1)
        return seen, open(LOG).read()
    finally:
        server.send_signal(signal.SIGTERM)
        try: server.wait(15)
        except subprocess.TimeoutExpired: server.kill()
        time.sleep(1)
for m in ['Game/recursion/00_archive_ignition.json', 'Game/recursion/02_corridor_subject4.json', 'Game/lot.json']:
    want=[a['sound'] for a in json.load(open(m))['Audio']]
    seen, log = heard(m)
    print('   %s: names %s, heard %s' % (m, want, seen))
    night=[a['sound'] for a in json.load(open(m))['Audio'] if a.get('follows') == 'night']
    check('%s: every sound its Audio list names is playing' % m.split('/')[-1], all(n in seen for n in want), sorted(set(want) - set(seen)))
    check('%s: no SoundScape complaint in the log' % m.split('/')[-1], 'SoundScape:' not in log)
# made-up maps in the test output folder: the same map with other sounds
base=json.load(open('Game/recursion/02_records_room.json'))
def variant(name, audio):
    d=copy.deepcopy(base); d['Audio']=audio; p=S+name; json.dump(d, open(p, 'w'), indent=1); return p
seen, log = heard(variant('v_frogs.json', [{"sound": "fire_loop", "volume": 0.5, "follows": "night"}, {"sound": "crickets_loop", "pitch": 0.6}]))
check('a map can use any sound, at another pitch, following the night: fire_loop and crickets_loop here', 'fire_loop' in seen and 'crickets_loop' in seen and 'hum_loop' not in seen, seen)
seen, log = heard(variant('v_none.json', []))
check('a map with an empty list: none of its sounds (no hard-coded wind or crickets)', not ({'wind_loop', 'crickets_loop', 'fire_loop', 'hum_loop'} & set(seen)), seen)
seen, log = heard(variant('v_old.json', {"wind": 0.15, "crickets": 0.25, "sources": []}))
check('the old shape: refused with a message, nothing played', 'is a list of sounds' in log and not ({'wind_loop', 'crickets_loop'} & set(seen)), seen)
# a one-shot: the shortest sound of a few seconds (it must end inside the time listened, and last
# long enough to be seen playing first)
import glob, wave
def length(path):
    w=wave.open(path); return w.getnframes() / w.getframerate()
short=min((p for p in glob.glob('res/sounds/*.wav') if not p.endswith('_loop.wav') and length(p) >= 3.0), key=length)
once, once_secs = os.path.basename(short)[:-4], length(short)
seen, log = heard(variant('v_once.json', [{"sound": once, "loop": False}, {"sound": "hum_loop", "follows": "rain"}]), secs=once_secs + 6)
print('   the one-shot: %s (%.1f s), listened %.1f s: %s' % (once, once_secs, once_secs + 6, seen))
check('"loop": false plays once (gone after it ends); an unknown "follows" is reported and it still plays',
      0 < seen.get(once, 0) < seen.get('hum_loop', 0) - 20 and 'follows "rain"' in log and seen.get('hum_loop', 0) > 40, (seen, [l for l in log.splitlines() if 'SoundScape' in l]))
check('no file on disk changed', guard.unchanged())
bad=[r for r in results if not r[1]]
print('\n%d of %d checks passed' % (len(results)-len(bad), len(results)))
for r in bad: print('FAILED:', r[0])
