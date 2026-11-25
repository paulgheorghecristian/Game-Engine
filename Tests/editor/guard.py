"""guard: imported first by a test suite (run from an engine folder). Keeps a copy of every map and
timeline file there (Game/*.json, Game/recursion/*.json, Game/sequences/*.json). unchanged(): whether
they are all as they were (none changed, removed or added). When the suite ends, any it changed or
removed is put back and any it added is deleted, whatever happened in the suite."""
import atexit, glob, os

def _files():
    return sorted(set(glob.glob('Game/*.json') + glob.glob('Game/recursion/*.json') + glob.glob('Game/sequences/*.json')))

def _read(path):
    with open(path, 'rb') as f:
        return f.read()

_kept = {path: _read(path) for path in _files()}

def changed():
    """the files not as they were (changed, removed or added)"""
    now = _files()
    out = [p for p in now if p not in _kept or _read(p) != _kept[p]]
    return out + [p for p in _kept if p not in now]

def unchanged():
    return not changed()

def _restore():
    for path in _files():
        if path not in _kept:
            os.remove(path)
    for path, data in _kept.items():
        if not os.path.exists(path) or _read(path) != data:
            with open(path, 'wb') as f:
                f.write(data)

atexit.register(_restore)
