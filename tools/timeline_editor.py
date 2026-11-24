#!/usr/bin/env python3
"""The timeline editor: edit a map's scripted sequences (Core/Timeline) in the
browser, watching the engine render them live.

  python3 tools/timeline_editor.py [map.json] [--port 8765] [--size 1280x720]

then open http://localhost:8765 . It starts the engine on the map (it opens
its own small window: it renders there), driven from here (GE_REMOTE): the
engine sends what it renders, which the page shows as a live video; every
change in the page goes straight to the engine, which swaps in the new
timeline without restarting. Scrub to any moment, pause, play, turn the
camera. "Save" writes each sequence back where the map keeps it: the file of
a {"file": ...} entry (Game/sequences/...), or the map itself for inline ones
(a map with a Moon destruction and none of its own plays the default
Game/sequences/kugelblitz_storm.json). "Add file" puts an uploaded sequence
JSON into Game/sequences/ and into the map's "Sequences": it plays alongside
the others. "Remove" takes one out of the map (its file stays).

Localhost only. Needs Pillow (the frames become JPEG for the browser).
"""
import argparse
import array
import copy
import glob
import http.server
import io
import json
import os
import re
import signal
import socket
import socketserver
import subprocess
import sys
import threading
import time
import wave

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, 'timeline_editor', 'index.html')
SESSION = '%x' % int(time.time() * 1000)     # this run of the editor: a page left open notices a restart
DEFAULT_STORM = 'Game/sequences/kugelblitz_storm.json'


def known_sounds():
    return sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob('res/sounds/*.wav'))


SHAPE_BINS = 200
_shapes = {}        # path -> (mtime, shape)


def sound_shape(name):
    """how a sound looks on the timeline: its length in seconds and how loud it is along it
    (SHAPE_BINS peaks, 0..1); None if the file cannot be read"""
    path = os.path.join('res', 'sounds', name + '.wav')
    mtime = 0
    try:
        mtime = os.path.getmtime(path)
        if path in _shapes and _shapes[path][0] == mtime:
            return _shapes[path][1]
        with wave.open(path, 'rb') as w:
            width, channels, rate, frames = w.getsampwidth(), w.getnchannels(), w.getframerate(), w.getnframes()
            data = w.readframes(frames)
        if width == 2:
            samples, full = array.array('h', data), 32768.0
        elif width == 1:
            samples, full = array.array('b', bytes((b - 128) & 0xff for b in data)), 128.0
        elif width == 4:
            samples, full = array.array('i', data), 2147483648.0
        else:
            return {"length": frames / float(rate), "peaks": []}
        if sys.byteorder != 'little' and width > 1:
            samples.byteswap()
        step = max(1, len(samples) // SHAPE_BINS)
        peaks = []
        for i in range(0, len(samples), step):
            chunk = samples[i:i + step]
            peaks.append(round(max(max(chunk), -min(chunk)) / full, 3))
        shape = {"length": round(frames / float(rate), 3), "peaks": peaks[:SHAPE_BINS]}
    except (OSError, EOFError, wave.Error, ValueError):
        shape = None
    _shapes[path] = (mtime, shape)
    return shape


def sound_shapes():
    return {name: sound_shape(name) for name in known_sounds()}


def about_file(path):
    """what to know about a timeline file before playing or changing it ('' if nothing)"""
    if path and os.path.normpath(path) == os.path.normpath(DEFAULT_STORM):
        return ("The engine's standard storm: every map with a Moon destruction and no timeline of its own "
                "plays it (Game/lagoon.json does). Changing it changes those maps.")
    return ''


def write_json(path, data):
    """a file written whole or not at all (a crash halfway leaves the old one)"""
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    temporary = path + '.writing'
    with open(temporary, 'w') as out:
        json.dump(data, out, indent=1)
    os.replace(temporary, path)


def canonical(value):
    """a timeline as text, the same for the same content: keys in order, 8 and 8.0 alike"""
    def tidy(v):
        if isinstance(v, bool):
            return v
        if isinstance(v, (int, float)):
            return round(float(v), 6)
        if isinstance(v, list):
            return [tidy(x) for x in v]
        if isinstance(v, dict):
            return {k: tidy(x) for k, x in v.items()}
        return v
    return json.dumps(tidy(value), sort_keys=True)


class Sequences:
    """the map's sequences: each entry of its "Sequences" a file
    ({"file": "Game/sequences/..."}) or kept inline in the map; loading,
    saving each where it lives, adding files to the map, removing them"""

    def __init__(self, map_path):
        self.map_path = map_path
        self.reload()

    def map_json(self):
        return json.load(open(self.map_path))

    @property
    def dirty(self):
        """changed (and applied to the engine) but not saved: compared with what is on disk,
        so undoing everything, by whatever route, is "no changes" again"""
        return canonical(self.entries) != canonical(self.saved)

    def reload(self):
        self.entries = self.from_disk()
        self.saved = copy.deepcopy(self.entries)

    def from_disk(self):
        """the map's entries as the map and their files have them now"""
        game = self.map_json()
        entries = game.get('Sequences')
        self.implicit = False
        if entries is None:
            entries = []
            holes = game.get('BlackHoles') or []
            if any(h.get('sky') and h.get('kugelblitz') for h in holes):
                entries = [{"file": DEFAULT_STORM}]      # a Moon destruction gets the default storm
                self.implicit = True
        found = []
        for entry in (entries if isinstance(entries, list) else [entries]):
            if isinstance(entry, dict) and 'file' in entry:
                path = entry['file']
                data = json.load(open(path)) if os.path.exists(path) else []
                found.append({"file": path, "sequences": data if isinstance(data, list) else [data]})
            else:
                found.append({"file": None, "sequences": [entry]})
        return found

    def all(self):
        """every sequence, for the engine"""
        return [s for entry in self.entries for s in entry['sequences']]

    def write_map_list(self, inline_from=None):
        """the map's "Sequences" list, as the entries are now. A timeline kept inside the map is
        written as it was last saved (inline_from: as these entries have it, when saving)"""
        game = self.map_json()
        source = inline_from if inline_from is not None else self.saved
        game['Sequences'] = [{"file": e['file']} if e['file'] else source[i]['sequences'][0]
                             for i, e in enumerate(self.entries) if e['file'] or source[i]['sequences']]
        write_json(self.map_path, game)
        self.implicit = False

    def save(self, entries):
        """what changed since the last save: file entries into their files, inline ones into the map"""
        for entry, edited in zip(self.entries, entries):
            entry['sequences'] = edited['sequences']
        changed = [i for i, e in enumerate(self.entries) if canonical(e) != canonical(self.saved[i])]
        for i in changed:
            entry = self.entries[i]
            if entry['file'] and entry['sequences']:
                write_json(entry['file'], entry['sequences'][0] if len(entry['sequences']) == 1 else entry['sequences'])
        if any(not self.entries[i]['file'] for i in changed):
            self.write_map_list(inline_from=self.entries)
        self.saved = copy.deepcopy(self.entries)
        return [self.entries[i]['file'] or self.map_path for i in changed]

    def add(self, filename, content):
        """an uploaded sequence file: into Game/sequences/ (a new name if a
        different file is there already), and into the map's list"""
        data = json.loads(content)
        sequences = data if isinstance(data, list) else [data]
        if not sequences or not all(isinstance(s, dict) and ('params' in s or 'sounds' in s or 'events' in s)
                                    for s in sequences):
            raise ValueError('not a sequence file (no "params", "sounds" or "events")')
        base = re.sub(r'[^A-Za-z0-9_.-]', '_', os.path.basename(filename)) or 'sequence.json'
        if not base.endswith('.json'):
            base += '.json'
        stem, path, n = base[:-5], os.path.join('Game', 'sequences', base), 2
        while os.path.exists(path) and json.load(open(path)) != data:
            path = os.path.join('Game', 'sequences', '%s_%d.json' % (stem, n))
            n += 1
        if any(e['file'] == path for e in self.entries):
            raise ValueError('%s is in the map already' % path)
        write_json(path, data)
        self.entries.append({"file": path, "sequences": sequences})
        self.saved.append({"file": path, "sequences": copy.deepcopy(sequences)})
        self.write_map_list()
        return path

    def save_as(self, index, name, entries):
        """the entry's timeline, as edited, into a new file (never over an existing one);
        the map plays that file from now on, the one it was in stays as it was saved"""
        base = re.sub(r'[^A-Za-z0-9_.-]', '_', os.path.basename(name.strip()))
        if base.endswith('.json'):
            base = base[:-5]
        if not base.strip('._-'):
            raise ValueError('give it a name (letters, digits, _ or -)')
        path = os.path.join('Game', 'sequences', base + '.json')
        if os.path.exists(path):
            raise ValueError('%s is there already: choose another name (Save as never overwrites)' % path)
        for entry, edited in zip(self.entries, entries):       # the page's edits (the other entries stay unsaved)
            entry['sequences'] = edited['sequences']
        entry = self.entries[index]
        write_json(path, entry['sequences'][0] if len(entry['sequences']) == 1 else entry['sequences'])
        entry['file'] = path
        self.saved[index] = copy.deepcopy(entry)
        self.write_map_list()
        return path

    def others(self):
        """the timeline files in Game/sequences/ that the map does not play"""
        playing = {os.path.normpath(e['file']) for e in self.entries if e['file']}
        found = []
        for path in sorted(glob.glob(os.path.join('Game', 'sequences', '*.json'))):
            if os.path.normpath(path) in playing:
                continue
            if os.path.normpath(path) == os.path.normpath(DEFAULT_STORM):
                continue        # the engine's standard storm is other maps': not a version of this one to switch to
            try:
                data = json.load(open(path))
            except ValueError:
                continue
            first = data[0] if isinstance(data, list) and data else data
            if isinstance(first, dict) and ('params' in first or 'sounds' in first or 'events' in first):
                found.append({"file": path, "name": first.get('name', ''), "note": about_file(path),
                              "modified": time.strftime('%Y-%m-%d %H:%M', time.localtime(os.path.getmtime(path)))})
        return found

    def switch(self, index, path):
        """the map plays this file in the entry's place (what the entry had unsaved is dropped)"""
        if os.path.normpath(path) not in {os.path.normpath(o['file']) for o in self.others()}:
            raise ValueError('%s is not a timeline file the map could play' % path)
        data = json.load(open(path))
        self.entries[index] = {"file": path, "sequences": data if isinstance(data, list) else [data]}
        self.saved[index] = copy.deepcopy(self.entries[index])
        self.write_map_list()

    def remove(self, index):
        """an entry out of the map's list (its file stays on disk)"""
        entry = self.entries.pop(index)
        self.saved.pop(index)
        self.write_map_list()
        return entry['file']


class Engine:
    """the engine, driven from here: it connects back, sends frames and its state"""

    LOG = '/tmp/timeline_editor_engine.log'

    def __init__(self, map_path, size):
        self.map_path, self.size = map_path, size
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(1)
        self.listener.settimeout(120)
        self.lock = threading.Lock()
        self.params = []
        self.schema = {}
        self.frame = None
        self.frame_state = {}
        self.frame_id = 0
        self.new_frame = threading.Condition()
        self.state = {}
        self.last = {}                 # the last command of each kind: sent again to a restarted engine
        self.alive = False
        self.gave_up = False           # it died 3 times in a minute: not started again (the page says so)
        self.stopping = False
        self.starts = []               # when it was started (not restarted for ever if it keeps dying)
        self.launch()

    def launch(self):
        port = self.listener.getsockname()[1]
        env = dict(os.environ, GE_REMOTE=str(port), GE_SKIP_MENU='1', GE_CLEAN_SHOTS='1', GE_SIZE=self.size)
        self.log = open(self.LOG if os.access('/tmp', os.W_OK) else os.devnull, 'w')
        self.process = subprocess.Popen(['./ge', self.map_path], env=env, stdout=self.log, stderr=subprocess.STDOUT)
        self.starts.append(time.time())
        try:
            self.connection, _ = self.listener.accept()
        except OSError:
            self.process.kill()         # it never connected: not left running
            raise
        self.alive = True
        threading.Thread(target=self.read, args=(self.connection,), daemon=True).start()

    def read(self, connection):
        try:
            self.receive(connection.makefile('rb'))
        except (OSError, ValueError):
            pass
        # the engine went away (it crashed, or its window was closed): started again,
        # put back where it was, unless it keeps dying
        self.alive = False
        if self.stopping:
            return
        try:
            self.process.wait(10)       # (the connection can go before the process does)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        recent = [t for t in self.starts if time.time() - t < 60]
        if len(recent) >= 3:
            print('the engine stopped 3 times in a minute (its output: %s): not starting it again' % self.LOG, flush=True)
            self.gave_up = True
            return
        print('the engine stopped (exit code %s): starting it again ...' % self.process.returncode, flush=True)
        try:
            self.launch()
        except OSError as error:
            print('could not start the engine again: %s' % error, flush=True)
            self.gave_up = True
            return
        for kind in ('sequences', 'solo', 'mute', 'pause', 'camera', 'fps', 'seek'):
            if kind in self.last:
                self.send(self.last[kind])

    def receive(self, stream):
        while True:
            line = stream.readline()
            if not line:
                break
            if line.startswith(b'F '):
                _, w, h, n = line.split()
                data = stream.read(int(n))
                picture = Image.frombytes('RGB', (int(w), int(h)), data)
                out = io.BytesIO()
                picture.save(out, 'JPEG', quality=82)
                with self.new_frame:
                    self.frame = out.getvalue()
                    self.frame_state = dict(self.state)       # (the engine sends its state just before each frame)
                    self.frame_id += 1
                    self.new_frame.notify_all()
            elif line.startswith(b'S '):
                try:
                    self.state = json.loads(line[2:])
                except ValueError:
                    pass
            elif line.startswith(b'P '):
                # what a timeline can hold, as the engine has it (Core/Aftermath, Core/Timeline):
                # its effects (name, group, min, max, standard, about), where a sound can be
                # placed, a sound's ranges. The page builds itself from this
                try:
                    self.schema = json.loads(line[2:])
                    self.params = self.schema.get('parameters', [])
                except (ValueError, AttributeError):
                    pass

    def send(self, command):
        """False if the engine is not there to take it"""
        with self.lock:
            self.last[command.get('cmd')] = command
            if not self.alive:
                return False
            try:
                self.connection.sendall((json.dumps(command, separators=(',', ':')) + '\n').encode())
                return True
            except OSError:
                return False

    def stop(self):
        self.stopping = True
        self.process.terminate()
        try:
            self.process.wait(5)
        except subprocess.TimeoutExpired:
            self.process.kill()


def make_handler(engine, sequences):
    lock = threading.Lock()          # the sequences: one request at a time (the server answers in threads)

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, data, kind='application/json', code=200):
            body = data if isinstance(data, bytes) else json.dumps(data).encode()
            self.send_response(code)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)

        def body(self):
            n = int(self.headers.get('Content-Length', 0))
            return json.loads(self.rfile.read(n) or b'{}')

        def do_GET(self):
            if self.path.split('?')[0] == '/':
                self.reply(open(PAGE, 'rb').read(), 'text/html; charset=utf-8')
            elif self.path == '/api/state':
                # (the timeline's length, where each sequence starts, its named moments: in "engine",
                # the engine's own state; what a timeline can hold: "params" and "schema", from it too)
                with lock:
                    state = {"map": sequences.map_path, "entries": sequences.entries,
                             "folder": os.path.join('Game', 'sequences') + '/',      # where timeline files live
                             "dirty": sequences.dirty, "saved": sequences.saved, "others": sequences.others(),
                             "notes": {e['file']: about_file(e['file']) for e in sequences.entries if e['file'] and about_file(e['file'])},
                             "engine": engine.state, "params": engine.params, "schema": engine.schema, "sounds": known_sounds(), "shapes": sound_shapes()}
                    self.reply(state)
            elif self.path.split('?')[0] == '/live.js':
                self.reply(open(os.path.join(os.path.dirname(PAGE), 'live.js'), 'rb').read(), 'text/javascript; charset=utf-8')
            elif self.path == '/api/engine':
                self.reply(dict(engine.state, alive=engine.alive, session=SESSION, frame=engine.frame_id))
            elif self.path.startswith('/frame'):
                # the next picture after the one the page has (waiting a moment for it), with the
                # engine's state at that frame; 204 if there is none yet (the engine is away)
                try:
                    after = int(self.path.split('after=')[1].split('&')[0])
                except (IndexError, ValueError):
                    after = -1
                with engine.new_frame:
                    engine.new_frame.wait_for(lambda: engine.frame is not None and engine.frame_id != after and engine.alive,
                                              timeout=1.0)
                    frame, number, state = engine.frame, engine.frame_id, dict(engine.frame_state)
                fresh = frame is not None and number != after and engine.alive
                state['alive'] = engine.alive
                state['gaveUp'] = engine.gave_up
                self.send_response(200 if fresh else 204)
                self.send_header('X-Session', SESSION)
                self.send_header('X-Frame', str(number))
                self.send_header('X-Engine', json.dumps(state))
                self.send_header('Cache-Control', 'no-store')
                if fresh:
                    self.send_header('Content-Type', 'image/jpeg')
                    self.send_header('Content-Length', str(len(frame)))
                    self.end_headers()
                    try:
                        self.wfile.write(frame)
                    except (BrokenPipeError, ConnectionResetError):
                        pass
                else:
                    self.send_header('Content-Length', '0')
                    self.end_headers()
            else:
                self.reply({"error": "not found"}, code=404)

        def do_POST(self):
            try:
                data = self.body()
                if not isinstance(data, dict):
                    raise ValueError('expected a JSON object')
                with lock:
                    self.post(data)
            except (ValueError, KeyError, TypeError, IndexError, OSError) as error:
                self.reply({"ok": False, "error": '%s: %s' % (type(error).__name__, error)}, code=400)

        def post(self, data):
            if self.path in ('/api/apply', '/api/save', '/api/saveas') and len(data.get('entries') or []) != len(sequences.entries):
                # a page that has not caught up with the file list (another page added or took one out)
                self.reply({"ok": False, "error": "the page's list of timeline files is out of date: reload the page"}, code=409)
                return
            if self.path == '/api/apply':
                for entry, edited in zip(sequences.entries, data['entries']):
                    entry['sequences'] = edited['sequences']
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "dirty": sequences.dirty})
            elif self.path == '/api/save':
                written = sequences.save(data['entries'])
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "files": written})
            elif self.path == '/api/saveas':
                try:
                    path = sequences.save_as(int(data.get('entry', 0)), str(data.get('name', '')), data['entries'])
                except ValueError as error:
                    self.reply({"ok": False, "error": str(error)})
                    return
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "file": path, "entries": sequences.entries, "saved": sequences.saved,
                            "others": sequences.others()})
            elif self.path == '/api/switch':
                try:
                    sequences.switch(int(data.get('entry', 0)), str(data.get('file', '')))
                except (ValueError, OSError) as error:
                    self.reply({"ok": False, "error": str(error)})
                    return
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "entries": sequences.entries, "saved": sequences.saved, "others": sequences.others()})
            elif self.path == '/api/revert':
                sequences.reload()
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "entries": sequences.entries, "saved": sequences.saved})
            elif self.path == '/api/add':
                try:
                    path = sequences.add(data['filename'], data['content'])
                except ValueError as error:
                    self.reply({"ok": False, "error": str(error)})
                    return
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "file": path, "entries": sequences.entries, "saved": sequences.saved, "others": sequences.others()})
            elif self.path == '/api/remove':
                removed = sequences.remove(int(data['entry']))
                engine.send({"cmd": "sequences", "json": sequences.all()})
                self.reply({"ok": True, "removed": removed, "entries": sequences.entries, "saved": sequences.saved, "others": sequences.others()})
            elif self.path in ('/api/seek', '/api/pause', '/api/camera', '/api/fps', '/api/mute', '/api/solo'):
                # (solo: only some lanes play, {"params": [[sequence, name]], "sounds": [[sequence, index]]};
                # the engine decides what the rest is then. Nothing is written: it is not part of the timeline)
                engine.send(dict(data, cmd=self.path.split('/')[-1]))
                self.reply({"ok": True})
            else:
                self.reply({"error": "not found"}, code=404)

    return Handler


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('map', nargs='?', default='Game/recursion/00_archive_ignition.json')
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--size', default='1280x720', help='the picture the engine renders (default 1280x720)')
    parser.add_argument('--fps', type=float, default=30, help='pictures a second sent to the page (1 to 60; default 30)')
    args = parser.parse_args()
    if not os.path.exists('ge'):
        sys.exit('run it from the engine folder (where ./ge is)')
    sequences = Sequences(args.map)
    print('starting the engine on %s ...' % args.map)
    engine = Engine(args.map, args.size)
    engine.send({"cmd": "fps", "fps": max(1.0, min(args.fps, 60.0))})      # (sent again to a restarted engine)
    server = Server(('127.0.0.1', args.port), make_handler(engine, sequences))
    print('timeline editor: http://localhost:%d  (sequences: %s)'
          % (args.port, ', '.join(e['file'] or 'inline' for e in sequences.entries) or 'none yet'))
    def stop(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)            # killed: the engine goes with it
    signal.signal(signal.SIGINT, stop)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        engine.stop()


if __name__ == '__main__':
    main()
