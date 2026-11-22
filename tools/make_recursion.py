"""RECURSION: The Year 2000 -- one scene per idea of the game plan, built from
what the engine already has (the canyon and lagoon maps, the Kugelblitz
moon, boxes, the old models and textures). Each is a map in Game/recursion/:

  00_archive_ignition     the archive video: the Kugelblitz ignites in the
                          Moon (plays live, slowly), grainy like old footage
  01_room7                Ch.1: hospital room, CRT, desk, fuse box, keypad door
  02_corridor_subject4    Ch.2: peeling corridor of cells, Subject 4's window
  02_twin_levers          Ch.2: two rooms, a lever in each, walkie-talkies
  02_security_office      Ch.2: wall of CCTV monitors; the cleaning robot's hall
  02_waiting_room         Ch.2 "things that don't add up": fake windows,
                          clocks stuck at 11:59, the radio, the newspapers
  02_records_room         Ch.2 the tapes: shelves of cassettes, the shutter
  03_stairwell            Ch.3: the EXIT door and the endless stairs up
  03_canyon_hatch         Ch.3: out of the hatch into the canyon at night,
                          the Moon cracked (accretion vortex)
  04_monument             Ch.4 clue 1: the AURA statue and its plaque (jets)
  04_survivor_camp        Ch.4 clue 2: the warm campfire, the journal pages
  04_archive_kiosk        Ch.4 clue 3: the kiosk at noon, mirrors round the pool
  04_telemetry            Ch.4 clue 4: the kiosk printing at night, the jets
  05_lighthouse_hill      Ch.5: the lamp path to the lighthouse, its beam on
                          the Moon in its death spiral
  05_lighthouse_interior  Ch.5: walls of CRT screens, the twin-lever console
  06_ending               sunrise over the lagoon, no Moon, a ring in the sky

The Moon is the clock (the plan's table): each outdoor scene freezes the
Kugelblitz at its chapter's stage (collapse 0.45 cracks, 0.66 jets, 0.88
death spiral). Run after make_canyon.py / make_lagoon.py (it starts from
their maps):  python3 tools/make_recursion.py
"""
import copy, json, math, os, random

random.seed(2000)
OUT = 'Game/recursion'

# --------------------------------------------------------------------------
# helpers (same conventions as the other generators)
# --------------------------------------------------------------------------
def mat(diffuse, texture=None, normal=None, emissive=0.0, shininess=20.0, extra=None):
    m = {"ambient": [0, 0, 0], "diffuse": list(diffuse), "specular": [0.2, 0.2, 0.2], "shininess": shininess}
    if texture: m["Texture"] = texture
    if normal: m["NormalMapTexture"] = normal
    if emissive: m["emissive"] = emissive
    if extra: m.update(extra)
    return m

def box_physics(size):
    return {"BoundingBodyType": ["cube"], "scale": [round(s, 2) for s in size], "mass": 0}

def entity(mesh, pos, scale, rot=(0, 0, 0), material=None, physics=None, cast_shadow=True):
    rc = {"Mesh": mesh}
    if not cast_shadow: rc["castShadow"] = False
    if material: rc["Material"] = material
    comps = {"RenderComponent": rc}
    if physics: comps["PhysicsComponent"] = physics
    return {"Transform": {"position": [round(v, 2) for v in pos], "rotation": [round(v, 2) for v in rot],
                          "scale": [round(v, 3) for v in (scale if isinstance(scale, (list, tuple)) else (scale,) * 3)]},
            "Components": comps}

def box(pos, size, material, solid=False, rot=(0, 0, 0), cast_shadow=True):
    """a box from cube4.obj (a unit cube: its scale is its size), centred at pos"""
    return entity("res/models/cube4.obj", pos, size, rot, material, box_physics(size) if solid else None, cast_shadow)

def light(pos, color, kind, scale, rot=(0, 0, 0), **attrs):
    a = {"color": list(color), "type": kind}
    a.update(attrs)
    return {"Transform": {"position": [round(v, 2) for v in pos], "rotation": list(rot),
                          "scale": list(scale)}, "Attributes": a}

def aim(frm, to):
    d = [to[i] - frm[i] for i in range(3)]
    n = math.sqrt(sum(c * c for c in d)); d = [c / n for c in d]
    return (round(math.degrees(math.asin(d[1])), 2), round(math.degrees(math.atan2(-d[0], -d[2])), 2), 0)

def yaw_towards(frm, to):
    """the camera yaw (degrees; 0 looks down -z) looking from frm towards to (xz)"""
    return round(math.degrees(math.atan2(-(to[0] - frm[0]), -(to[1] - frm[1]))), 1)

# materials
WHITE = mat((0.92, 0.92, 0.9), extra={"roughness": 0.7})
PAPER = mat((0.95, 0.94, 0.88), extra={"roughness": 0.9})
OFFWHITE = mat((0.8, 0.8, 0.76), extra={"roughness": 0.8})
# (cube4.obj maps a texture once over each face: on a wall it is one huge
# blotch, so the lab is plain paint: institutional green over a darker band)
PEELING = mat((0.42, 0.5, 0.44), extra={"roughness": 0.85})          # the walls' tired green paint
WAINSCOT = mat((0.22, 0.27, 0.24), extra={"roughness": 0.6})
TERRAZZO = mat((0.3, 0.3, 0.29), shininess=80, extra={"roughness": 0.22, "specularStrength": 0.7})   # waxed vinyl: reflects
CONCRETE = mat((0.36, 0.36, 0.35), extra={"roughness": 0.9})
BEIGE = mat((0.78, 0.72, 0.6), shininess=40, extra={"roughness": 0.5})                    # 1999 computer plastic
DARK_PLASTIC = mat((0.08, 0.08, 0.09), shininess=60, extra={"roughness": 0.4})
STEEL = mat((0.55, 0.56, 0.6), "res/textures/metal.jpg", "res/textures/metal_normal.jpg", extra={"metallic": 0.6, "roughness": 0.45})
RUSTY = mat((0.75, 0.7, 0.65), "res/textures/158.JPG", "res/textures/158_norm.JPG", extra={"roughness": 0.65, "metallic": 0.3})
WOOD = mat((0.42, 0.28, 0.17), extra={"roughness": 0.6})
MATTRESS = mat((0.85, 0.88, 0.9), extra={"roughness": 0.95})
BRONZE = mat((0.62, 0.5, 0.38), shininess=60, extra={"metallic": 0.35, "roughness": 0.5})
MIRROR = mat((0.9, 0.92, 0.95), shininess=200, extra={"metallic": 1.0, "roughness": 0.04})
GLASS_DARK = mat((0.02, 0.03, 0.03), shininess=150, extra={"roughness": 0.05, "specularStrength": 1.0})
def glow(color, nightOnly=False):
    return mat(color, emissive=1.0, extra={"emissiveNightOnly": nightOnly})
TUBE = glow((1.0, 1.0, 0.95))
SCREEN_GREEN = glow((0.35, 1.0, 0.45))
SCREEN_BLUE = glow((0.45, 0.7, 1.0))
EXIT_GREEN = glow((0.2, 1.0, 0.35))
LED_RED = glow((1.0, 0.1, 0.05))

# --------------------------------------------------------------------------
# the outdoor maps: ground heights from their terrain meshes
# --------------------------------------------------------------------------
class Ground:
    """bilinear heights on a regular-grid terrain OBJ (as placed by its map entity)"""
    def __init__(self, obj, position=(0, 0, 0), scale=(1, 1, 1)):
        verts = [tuple(map(float, l.split()[1:4])) for l in open(obj) if l.startswith('v ')]
        xs = sorted(set(round(v[0], 4) for v in verts)); zs = sorted(set(round(v[2], 4) for v in verts))
        self.x0, self.z0 = xs[0], zs[0]
        self.dx, self.dz = xs[1] - xs[0], zs[1] - zs[0]
        self.nx, self.nz = len(xs), len(zs)
        self.h = {}
        for v in verts:
            self.h[(round((v[0] - self.x0) / self.dx), round((v[2] - self.z0) / self.dz))] = v[1]
        self.pos, self.scale = position, scale
    def __call__(self, x, z):
        lx = (x - self.pos[0]) / self.scale[0]; lz = (z - self.pos[2]) / self.scale[2]
        fx = min(max((lx - self.x0) / self.dx, 0), self.nx - 1.001); fz = min(max((lz - self.z0) / self.dz, 0), self.nz - 1.001)
        i, j = int(fx), int(fz); u, v = fx - i, fz - j
        g = lambda a, b: self.h.get((a, b), 0.0)
        y = (g(i, j) * (1 - u) + g(i + 1, j) * u) * (1 - v) + (g(i, j + 1) * (1 - u) + g(i + 1, j + 1) * u) * v
        return y * self.scale[1] + self.pos[1]

def load(path):
    return json.load(open(path))

CANYON = load('Game/canyon.json')
LAGOON = load('Game/lagoon.json')
canyon_ground = Ground('res/models/canyon_terrain.obj')
_lt = LAGOON['Entities'][0]['Transform']
lagoon_ground = Ground('res/models/terrain4.obj', _lt['position'], _lt['scale'])
# the Moon hangs up the valley (seen from the hatch, and over the statue
# from the plaza); the canyon's own moon sits in the east behind a cliff
_m = (0.32, 0.6, -0.73)
CANYON_MOON = [round(c / math.sqrt(sum(v * v for v in _m)), 4) for c in _m]
LAGOON_MOON = LAGOON['Environment']['moonDirection']
CANYON_TOP = 1.5                                    # the plaza paving
PED, POOL_Z0, POOL_Z1 = (0.0, -1022.0), -940.0, -660.0
CANYON_SPAWN = (40.0, 820.0)
LIGHTHOUSE = (515.36, 166.82, -1214.73)
CAMPFIRE = (721.0, 26.0, -360.0)

def kugelblitz(direction, collapse, play=False, speed=0.0, delay=0.0):
    """the sky's Kugelblitz in the Moon, held at a stage of its collapse (or playing)"""
    h = {"sky": True, "direction": direction, "angularRadius": 0.03, "kugelblitz": True,
         "collapse": collapse, "collapseSpeed": speed, "collapseDelay": delay, "collapseSite": [0.36, 0.77],
         "lensing": 0.8, "influence": 11, "colorShift": 0.7, "diskInner": 3, "diskOuter": 16,
         "diskColor": [1.0, 0.55, 0.26], "diskBrightness": 5.0, "diskOpacity": 0.55, "diskSpeed": 0.8,
         "diskTilt": [41.54, -21.34], "glow": 0, "moonStream": 0, "cloudClear": 0.7}
    if play: h["autoPlay"] = True
    return h

def outdoor(base, time_of_day, moon_direction, holes, start, extra_env=None):
    """a story scene on an outdoor map: its time held, its Moon at a stage"""
    g = copy.deepcopy(base)
    env = g["Environment"]
    env.update({"timeOfDay": time_of_day, "timePaused": True, "moon": True, "moonDirection": moon_direction,
                "moonSize": 0.04, "milkyWay": 0.9, "milkyWayCenter": moon_direction})
    if extra_env: env.update(extra_env)
    g["BlackHoles"] = holes
    g["PlayerStart"] = start
    for l in g["Lights"]:
        if l["Attributes"]["type"] == "DIR":
            l["Attributes"]["dir"] = moon_direction                                      # moonlight from the Moon
    return g

def start(x, y, z, yaw=0.0, pitch=0.0):
    return {"position": [round(x, 2), round(y, 2), round(z, 2)], "yaw": yaw, "pitch": pitch}

# --------------------------------------------------------------------------
# the lab: rooms built of boxes
# --------------------------------------------------------------------------
H_ROOM = 52.0                                       # ~2.9 m (a person is ~32)

def interior_env(**over):
    e = {"sky": "night", "zenithColor": [0, 0, 0], "horizonColor": [0, 0, 0], "starIntensity": 0.0,
         "moon": False, "moonColor": [0, 0, 0], "fogDensity": 0.0, "fogEnd": 0, "cloudCoverage": 0.0,
         "ambient": 0.06, "ambientSkyTint": [0.9, 0.95, 1.05], "ambientGroundTint": [0.6, 0.6, 0.6],
         "emissiveStrength": 1.3, "pbr": True, "hdr": True, "exposure": 1.1, "bloomStrength": 0.45,
         "bloomThreshold": 0.9, "aoStrength": 1.0, "aoRadius": 12, "saturation": 0.85, "grade": 0.6,
         "dayLength": 0, "fogLightGlow": 0.0, "lensDirt": 0.4, "chromaticAberration": 0.4, "filmGrain": 0.04}
    e.update(over)
    return e

def room(ents, cx, cz, w, d, h=H_ROOM, walls=PEELING, floor=TERRAZZO, ceiling=OFFWHITE, doors=(), band=True):
    """floor, ceiling and four walls (solid) of a w x d room at (cx, cz); doors:
    (side 'n'|'s'|'e'|'w', offset along the wall from its centre, width, height)"""
    t = 4.0
    ents.append(box((cx, -t / 2, cz), (w + 2 * t, t, d + 2 * t), floor, True))
    ents.append(box((cx, h + t / 2, cz), (w + 2 * t, t, d + 2 * t), ceiling, True))
    for side in 'nsew':
        along = w if side in 'ns' else d
        gaps = sorted([(o - dw / 2, o + dw / 2, dh) for (s, o, dw, dh) in doors if s == side])
        segments, a = [], -along / 2
        for (g0, g1, dh) in gaps:
            segments.append((a, g0, 0.0, h)); segments.append((g0, g1, dh, h)); a = g1
        segments.append((a, along / 2, 0.0, h))
        for (s0, s1, y0, y1) in segments:
            if s1 - s0 < 0.5 or y1 - y0 < 0.5: continue
            mid, length = (s0 + s1) / 2, s1 - s0
            if side == 'n': pos, size = (cx + mid, (y0 + y1) / 2, cz - d / 2 - t / 2), (length, y1 - y0, t)
            elif side == 's': pos, size = (cx + mid, (y0 + y1) / 2, cz + d / 2 + t / 2), (length, y1 - y0, t)
            elif side == 'w': pos, size = (cx - w / 2 - t / 2, (y0 + y1) / 2, cz + mid), (t, y1 - y0, length)
            else: pos, size = (cx + w / 2 + t / 2, (y0 + y1) / 2, cz + mid), (t, y1 - y0, length)
            ents.append(box(pos, size, walls, True))
            if band and y0 < 1.0 and walls is not CONCRETE:
                bh = 16.0
                inset = {'n': (0, 0, 0.6), 's': (0, 0, -0.6), 'w': (0.6, 0, 0), 'e': (-0.6, 0, 0)}[side]
                bsize = (size[0], bh, size[2]) if side in 'ns' else (size[0], bh, size[2])
                bsize = (bsize[0], bh, 0.4) if side in 'ns' else (0.4, bh, bsize[2])
                ents.append(box((pos[0] + inset[0] + (size[0] / 2 - 0.2) * (1 if side == 'w' else -1 if side == 'e' else 0),
                                 bh / 2, pos[2] + (size[2] / 2 - 0.2) * (1 if side == 'n' else -1 if side == 's' else 0)),
                                bsize, WAINSCOT, cast_shadow=False))

def tube(ents, lights, x, z, h=H_ROOM, along='x', flicker=0.04, color=(0.85, 0.95, 1.0), radius=180, shadow=True):
    """a buzzing fluorescent tube on the ceiling, and its light"""
    size = (26, 1.5, 3) if along == 'x' else (3, 1.5, 26)
    ents.append(box((x, h - 1.0, z), size, TUBE, cast_shadow=False))
    ents.append(box((x, h - 0.3, z), (size[0] + 3, 0.6, size[2] + 4), STEEL, cast_shadow=False))
    lights.append(light((x, h - 4, z), color, "POINT", (radius, radius, radius), cast_shadow=shadow, shadowMapSize=512,
                        diffuse_attenuation=[650, 12, 4], specular_attenuation=[500, 12, 4],
                        flicker=flicker, flickerSpeed=28))

def door(ents, x, z, along='x', locked=True, window=True, lit=False):
    """a closed door filling a 24 x 40 doorway (along: the wall's direction)"""
    size = (24, 40, 2.5) if along == 'x' else (2.5, 40, 24)
    ents.append(box((x, 20, z), size, STEEL, True))
    if window:
        wsize = (7, 9, 2.8) if along == 'x' else (2.8, 9, 7)
        ents.append(box((x, 29, z), wsize, glow((0.95, 0.85, 0.6)) if lit else GLASS_DARK, cast_shadow=False))

def crt(ents, x, y, z, yaw=0, on=False, screen=None):
    """a beige CRT monitor on a surface at height y, facing yaw (0: +z)"""
    a = math.radians(yaw); fx, fz = math.sin(a), math.cos(a)
    ents.append(box((x - fx * 2, y + 7, z - fz * 2), (15, 13, 14), BEIGE, rot=(0, yaw, 0)))
    ents.append(box((x + fx * 5.3, y + 7.5, z + fz * 5.3), (11, 9, 0.6), screen or (SCREEN_GREEN if on else GLASS_DARK),
                    rot=(0, yaw, 0), cast_shadow=False))

def desk(ents, x, z, w=55, d=26, yaw=0, top=26):
    a = math.radians(yaw)
    ents.append(box((x, top - 1.2, z), (w, 2.4, d), WOOD, True, rot=(0, yaw, 0)))
    for sx in (-1, 1):
        for sz in (-1, 1):
            lx, lz = sx * (w / 2 - 2), sz * (d / 2 - 2)
            px, pz = x + lx * math.cos(a) + lz * math.sin(a), z - lx * math.sin(a) + lz * math.cos(a)
            ents.append(box((px, (top - 2.4) / 2, pz), (2, top - 2.4, 2), STEEL))

def papers(ents, x, y, z, n=5, spread=10):
    for k in range(n):
        ents.append(box((x + random.uniform(-spread, spread), y + 0.15 + 0.05 * k, z + random.uniform(-spread / 2, spread / 2)),
                        (8.5, 0.1, 11), PAPER, rot=(0, random.uniform(-40, 40), 0), cast_shadow=False))

def scene(ents, lights, env, start_pos, emitters=None, audio=None, footsteps=None, extra=None):
    g = {"ConfigFilePath": "res/configs/config.json", "gravity": -10.0, "PlayerStart": start_pos,
         "Environment": env, "Entities": ents, "Lights": lights, "ParticleEmitters": emitters or []}
    if audio: g["Audio"] = audio
    if footsteps: g["Footsteps"] = footsteps
    if extra: g.update(extra)
    return g

scenes = {}

# --------------------------------------------------------------------------
# 00 the archive video: the Kugelblitz ignites (live, slowly), old footage
# --------------------------------------------------------------------------
moon_yaw = yaw_towards((490, -60), (490 + LAGOON_MOON[0], -60 + LAGOON_MOON[2]))
moon_pitch = round(math.degrees(math.asin(LAGOON_MOON[1])) * 0.8, 1)
scenes['00_archive_ignition'] = outdoor(LAGOON, 0.0, LAGOON_MOON,
    [kugelblitz(LAGOON_MOON, 0.0, play=True, speed=0.7, delay=4.0)],
    start(490, 40, -60, moon_yaw, moon_pitch),
    {"filmGrain": 0.09, "chromaticAberration": 1.4, "lensDirt": 0.9, "saturation": 0.6, "cloudCoverage": 0.2})

# --------------------------------------------------------------------------
# 01 Room 7: the hospital room (Ch.1)
# --------------------------------------------------------------------------
ents, lights = [], []
W, D = 90.0, 110.0
room(ents, 0, 0, W, D, doors=[('s', 0, 24, 40)])
door(ents, 0, D / 2 + 2, locked=True, window=True)
ents.append(box((15, 24, D / 2 - 0.5), (4, 6, 1.2), DARK_PLASTIC))                      # the keypad (no power)
ents.append(box((15, 26, D / 2 - 1.3), (0.6, 0.6, 0.4), LED_RED, cast_shadow=False))
# the hospital bed (along the west wall), an IV stand
bx, bz = -W / 2 + 20, -18
ents.append(box((bx, 12, bz), (32, 4, 72), STEEL, True))
ents.append(box((bx, 16, bz), (30, 5, 68), MATTRESS))
ents.append(box((bx, 19.5, bz - 28), (18, 3, 10), WHITE))                                 # pillow
ents.append(box((bx, 22, bz - 36), (32, 22, 2), STEEL))                                   # headboard
for sx in (-1, 1):
    for sz in (-1, 1):
        ents.append(box((bx + sx * 14, 5, bz + sz * 33), (2, 10, 2), STEEL))
ents.append(box((bx + 22, 22, bz - 30), (1.2, 44, 1.2), STEEL))
ents.append(box((bx + 22, 40, bz - 30), (7, 9, 2), mat((0.8, 0.9, 1.0), extra={"roughness": 0.1})))
# the desk (east wall): CRT, keyboard, papers, mug, Polaroid, radio; the fuse box under it
dx, dz = W / 2 - 15, -12
desk(ents, dx, dz, w=26, d=58)
crt(ents, dx + 2, 26, dz, yaw=-90)
ents.append(box((dx - 5, 26.5, dz), (5, 1, 14), BEIGE, rot=(0, -90, 0)))                 # keyboard
papers(ents, dx - 3, 26, dz + 18, n=6, spread=5)
ents.append(box((dx - 6, 28, dz - 20), (3, 4, 3), mat((0.9, 0.9, 0.85))))                # coffee mug
ents.append(box((dx - 2, 26.2, dz + 8), (4, 0.2, 5), PAPER, rot=(0, 20, 0)))              # the Polaroid (March 14)
ents.append(box((dx, 29.5, dz - 26), (6, 7, 13), DARK_PLASTIC))                          # the dusty radio
ents.append(box((W / 2 - 2, 8, dz + 15), (3, 12, 10), mat((0.5, 0.5, 0.52))))             # fuse box
for k, c in enumerate(((1, 0.1, 0.1), (0.1, 0.3, 1), (1, 0.85, 0.1), (0.1, 0.8, 0.2))):
    ents.append(box((W / 2 - 4, 5 + k * 0.8, dz + 11 + k * 2.5), (1.5, 0.5, 0.5), mat(c), rot=(0, 0, 30 + k * 10)))
ents.append(box((W / 2 - 12, 5, dz + 30), (8, 10, 8), DARK_PLASTIC))                      # the bin...
ents.append(box((W / 2 - 12, 12, dz + 30), (5, 7, 2), mat((0.95, 0.75, 0.15))))           # ...and the cereal box
ents.append(box((0, H_ROOM - 0.5, -10), (6, 1, 6), mat((0.6, 0.6, 0.6))))                 # the ceiling speaker
ents.append(box((-20, 32, -D / 2 + 0.6), (14, 18, 0.5), PAPER))                           # the calendar (Dec 1999)
ents.append(box((-20, 39, -D / 2 + 0.9), (14, 4, 0.5), mat((0.8, 0.1, 0.1))))
tube(ents, lights, 0, -5, along='z', flicker=0.06)
scenes['01_room7'] = scene(ents, lights, interior_env(), start(-5, 30, -25, 180, -4))

# --------------------------------------------------------------------------
# 02 the corridor and Subject 4's door (Ch.2)
# --------------------------------------------------------------------------
ents, lights = [], []
CW, CL = 50.0, 600.0
cell_doors = [(side, z, 24, 40) for side in 'ew' for z in range(-240, 300, 90)]
room(ents, 0, 0, CW, CL, doors=cell_doors)
for (side, z, _, _) in cell_doors:
    x = CW / 2 + 2 if side == 'e' else -CW / 2 - 2
    lit = side == 'e' and z == -60                                                          # Subject 4
    door(ents, x, z, along='z', window=True, lit=lit)
    ents.append(box((x + (-1.5 if side == 'e' else 1.5), 44, z), (0.5, 3, 8), PAPER))      # cell number plates
# Subject 4's cell behind her window: a small lit room, a figure at the glass
room(ents, CW / 2 + 40, -60, 72, 60, walls=PEELING, doors=[('w', 0, 24, 40)])
ents.append(entity("res/models/FinalBaseMesh.obj", (CW / 2 + 16, 0, -60), 1.45, (0, -90, 0),
                   mat((0.85, 0.7, 0.6), extra={"roughness": 0.6})))
tube(ents, lights, CW / 2 + 45, -60, flicker=0.02, color=(1.0, 0.92, 0.75), radius=120)
for k, z in enumerate(range(-270, 300, 120)):
    tube(ents, lights, 0, z, along='x', flicker=0.6 if k == 2 else 0.05, radius=170)
ents.append(box((0, 46, -CL / 2 + 1), (16, 5, 1), EXIT_GREEN, cast_shadow=False))
lights.append(light((0, 42, -CL / 2 + 8), (0.2, 1.0, 0.35), "POINT", (110, 110, 110), cast_shadow=False,
                    diffuse_attenuation=[500, 10, 3], specular_attenuation=[500, 10, 3]))
scenes['02_corridor_subject4'] = scene(ents, lights, interior_env(), start(0, 30, 270, 0, 0))

# --------------------------------------------------------------------------
# 02 the twin levers: two rooms off a hall, a lever in each
# --------------------------------------------------------------------------
ents, lights = [], []
room(ents, 0, 0, 80, 60, doors=[('w', 0, 24, 40), ('e', 0, 24, 40), ('n', 0, 30, 44)])   # the hall
ents.append(box((0, 22, -32), (30, 44, 3), STEEL, True))                                  # the locked exit
ents.append(box((-10, 47, -31), (3, 3, 1), LED_RED, cast_shadow=False))
ents.append(box((10, 47, -31), (3, 3, 1), LED_RED, cast_shadow=False))
for side in (-1, 1):
    rx = side * 95
    room(ents, rx, 0, 110, 80, doors=[('e' if side < 0 else 'w', 0, 24, 40)])
    wall_x = rx + side * 55
    ents.append(box((wall_x - side * 2, 26, -10), (3, 16, 10), STEEL))                    # the lever's plate
    ents.append(box((wall_x - side * 6, 30, -10), (8, 2, 2), mat((0.85, 0.12, 0.08), extra={"roughness": 0.4}),
                    rot=(0, 0, side * 35)))                                               # the handle
    desk(ents, rx, 20, w=40, d=22)
    ents.append(box((rx - 6, 30, 18), (3, 8, 2), DARK_PLASTIC))                           # a walkie-talkie
    ents.append(box((rx - 6, 36, 18), (0.5, 6, 0.5), DARK_PLASTIC))
    tube(ents, lights, rx, 0, flicker=0.05)
tube(ents, lights, 0, 0, flicker=0.03)
scenes['02_twin_levers'] = scene(ents, lights, interior_env(), start(-110, 30, 25, -90, 0))

# --------------------------------------------------------------------------
# 02 the security office: the CCTV wall; the robot's hallway beyond the glass
# --------------------------------------------------------------------------
ents, lights = [], []
room(ents, 0, 0, 110, 80, doors=[('s', 30, 24, 40)])
ents.append(box((0, 2, -40), (100, 4, 10), STEEL))
for row in range(4):
    for col in range(8):
        x = -45 + col * 12.8
        y = 10 + row * 10.5
        ents.append(box((x, y, -38), (12, 10, 9), DARK_PLASTIC))
        tint = random.choice(((0.55, 0.8, 1.0), (0.6, 1.0, 0.7), (0.9, 0.9, 1.0), (0.4, 0.6, 0.9)))
        k = random.uniform(0.12, 0.4)
        ents.append(box((x, y, -33.4), (10, 8, 0.4), glow(tuple(c * k for c in tint)), cast_shadow=False))
lights.append(light((0, 30, -20), (0.45, 0.65, 0.9), "POINT", (150, 150, 150), cast_shadow=True, shadowMapSize=512,
                    diffuse_attenuation=[300, 6, 2], specular_attenuation=[300, 6, 2], flicker=0.08, flickerSpeed=6))
desk(ents, 0, -18, w=70, d=20)
ents.append(box((0, 27, -18), (14, 1, 5), BEIGE))
ents.append(box((0, 10, -2), (14, 2, 14), DARK_PLASTIC))                                  # the chair
ents.append(box((0, 18, 5), (14, 16, 2), DARK_PLASTIC))
# the hallway behind a glass wall (east), the cleaning robot in it
room(ents, 115, 0, 120, 60, doors=[('w', 0, 50, 46)])
ents.append(box((57, 23, 0), (1, 46, 50), mat((0.6, 0.7, 0.75), shininess=200, extra={"roughness": 0.03})))
ents.append(entity("res/models/capsule.obj", (125, 9, 5), (6, 7, 6), material=mat((0.9, 0.85, 0.3), extra={"roughness": 0.4})))
ents.append(box((125, 18, 5), (8, 4, 8), mat((0.9, 0.9, 0.88), extra={"roughness": 0.3})))
ents.append(box((125, 18.5, 9.2), (4, 1.2, 0.4), LED_RED, cast_shadow=False))
tube(ents, lights, 115, 0, flicker=0.3)
scenes['02_security_office'] = scene(ents, lights, interior_env(ambient=0.035), start(30, 30, 30, 20, -6))

# --------------------------------------------------------------------------
# 02 things that don't add up: the waiting room
# --------------------------------------------------------------------------
ents, lights = [], []
room(ents, 0, 0, 120, 90, walls=mat((0.55, 0.5, 0.4), extra={"roughness": 0.85}), doors=[('s', -40, 24, 40)])
# the "windows": screens of a sunny street, the same red car
for k, x in enumerate((-30, 25)):
    ents.append(box((x, 30, -45.2), (40, 26, 1), STEEL))
    ents.append(box((x, 35, -44.4), (36, 12, 0.4), glow((0.22, 0.34, 0.5)), cast_shadow=False))      # sky
    ents.append(box((x, 25, -44.4), (36, 8, 0.4), glow((0.2, 0.19, 0.18)), cast_shadow=False))        # street
    ents.append(box((x - 6 + k * 14, 26.5, -44.1), (8, 3.5, 0.4), glow((0.55, 0.04, 0.03)), cast_shadow=False))  # the red car
lights.append(light((0, 30, -30), (0.9, 0.95, 1.0), "SPOT", (200, 200, 140), rot=(0, 180, 0), cast_shadow=True,
                    coneAngle=0.7, diffuse_attenuation=[320, 4, 1], specular_attenuation=[320, 4, 1]))
# clocks at 11:59, never moving
for x, z, yaw in ((-59.4, 0, 90), (59.4, -10, -90), (0, 44.4, 180)):
    a = math.radians(yaw)
    ents.append(entity("res/models/sphere.obj", (x, 38, z), (5, 5, 0.6), (0, yaw, 0), WHITE))
    ents.append(box((x + math.sin(a) * 0.8, 40, z + math.cos(a) * 0.8), (0.5, 4, 0.3), DARK_PLASTIC, rot=(0, yaw, 0)))
    ents.append(box((x + math.sin(a) * 0.9, 39.2, z + math.cos(a) * 0.9), (0.5, 3, 0.3), DARK_PLASTIC, rot=(0, yaw, -6)))
# sofa, coffee table, the newspapers (all the same date), the radio
ents.append(box((-20, 6, 25), (50, 12, 16), mat((0.35, 0.22, 0.18), extra={"roughness": 0.9}), True))
ents.append(box((-20, 15, 32), (50, 14, 4), mat((0.35, 0.22, 0.18), extra={"roughness": 0.9})))
ents.append(box((-20, 8, 5), (30, 2, 16), WOOD, True))
for k in range(5):
    ents.append(box((-26 + k * 3, 9.2 + k * 0.15, 5 + random.uniform(-2, 2)), (10, 0.15, 14), PAPER,
                    rot=(0, random.uniform(-20, 20), 0), cast_shadow=False))
ents.append(box((-6, 11, 5), (8, 5, 4), DARK_PLASTIC))
ents.append(box((40, 20, 30), (16, 40, 10), WOOD, True))                                  # shelves ("painted dust")
for y in (12, 24, 36):
    ents.append(box((40, y, 30), (16, 1, 10), OFFWHITE))
tube(ents, lights, 0, 15, flicker=0.04)
scenes['02_waiting_room'] = scene(ents, lights, interior_env(), start(-40, 30, 40, 0, 0))

# --------------------------------------------------------------------------
# 02 the tapes: the records room, the shutter coming down
# --------------------------------------------------------------------------
ents, lights = [], []
room(ents, 0, 0, 130, 110, walls=PEELING, doors=[('n', 0, 30, 44)])
tapes = []
for k, x in enumerate((-45, -15, 15, 45)):
    ents.append(box((x, 21, -10), (12, 42, 70), mat((0.4, 0.42, 0.44), shininess=60, extra={"roughness": 0.5, "metallic": 0.4}), True))
    for y in (6, 16, 26, 36):
        for z in range(-32, 33, 3):
            if random.random() < 0.85:
                tapes.append({"position": [x + 4, y + 1.2, z], "rotation": [0, 0, 0], "scale": [2.2, 3.2, 1.2]})
                tapes.append({"position": [x - 4, y + 1.2, z], "rotation": [0, 0, 0], "scale": [2.2, 3.2, 1.2]})
ents.append({"Transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
             "Components": {"InstanceRenderComponent": {"Mesh": "res/models/cube4.obj", "Transforms": tapes,
                                                        "Material": mat((0.12, 0.12, 0.13), extra={"roughness": 0.5})}}})
desk(ents, 0, 40, w=40, d=20)
ents.append(box((0, 28, 40), (12, 4, 8), DARK_PLASTIC))                                   # the tape player
ents.append(box((4, 30.5, 40), (3, 0.6, 2), mat((0.15, 0.1, 0.08))))                      # RUN 30 in it
lights.append(light((-10, 34, 40), (1.0, 0.75, 0.45), "POINT", (110, 110, 110), cast_shadow=True, shadowMapSize=512,
                    diffuse_attenuation=[400, 8, 3], specular_attenuation=[400, 8, 3]))
ents.append(box((-10, 30, 40), (4, 4, 4), glow((1.0, 0.8, 0.5))))
# the shutter half way down over the door, a red warning light
ents.append(box((0, 32, -57), (30, 24, 3), RUSTY, True))
ents.append(box((0, 47, -54), (4, 4, 2), LED_RED, cast_shadow=False))
lights.append(light((0, 44, -50), (1.0, 0.1, 0.05), "POINT", (120, 120, 120), cast_shadow=False,
                    diffuse_attenuation=[500, 10, 4], specular_attenuation=[500, 10, 4], flicker=0.9, flickerSpeed=4))
tube(ents, lights, 0, -10, flicker=0.05, radius=150)
scenes['02_records_room'] = scene(ents, lights, interior_env(ambient=0.04), start(0, 30, 48, 0, -5))

# --------------------------------------------------------------------------
# 03 the stairwell: the EXIT door, the stairs going up and up
# --------------------------------------------------------------------------
ents, lights = [], []
SH, SW, FLIGHTS, RISE = 70.0, 70.0, 10, 34.0
room(ents, 0, 0, SW, SW, h=FLIGHTS * RISE + 40, walls=CONCRETE, floor=CONCRETE, ceiling=CONCRETE)
for f in range(FLIGHTS):
    y0 = f * RISE
    side = f % 2
    x = -18 if side == 0 else 18
    # ten steps up along z, alternating direction; a landing at each end
    for s in range(10):
        z = (-25 + s * 5) if side == 0 else (25 - s * 5)
        ents.append(box((x, y0 + (s + 0.5) * RISE / 10, z), (30, RISE / 10, 5), CONCRETE))
    # the walkable ramp under them (invisible), and the landing
    length = math.hypot(50, RISE)
    pitch = math.degrees(math.atan2(RISE, 50)) * (1 if side == 0 else -1)
    ents.append({"Transform": {"position": [x, y0 + RISE / 2 - 1, 0], "rotation": [pitch, 0, 0],
                               "scale": [30, 2, length]},
                 "Components": {"PhysicsComponent": box_physics((30, 2, length))}})
    zl = 31 if side == 0 else -31
    ents.append(box((0, y0 + RISE - 1, zl), (SW, 2, 8), CONCRETE, True))
    lights.append(light((0, y0 + RISE + 22, zl), (1.0, 0.72, 0.42), "POINT", (90, 90, 90), cast_shadow=False,
                        diffuse_attenuation=[300, 8, 3], specular_attenuation=[300, 8, 3],
                        flicker=0.5 if f % 3 == 1 else 0.08, flickerSpeed=9))
    ents.append(entity("res/models/sphere.obj", (0, y0 + RISE + 22, zl), 1.4, material=glow((1.0, 0.75, 0.45)),
                       cast_shadow=False))
ents.append(box((0, 46, SW / 2 - 1), (12, 3.5, 1), EXIT_GREEN, cast_shadow=False))
ents.append(box((0, 20, SW / 2 + 1), (24, 40, 2.5), STEEL, True))                          # the steel door behind you
lights.append(light((0, 44, SW / 2 - 6), (0.2, 1.0, 0.35), "POINT", (80, 80, 80), cast_shadow=False,
                    diffuse_attenuation=[900, 14, 5], specular_attenuation=[900, 14, 5]))
ents.append(box((0, FLIGHTS * RISE + 39, 31), (20, 2, 20), mat((0.25, 0.3, 0.45), emissive=0.4)))   # the hatch, night beyond
scenes['03_stairwell'] = scene(ents, lights, interior_env(ambient=0.03), start(0, 30, 24, 0, 38))

# --------------------------------------------------------------------------
# 03 out of the hatch: the canyon at night, the Moon cracked open
# --------------------------------------------------------------------------
hx, hz = CANYON_SPAWN
hy = canyon_ground(hx, hz)
g = outdoor(CANYON, 0.0, CANYON_MOON, [kugelblitz(CANYON_MOON, 0.45)],
            start(hx + 15, hy + 28, hz + 25, yaw_towards((hx, hz), (hx + CANYON_MOON[0], hz + CANYON_MOON[2])), 22))
g["Entities"] += [box((hx, hy + 2, hz), (22, 5, 22), RUSTY, True),
                  box((hx, hy + 5, hz), (16, 1, 16), mat((0.02, 0.02, 0.02))),
                  box((hx - 10, hy + 14, hz), (2, 18, 16), RUSTY, rot=(0, 0, -20)),       # the lid, thrown open
                  box((hx + 8, hy + 7, hz - 9), (4, 4, 2), DARK_PLASTIC)]                 # the speaker
scenes['03_canyon_hatch'] = g

# --------------------------------------------------------------------------
# 04 clue 1: the monument (the plaza at night, the jets)
# --------------------------------------------------------------------------
g = outdoor(CANYON, 0.0, CANYON_MOON, [kugelblitz(CANYON_MOON, 0.66)], start(0, 27.5, -625, 0, 12))
g["Entities"] += [box((0, 14, -995.5), (22, 9, 1), BRONZE),                                # the plaque, worn smooth
                  box((5, 12.5, -994.6), (8.5, 11, 0.15), PAPER, rot=(0, 0, 8), cast_shadow=False)]  # the charcoal rubbing
scenes['04_monument'] = g

# --------------------------------------------------------------------------
# 04 clue 2: the survivors' camp by the lagoon, the journal pages
# --------------------------------------------------------------------------
g = outdoor(LAGOON, 0.02, LAGOON_MOON, [kugelblitz(LAGOON_MOON, 0.66)],
            start(CAMPFIRE[0] - 60, CAMPFIRE[1] + 30, CAMPFIRE[2] + 60,
                  yaw_towards((CAMPFIRE[0] - 60, CAMPFIRE[2] + 60), (CAMPFIRE[0], CAMPFIRE[2])), -14))
for k in range(6):
    a = random.uniform(0, 2 * math.pi); r = random.uniform(22, 55)
    x, z = CAMPFIRE[0] + math.cos(a) * r, CAMPFIRE[2] + math.sin(a) * r
    g["Entities"].append(box((x, lagoon_ground(x, z) + 0.6, z), (8.5, 0.15, 11), PAPER,
                             rot=(random.uniform(-6, 6), random.uniform(0, 360), random.uniform(-6, 6)), cast_shadow=False))
x, z = CAMPFIRE[0] + 30, CAMPFIRE[2] - 20
g["Entities"].append(box((x, lagoon_ground(x, z) + 3, z), (12, 6, 9), mat((0.3, 0.25, 0.2), extra={"roughness": 0.9})))  # a pack
scenes['04_survivor_camp'] = g

# --------------------------------------------------------------------------
# 04 clue 3 / 4: the kiosk in the plaza (noon: the mirrors; night: telemetry)
# --------------------------------------------------------------------------
KX, KZ = -55.0, -640.0
def kiosk(on):
    e = [box((KX, CANYON_TOP + 16, KZ), (14, 32, 10), STEEL, True),
         box((KX + 7.2, CANYON_TOP + 22, KZ), (0.4, 10, 8), SCREEN_GREEN if on else GLASS_DARK, cast_shadow=False),
         box((KX, CANYON_TOP + 33, KZ), (20, 1, 14), mat((0.1, 0.12, 0.25), shininess=120, extra={"roughness": 0.15}),
             rot=(20, 0, 0)),                                                                # its solar panel
         box((KX + 7.4, CANYON_TOP + 12, KZ), (0.4, 4, 6), DARK_PLASTIC)]                     # buttons
    return e
g = outdoor(CANYON, 0.5, CANYON_MOON, [kugelblitz(CANYON_MOON, 0.66)], start(-20, 27.5, -600, 30, -8))
g["Entities"] += kiosk(False)
for (x, z, yaw) in ((-38, -700, 60), (38, -760, -60), (-38, -840, 120), (38, -900, -120)):
    g["Entities"] += [box((x, CANYON_TOP + 10, z), (1.5, 20, 1.5), STEEL),
                      box((x, CANYON_TOP + 22, z), (14, 12, 0.6), MIRROR, rot=(15, yaw, 0))]
scenes['04_archive_kiosk'] = g

g = outdoor(CANYON, 0.0, CANYON_MOON, [kugelblitz(CANYON_MOON, 0.7)], start(-20, 27.5, -600, 30, 4))
g["Entities"] += kiosk(True)
g["Entities"].append(box((KX + 7.5, CANYON_TOP + 15, KZ), (0.2, 12, 8), PAPER, rot=(0, 0, -8)))   # printing
for k in range(9):
    x, z = KX + random.uniform(6, 30), KZ + random.uniform(-14, 14)
    g["Entities"].append(box((x, CANYON_TOP + 0.2, z), (8.5, 0.12, 11), PAPER,
                             rot=(0, random.uniform(0, 360), 0), cast_shadow=False))
g["Lights"].append(light((KX + 14, CANYON_TOP + 22, KZ), (0.3, 1.0, 0.45), "POINT", (90, 90, 90), cast_shadow=False,
                         diffuse_attenuation=[400, 8, 3], specular_attenuation=[400, 8, 3], flicker=0.15, flickerSpeed=5))
scenes['04_telemetry'] = g

# --------------------------------------------------------------------------
# 05 the lighthouse hill: its beam on the Moon in its death spiral
# --------------------------------------------------------------------------
g = outdoor(LAGOON, 0.0, LAGOON_MOON, [kugelblitz(LAGOON_MOON, 0.88)], start(490, 40, -60, 0, 8))
far = [LIGHTHOUSE[i] + LAGOON_MOON[i] * 1000 for i in range(3)]
for l in g["Lights"]:
    p = l["Transform"]["position"]
    if l["Attributes"]["type"] == "SPOT" and p[1] > 100:
        l["Transform"]["rotation"] = list(aim(LIGHTHOUSE, far))                            # straight up at the Moon
scenes['05_lighthouse_hill'] = g

# --------------------------------------------------------------------------
# 05 inside the lighthouse: walls of CRT screens, the console
# --------------------------------------------------------------------------
ents, lights = [], []
R = 70.0
ents.append(box((0, -2, 0), (2 * R + 20, 4, 2 * R + 20), CONCRETE, True))
ents.append(box((0, 82, 0), (2 * R + 20, 4, 2 * R + 20), CONCRETE, True))
for k in range(8):
    a = k * math.pi / 4
    x, z = math.sin(a) * R, math.cos(a) * R
    yaw = math.degrees(a)
    ents.append(box((x, 40, z), (58, 80, 4), CONCRETE, True, rot=(0, yaw, 0)))
    for row in range(5):
        for col in range(4):
            off = (col - 1.5) * 13
            px = x - math.sin(a) * 4 + math.cos(a) * off
            pz = z - math.cos(a) * 4 - math.sin(a) * off
            k = random.uniform(0.12, 0.3)
            crt(ents, px, 4 + row * 14, pz, yaw=yaw + 180, screen=glow((0.45 * k, 0.8 * k, 1.0 * k)) if random.random() < 0.9 else GLASS_DARK)
lights.append(light((0, 40, 0), (0.4, 0.7, 1.0), "POINT", (220, 220, 220), cast_shadow=True, shadowMapSize=512,
                    diffuse_attenuation=[700, 8, 2], specular_attenuation=[700, 8, 2], flicker=0.06, flickerSpeed=12))
ents.append(box((0, 12, 0), (40, 24, 18), STEEL, True))                                   # the console
ents.append(box((0, 24.5, 4), (30, 1, 8), DARK_PLASTIC, rot=(-20, 0, 0)))
for side in (-1, 1):
    ents.append(box((side * 15, 28, -5), (1.5, 9, 1.5), mat((0.85, 0.12, 0.08)), rot=(15, 0, 0)))   # the twin levers
ents.append(box((0, 80, 0), (12, 2, 12), mat((1.0, 0.95, 0.85), emissive=1.0)))            # the beam's shaft above
scenes['05_lighthouse_interior'] = scene(ents, lights, interior_env(ambient=0.03, bloomStrength=0.6, emissiveStrength=1.0),
                                          start(0, 30, 45, 0, 4))

# --------------------------------------------------------------------------
# 06 the ending: sunrise on the hill, no Moon, the ring in the sky
# --------------------------------------------------------------------------
g = outdoor(LAGOON, 0.22, LAGOON_MOON, [], start(LIGHTHOUSE[0] + 30, LIGHTHOUSE[1] + 10, LIGHTHOUSE[2] + 90, 175, 34),
            # (the ring: the engine's Milky Way band, laid across the sky where the Moon was)
            {"moon": False, "milkyWay": 3.5, "milkyWayNormal": [0.0, 0.5, -0.866], "milkyWayCenter": [1.0, 0.0, 0.0]})
scenes['06_ending'] = g

# --------------------------------------------------------------------------
os.makedirs(OUT, exist_ok=True)
for name, game in scenes.items():
    path = os.path.join(OUT, name + '.json')
    json.dump(game, open(path, 'w'), indent=1)
    print('%-28s entities %3d lights %2d' % (path, len(game["Entities"]), len(game["Lights"])))
