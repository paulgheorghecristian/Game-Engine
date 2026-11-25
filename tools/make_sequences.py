#!/usr/bin/env python3
"""The scripted sequences (Core/Timeline): the storms after the sky's Moon
destruction (the Kugelblitz), as data.

  python3 tools/make_sequences.py [--force]

writes, if they are not there yet (--force: over them):
  Game/sequences/kugelblitz_storm.json   the default storm: a map with a Moon
                                         destruction and no "Sequences" of its
                                         own gets it (Game/lagoon.json)
  Game/sequences/00_archive_storm.json   the 2038 archive footage's storm, until
                                         nothing is left but dust
                                         (Game/recursion/00_archive_ignition.json
                                         refers to it)

After that the files are the source: tune them in tools/timeline_editor.py
(this would overwrite the tuning only with --force).

The storms are a few keyframes per effect (the engine blends smoothly between
them). The default storm's times are seconds since the last of the moon fell in
("trigger": "kugelblitzFormed": any map's Moon destruction, however long it
takes). The 00 archive's is the whole animation on one clock from its start
("trigger": "mapStart"): the Moon's destruction itself on it too, as "collapse".
Core/Aftermath reads the parameters (its header lists them); the sounds play
from the timeline.
"""
import json
import math
import os

ARRIVAL = 8.0          # the debris reaches the Earth (the sky's shell, the impact)
SHELL_GONE = 4.0       # ... and has thinned out this long after
FRONT_START = 0.6      # the shockwave's wall sets off from the horizon ...
FRONT_DISTANCE = 2400  # ... this far upwind
DUST_OUT = 24.0        # the 2038 footage: nothing left but dust
# the 00 archive's Moon: whole for MOON_WHOLE seconds, then falling in over MOON_FALLS
# (as tools/make_recursion.py plays it there: collapseDelay 4, collapseSpeed 5 a minute)
MOON_WHOLE = 4.0
MOON_FALLS = 12.0


def smoothstep(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def keys(f, times):
    """a curve as a few keyframes: f at these times (the engine blends
    smoothly between them); few, so that each can be moved by hand"""
    return [[round(t, 3), round(f(t), 4)] for t in times]


def linear(points):
    return {"keys": [[t, v] for t, v in points], "curve": "linear"}


# the calm before: the wind dies and turns to blow from the hole
CALM = [[0.3, 0], [1.2, 1], [ARRIVAL - 0.3, 1], [ARRIVAL, 0]]
WIND_TURN = [[0.3, 0], [ARRIVAL - 0.5, 1]]
# the shockwave shakes the camera hard, then the gale keeps it trembling
SHAKE = [[ARRIVAL - 0.02, 0], [ARRIVAL, 0.035], [ARRIVAL + 0.6, 0.024], [ARRIVAL + 1.5, 0.013],
         [ARRIVAL + 3, 0.0073], [ARRIVAL + 6, 0.006]]
# the shockwave's wall, coming in from the horizon
FRONT = linear([[FRONT_START, FRONT_DISTANCE], [ARRIVAL, 0]])
ROAR_IN = [[ARRIVAL - 1.5, 0], [ARRIVAL - 0.8, 1.1]]     # silence, then the debris is heard coming
# the debris shell across the sky: at the hole, reaching us at the impact, then gone
SHELL = linear([[0, 0], [ARRIVAL, 1], [ARRIVAL + SHELL_GONE, 2]])


def weather(points):
    """the gale: its wind, fog, clouds and waves (each its own effect), all on the same keys"""
    return {name: [list(k) for k in points] for name in ("wind", "fog", "clouds", "waves")}


# the sounds the storm's own noises take turns with (Core/Aftermath plays them on its schedule:
# how often and how loud are the "gusts", "booms", "thuds", "lightning" effects)
NOISES = {
    "gusts": ["gust_1", "gust_2", "gust_3"],
    "booms": ["boom_1", "boom_2", "boom_3", {"sound": "blast", "volume": 0.7}],
    "thuds": ["land"],
    "thunder": ["boom_1", "boom_2", "boom_3"],
}


def impact_sounds():
    """the debris arriving: the blast"""
    return [
        {"at": ARRIVAL, "sound": "blast", "volume": 1.0, "pitch": 0.75},
        {"at": ARRIVAL, "sound": "boom_1", "volume": 0.9, "pitch": 0.85},
        {"at": ARRIVAL, "sound": "gust_1", "volume": 1.0, "pitch": 0.8, "anchor": "upwind", "radius": 800},
        {"at": ARRIVAL + 0.45, "sound": "boom_2", "volume": 0.7, "pitch": 0.8},
    ]


def default_storm():
    """the gale after the Moon is gone, settling into a windier, foggier new
    normal (Game/lagoon.json)"""
    return {
        "name": "the storm after the Moon",
        "trigger": "kugelblitzFormed",
        "params": {
            "debrisShell": SHELL,
            **weather([[ARRIVAL, 0], [ARRIVAL + 0.5, 1], [14, 1], [32, 0.2]]),
            "calm": CALM,
            "windTurn": WIND_TURN,
            "shake": SHAKE,
            "groundDebris": [[ARRIVAL - 0.3, 0], [ARRIVAL + 0.3, 1], [10, 1], [28, 0.12]],
            "wall": [[FRONT_START, 0], [FRONT_START + 0.6, 1], [ARRIVAL, 1], [ARRIVAL + 0.8, 0]],
            "frontDistance": FRONT,
            "debrisRain": [[ARRIVAL - 0.4, 0], [ARRIVAL + 0.4, 1], [28, 0.15]],
            "gusts": [[ARRIVAL + 0.4, 0], [ARRIVAL + 0.45, 1]],
        },
        "noises": NOISES,
        "sounds": [
            {"at": ARRIVAL - 1.5, "sound": "storm_loop", "loop": True, "anchor": "front", "radius": FRONT_DISTANCE * 1.25,
             "volume": ROAR_IN + [[14, 1.1], [32, 0.22]]},
            {"at": ARRIVAL, "sound": "spray_loop", "loop": True,
             "volume": [[ARRIVAL, 0], [ARRIVAL + 0.5, 0.45], [14, 0.45], [32, 0.09]]},
        ] + impact_sounds(),
    }


def dust_out_storm(wet=0.8, fires=30, gain=1.8):
    """the 2038 footage: silence while the debris crosses, then it only gets
    worse: matter raining out of the sky, fires lit round the camera, the dust
    thickening, lightning in it, rain and drops on the lens, louder and louder,
    until there is nothing but dust (DUST_OUT seconds after the moon is gone)"""
    def cover(t):                       # how much of the picture the dust hides
        return smoothstep(ARRIVAL + 2.0, DUST_OUT, t) ** 2

    def rage(t):                        # 0 at the impact .. 1 when the dust has everything
        return smoothstep(ARRIVAL, DUST_OUT, t)

    return {
        "name": "the storm after the Moon, until nothing is left but dust",
        "trigger": "kugelblitzFormed",
        "params": {
            "debrisShell": SHELL,
            "gain": gain,
            **weather([[ARRIVAL, 0], [ARRIVAL + 0.5, 1]]),
            "calm": CALM,
            "windTurn": WIND_TURN,
            "shake": SHAKE,
            "frontDistance": FRONT,
            "debrisRain": [[ARRIVAL - 0.4, 0], [ARRIVAL + 0.4, 1]],
            "dust": [[ARRIVAL + 0.5, 0], [DUST_OUT, 1]],
            "dustVeil": keys(cover, [ARRIVAL + 2, 14, 17, 20, DUST_OUT]),
            # lightning: none until the dust covers a good part of the picture
            "lightning": [[17, 0], [22.3, 0.75]],
            "thunderVolume": [[ARRIVAL, 0.8], [DUST_OUT, 1.3]],
            "rain": [[ARRIVAL - 0.2, 0], [ARRIVAL + 1.5, wet]],
            "lensWater": [[ARRIVAL - 0.2, 0], [ARRIVAL + 1.5, wet]],
            "fires": linear([[ARRIVAL + 0.2, 0], [ARRIVAL + 0.2 + fires * 0.55, fires]]),
            "gusts": [[ARRIVAL + 0.4, 0], [ARRIVAL + 0.45, 1], [DUST_OUT, 3]],
            "gustVolume": [[ARRIVAL, 1], [DUST_OUT, 1.4]],
            "booms": [[ARRIVAL + 0.99, 0]] + keys(lambda t: 1.0 / (2.8 - 2.2 * rage(t)), [ARRIVAL + 1, 14, 18, 21, DUST_OUT]),
            "boomVolume": [[ARRIVAL, 0.5], [DUST_OUT, 1]],
            "thuds": [[10.5, 0], [10.6, 0.5], [DUST_OUT, 1.87]],
        },
        "noises": NOISES,
        "sounds": [
            {"at": ARRIVAL - 1.5, "sound": "storm_loop", "loop": True, "anchor": "front", "radius": FRONT_DISTANCE * 1.25,
             "volume": ROAR_IN + [[ARRIVAL + 0.5, 0.9], [DUST_OUT, 2.14]]},
            {"at": ARRIVAL, "sound": "spray_loop", "loop": True,
             "volume": [[ARRIVAL, 0], [ARRIVAL + 0.5, 0.45], [DUST_OUT, 0.8]]},
            {"at": ARRIVAL, "sound": "storm_loop", "loop": True, "pitch": 0.55,
             "volume": [[ARRIVAL, 0], [DUST_OUT, 1.2]]},                          # a deep rumble under the roar
            {"at": ARRIVAL + 0.5, "sound": "wind_loop", "loop": True, "pitch": 0.8,
             "volume": [[ARRIVAL + 0.5, 0], [DUST_OUT, 1.1]]},                    # the wind's howl
        ] + impact_sounds(),
    }


def on_scene_clock(sequence, formed, collapse):
    """a storm timed from the black hole forming, as one on the scene's clock from 0 (the
    black hole forms at `formed`), with the Moon's destruction on it as its "collapse" """
    def shift(value):
        if isinstance(value, list):
            return [[round(k[0] + formed, 3)] + k[1:] for k in value]
        if isinstance(value, dict):
            return dict(value, keys=[[round(k[0] + formed, 3)] + k[1:] for k in value['keys']])
        return value                                    # a constant: all the time
    out = dict(sequence, trigger="mapStart")
    params = {"collapse": collapse}
    for name, value in sequence['params'].items():
        params[name] = shift(value)
    out['params'] = params
    out['sounds'] = [dict(s, at=round(s['at'] + formed, 3), **({"volume": shift(s['volume'])} if 'volume' in s else {}))
                     for s in sequence.get('sounds', [])]
    if sequence.get('events'):
        out['events'] = [dict(e, at=round(e['at'] + formed, 3)) for e in sequence['events']]
    return out


def archive_storm():
    """the 00 archive: the Moon whole, then falling in, then the storm until nothing is left
    but dust, all on one clock from the start"""
    formed = MOON_WHOLE + MOON_FALLS
    out = on_scene_clock(dust_out_storm(), formed, linear([[MOON_WHOLE, 0], [formed, 1]]))
    # matter thrown straight at us: as the last of the Moon falls in (collapse 0.85 .. 0.95),
    # until just before the debris arrives
    params = dict(out['params'])
    out['params'] = dict(list(params.items())[:2] + [("ejecta", [
        [round(MOON_WHOLE + 0.85 * MOON_FALLS, 3), 0], [round(MOON_WHOLE + 0.95 * MOON_FALLS, 3), 1],
        [formed + ARRIVAL - 1.5, 1], [formed + ARRIVAL, 0]])] + list(params.items())[2:])
    return out


FILES = {'Game/sequences/kugelblitz_storm.json': default_storm,
         'Game/sequences/00_archive_storm.json': archive_storm,
         'Game/sequences/00_archive_storm_alternative.json': archive_storm}      # (00_archive_ignition_alternative's)

if __name__ == '__main__':
    import sys
    force = '--force' in sys.argv
    for path, make in FILES.items():
        if os.path.exists(path) and not force:
            print('%s exists (kept; --force to overwrite)' % path)
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        json.dump(make(), open(path, 'w'), indent=1)
        print('wrote', path)
