"""sounds.py <port>: after a jump in time nothing from the old moment is still sounding. Own editor + engine."""
import guard
import sys, json, time, urllib.request, subprocess, signal
PORT=sys.argv[1]; B='http://localhost:%s' % PORT
server=subprocess.Popen(['python3','tools/timeline_editor.py','Game/recursion/00_archive_ignition.json','--size','960x540','--port',PORT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
def get(path): return json.load(urllib.request.urlopen(B+path, timeout=5))
def post(path, body):
    r=urllib.request.Request(B+path, json.dumps(body).encode(), {'Content-Type':'application/json'}); return json.load(urllib.request.urlopen(r, timeout=10))
def sounds(): return [s for s in get('/api/engine').get('sounds', '').split(',') if s]
STORM=('blast', 'boom', 'gust', 'thud', 'thunder', 'storm_loop', 'spray_loop')
def of_storm(names): return [n for n in names if n.startswith(STORM)]
def play(): post('/api/mute', {'muted': False}); post('/api/pause', {'paused': False})
def pause(): post('/api/pause', {'paused': True}); post('/api/mute', {'muted': True})
results=[]
def check(name, ok, detail=''):
    results.append((name, bool(ok))); print(('PASS  ' if ok else 'FAIL  ')+name+('  — '+str(detail) if detail != '' else ''), flush=True)
try:
    for i in range(60):
        try:
            if get('/api/engine').get('alive') and 'running' in get('/api/engine'): break
        except Exception: pass
        time.sleep(1)
    time.sleep(1.5); pause()
    # when things sound, from the timeline the map plays (not written in here: it is edited)
    st=get('/api/state'); seq=st['entries'][0]['sequences'][0]; start=st['engine']['starts'][0]
    BLAST=start+next(x.get('at', 0) for x in seq['sounds'] if x['sound']=='blast')
    AHEAD=max(BLAST+6, min(start+x.get('at', 0) for x in seq['sounds'] if x['sound']=='storm_loop')+2)
    QUIET=min(3, st['engine']['firstSound']-2)
    print('   the blast at %.1f s, the storm sounding by %.1f s, nothing before %.1f s' % (BLAST, AHEAD, st['engine']['firstSound']))
    post('/api/seek', {'t': BLAST-0.2}); time.sleep(0.5); play(); time.sleep(1.5)
    now=sounds(); check('playing through the impact: the blast is sounding', 'blast' in now, now)
    pause(); time.sleep(0.4)
    held=sounds(); check('paused: its sounds are held, not lost', 'blast' in held and get('/api/engine')['volume'] == 0, held)
    play(); time.sleep(0.3)
    check('Play carries on with them', 'blast' in sounds(), sounds())
    # the reported bug: back to the start, the explosion must not still be sounding
    pause(); post('/api/seek', {'t': -12}); time.sleep(1.0)
    check('jumped back to the start (paused): nothing of the storm left', not of_storm(sounds()), sounds())
    play(); time.sleep(1.2)
    check('playing from the start: no explosion, no storm sounds', not of_storm(sounds()) and get('/api/engine')['volume'] > 0, sounds())
    pause()
    # back while playing
    post('/api/seek', {'t': BLAST-0.2}); time.sleep(0.4); play(); time.sleep(1.5)
    check('(the blast again)', 'blast' in sounds(), sounds())
    post('/api/seek', {'t': -10}); time.sleep(0.5)
    check('jumped back to the start while playing: the blast is cut', not of_storm(sounds()), sounds())
    # ahead while playing: the one-shots are cut, the storm's loops carry on
    post('/api/seek', {'t': BLAST-0.2}); time.sleep(1.5)
    check('(the blast again)', 'blast' in sounds(), sounds())
    post('/api/seek', {'t': AHEAD}); time.sleep(0.6)
    now=sounds(); check('jumped ahead while playing: the blast is cut, the storm goes on', 'blast' not in now and 'storm_loop' in now, now)
    pause(); post('/api/seek', {'t': QUIET}); time.sleep(0.8)
    check('jumped to before the storm\'s sounds (paused): none left', not of_storm(sounds()), sounds())
finally:
    server.send_signal(signal.SIGTERM)
    try: server.wait(15)
    except subprocess.TimeoutExpired: server.kill()
bad=[r for r in results if not r[1]]
print('\n%d of %d checks passed' % (len(results)-len(bad), len(results)))
