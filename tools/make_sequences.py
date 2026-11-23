#!/usr/bin/env python3
"""The scripted sequences (Core/Timeline): the storms after the sky's Moon
destruction (the Kugelblitz), as data.

  python3 tools/make_sequences.py    writes Game/sequences/kugelblitz_storm.json

That is the default storm: a map with a Moon destruction and no "Sequences"
of its own gets it (Game/lagoon.json). tools/make_recursion.py takes
dust_out_storm() from here for the 2035 archive footage
(Game/recursion/00_archive_ignition.json).

The storms are written as curves of their time (seconds since the last of
the moon fell in) and sampled into keyframes. Core/Aftermath reads the
parameters (its header lists them); the sounds play from the timeline.
"""
import json
import math
import os

ARRIVAL = 8.0          # the debris reaches the Earth (the sky's shell, the impact)
FRONT_START = 0.6      # the shockwave's wall sets off from the horizon ...
FRONT_DISTANCE = 2400  # ... this far upwind
END = 40.0             # sampled up to here (the last value holds)


def smoothstep(a, b, x):
    t = min(max((x - a) / (b - a), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, u):
    return a + (b - a) * u


def keys(f, step=0.25, tolerance=0.002):
    """f sampled every step up to END, the points a straight line between
    their neighbours already gives left out"""
    points = [(round(i * step, 3), f(i * step)) for i in range(int(END / step) + 1)]
    kept = [points[0]]
    for i in range(1, len(points) - 1):
        (t0, v0), (t1, v1), (t2, v2) = kept[-1], points[i], points[i + 1]
        if abs(mix(v0, v2, (t1 - t0) / (t2 - t0)) - v1) > tolerance:
            kept.append(points[i])
    kept.append(points[-1])
    return {"keys": [[t, round(v, 4)] for t, v in kept], "curve": "linear"}


def front_distance(t):
    return FRONT_DISTANCE - FRONT_DISTANCE / (ARRIVAL - FRONT_START) * (t - FRONT_START)


def calm(t):
    return smoothstep(0.3, 1.2, t) * (1 - smoothstep(ARRIVAL - 0.3, ARRIVAL, t))


def wind_turn(t):
    return smoothstep(0.3, ARRIVAL - 0.5, t)


def shake(storm):
    """the shockwave shakes the camera hard, then the gale keeps it trembling"""
    return lambda t: (0.035 * math.exp(-(t - ARRIVAL) * 1.1) if t >= ARRIVAL else 0.0) + 0.006 * storm(t)


def impact_sounds():
    """the debris arriving: the roar riding the front in, then the blast"""
    return [
        {"at": ARRIVAL, "sound": "blast", "volume": 1.0, "pitch": 0.75},
        {"at": ARRIVAL, "sound": "boom_1", "volume": 0.9, "pitch": 0.85},
        {"at": ARRIVAL, "sound": "gust_1", "volume": 1.0, "pitch": 0.8, "anchor": "upwind", "radius": 800},
        {"at": ARRIVAL + 0.45, "sound": "boom_2", "volume": 0.7, "pitch": 0.8},
    ]


def default_storm():
    """the gale after the Moon is gone, settling into a windier, foggier new
    normal (Game/lagoon.json)"""
    def storm(t):
        return smoothstep(ARRIVAL, ARRIVAL + 0.5, t) * mix(1.0, 0.2, smoothstep(14.0, 32.0, t))

    def roar(t):
        if t < ARRIVAL:
            return 1.1 * smoothstep(ARRIVAL - 1.5, ARRIVAL - 0.8, t)
        return 1.1 * max(storm(t), 1.0 - smoothstep(ARRIVAL, ARRIVAL + 0.5, t))

    return {
        "name": "the storm after the Moon",
        "trigger": "kugelblitzFormed",
        "params": {
            "debrisArrival": ARRIVAL,
            "storm": keys(storm),
            "calm": keys(calm),
            "windTurn": keys(wind_turn),
            "shake": keys(shake(storm)),
            "groundDebris": keys(lambda t: smoothstep(ARRIVAL - 0.3, ARRIVAL + 0.3, t) * mix(1.0, 0.12, smoothstep(10, 28, t))),
            "wall": keys(lambda t: smoothstep(FRONT_START, FRONT_START + 0.6, t) * (1 - smoothstep(ARRIVAL, ARRIVAL + 0.8, t))),
            "frontDistance": keys(front_distance),
            "debrisRain": keys(lambda t: smoothstep(ARRIVAL - 0.4, ARRIVAL + 0.4, t) * mix(1.0, 0.15, smoothstep(8, 28, t))),
            "gusts": keys(lambda t: 1.0 if t >= ARRIVAL + 0.4 else 0.0),
        },
        "sounds": [
            {"at": ARRIVAL - 1.5, "sound": "storm_loop", "loop": True, "volume": keys(roar),
             "anchor": "front", "radius": FRONT_DISTANCE * 1.25},
            {"at": ARRIVAL, "sound": "spray_loop", "loop": True, "volume": keys(lambda t: 0.45 * storm(t))},
        ] + impact_sounds(),
    }


def dust_out_storm(dust_out=24.0, wet=0.8, fires=30, gain=1.8):
    """the 2035 footage: silence while the debris crosses, then it only gets
    worse: matter raining out of the sky, fires lit round the camera, the dust
    thickening, lightning in it, rain and drops on the lens, louder and louder,
    until there is nothing but dust (dust_out seconds after the moon is gone)"""
    def storm(t):
        return smoothstep(ARRIVAL, ARRIVAL + 0.5, t)

    def dust(t):
        return smoothstep(ARRIVAL + 0.5, dust_out, t)

    def cover(t):
        return smoothstep(ARRIVAL + 2.0, dust_out, t) ** 2

    def rage(t):
        return smoothstep(ARRIVAL, dust_out, t)

    def roar(t):
        if t < ARRIVAL:
            return 1.1 * smoothstep(ARRIVAL - 1.5, ARRIVAL - 0.8, t)
        return 1.1 * storm(t) * (0.8 + 0.6 * rage(t)) + 0.6 * dust(t)

    return {
        "name": "the storm after the Moon, until nothing is left but dust",
        "trigger": "kugelblitzFormed",
        "params": {
            "debrisArrival": ARRIVAL,
            "gain": gain,
            "storm": keys(storm),
            "calm": keys(calm),
            "windTurn": keys(wind_turn),
            "shake": keys(shake(storm)),
            "frontDistance": keys(front_distance),
            "debrisRain": keys(lambda t: smoothstep(ARRIVAL - 0.4, ARRIVAL + 0.4, t)),
            "dust": keys(dust),
            "dustVeil": keys(cover),
            "lightning": keys(lambda t: 0.75 * smoothstep(0.25, 0.85, cover(t))),
            "thunderVolume": keys(lambda t: 0.8 + 0.5 * rage(t)),
            "rain": keys(lambda t: smoothstep(ARRIVAL - 0.2, ARRIVAL + 1.5, t) * wet),
            "lensWater": keys(lambda t: smoothstep(ARRIVAL - 0.2, ARRIVAL + 1.5, t) * wet),
            "fires": keys(lambda t: min(max((t - (ARRIVAL + 0.2)) / 0.55, 0.0), fires), step=0.5, tolerance=0.01),
            "gusts": keys(lambda t: (1.0 + 2.0 * rage(t)) if t >= ARRIVAL + 0.4 else 0.0),
            "gustVolume": keys(lambda t: 1.0 + 0.4 * rage(t)),
            "booms": keys(lambda t: 1.0 / (2.8 - 2.2 * rage(t)) if t >= ARRIVAL + 1.0 else 0.0),
            "boomVolume": keys(lambda t: 0.5 + 0.5 * rage(t)),
            "thuds": keys(lambda t: 1.44 * (0.3 + dust(t)) if dust(t) > 0.05 else 0.0),     # per second
        },
        "sounds": [
            {"at": ARRIVAL - 1.5, "sound": "storm_loop", "loop": True, "volume": keys(roar),
             "anchor": "front", "radius": FRONT_DISTANCE * 1.25},
            {"at": ARRIVAL, "sound": "spray_loop", "loop": True,
             "volume": keys(lambda t: 0.45 * storm(t) + 0.35 * dust(t))},
            {"at": ARRIVAL, "sound": "storm_loop", "loop": True, "pitch": 0.55,
             "volume": keys(lambda t: 1.2 * rage(t))},                          # a deep rumble under the roar
            {"at": ARRIVAL + 0.5, "sound": "wind_loop", "loop": True, "pitch": 0.8,
             "volume": keys(lambda t: 1.1 * dust(t))},                          # the wind's howl
        ] + impact_sounds(),
    }


if __name__ == '__main__':
    path = 'Game/sequences/kugelblitz_storm.json'
    os.makedirs(os.path.dirname(path), exist_ok=True)
    json.dump(default_storm(), open(path, 'w'), indent=1)
    print(path)
