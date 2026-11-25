"""noises.py <port>: the storm's gusts, booms, thuds and thunder play the sounds the timeline's
"noises" name, in turn, with each one's volume; with none named they are silent."""
import guard
import sys, json, time, urllib.request, subprocess, signal, copy
PORT=sys.argv[1]; B='http://localhost:%s' % PORT
server=subprocess.Popen(['python3','tools/timeline_editor.py','Game/recursion/00_archive_ignition.json','--size','320x180','--port',PORT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def get(path): return json.load(urllib.request.urlopen(B+path, timeout=5))
def post(path, body):
    r=urllib.request.Request(B+path, json.dumps(body).encode(), {'Content-Type':'application/json'}); return json.load(urllib.request.urlopen(r, timeout=10))
results=[]
def check(name, ok, detail=''):
    results.append((name, bool(ok))); print(('PASS  ' if ok else 'FAIL  ')+name+('  — '+str(detail) if detail != '' else ''), flush=True)
SCENE={'wind_loop', 'crickets_loop', 'fire_loop'}
try:
    for i in range(90):
        try:
            if get('/api/engine').get('alive') and 'running' in get('/api/engine'): break
        except Exception: pass
        time.sleep(1)
    time.sleep(1.5)
    entries=get('/api/state')['entries']
    collapse=entries[0]['sequences'][0]['params'].get('collapse')
    def run(params, noises, T=40, secs=4.0):
        e=copy.deepcopy(entries); s=e[0]['sequences'][0]
        s['params']=dict(({'collapse': collapse} if collapse is not None else {}), **params); s['sounds']=[]
        if noises is None: s.pop('noises', None)
        else: s['noises']=noises
        post('/api/apply', {'entries': e}); post('/api/pause', {'paused': True}); post('/api/seek', {'t': T}); time.sleep(0.8)
        p0=get('/api/engine')['played']
        post('/api/mute', {'muted': False}); post('/api/pause', {'paused': False})
        seen={}; end=time.time() + secs
        while time.time() < end:
            for n in filter(None, get('/api/engine').get('sounds', '').split(',')): seen[n]=seen.get(n, 0) + 1
            time.sleep(0.08)
        post('/api/pause', {'paused': True}); post('/api/mute', {'muted': True}); time.sleep(0.5)
        en=get('/api/engine')
        return {n for n in seen if n not in SCENE}, en['played'], p0
    heard, played, _ = run({'booms': 3}, {'booms': ['gust_2']})
    check('booms play what "booms" names (gust_2 here), nothing else', heard == {'gust_2'} and played['booms'][0] > 0, (heard, played['booms']))
    heard, played, _ = run({'booms': 3}, {'booms': ['gust_2', 'land']})
    check('two named: they take turns', heard == {'gust_2', 'land'}, heard)
    heard, played, _ = run({'booms': 3}, None)
    check('none named: booms are silent (the engine has none of its own)', not heard and played['booms'][1] == 0, (heard, played['booms']))
    heard, played, _ = run({'booms': 3}, {'booms': [{'sound': 'land', 'volume': 0.5}]})
    v_half=played['booms'][1]
    heard2, played2, _ = run({'booms': 3}, {'booms': ['land']})
    check('a sound\'s own volume in the set: half as loud', heard == {'land'} and abs(v_half / max(played2['booms'][1], 1e-6) - 0.5) < 0.02, (v_half, played2['booms'][1]))
    heard, played, _ = run({'gusts': 4}, {'gusts': ['boom_3']})
    check('gusts play what "gusts" names', heard == {'boom_3'} and played['gusts'][0] > 0, (heard, played['gusts']))
    heard, played, _ = run({'thuds': 3}, {'thuds': ['gust_1']})
    check('thuds play what "thuds" names', heard == {'gust_1'}, heard)
    heard, played, _ = run({'lightning': 1}, {'thunder': ['gust_3']})
    check('thunder after lightning plays what "thunder" names', heard == {'gust_3'} and played['thunder'][0] > 0, (heard, played['thunder']))
    heard, played, _ = run({'lightning': 1}, {'booms': ['land']})
    check('lightning with no "thunder" named: silent strikes', not heard and played['thunder'][0] == 0, (heard, played['thunder']))
    heard, played, _ = run({'booms': 3, 'gusts': 4}, {'booms': ['land'], 'gusts': ['gust_1']})
    check('each noise its own set (booms: land, gusts: gust_1)', heard == {'land', 'gust_1'}, heard)
    post('/api/revert', {})
    log=open('/tmp/timeline_editor_engine.log').read()
    check('the engine never stopped, nothing odd in its log', get('/api/engine').get('alive') and 'terminate' not in log and 'Abort' not in log)
finally:
    server.send_signal(signal.SIGTERM)
    try: server.wait(15)
    except subprocess.TimeoutExpired: server.kill()
check('no file on disk changed', guard.unchanged())
bad=[r for r in results if not r[1]]
print('\n%d of %d checks passed' % (len(results)-len(bad), len(results)))
for r in bad: print('FAILED:', r[0])
