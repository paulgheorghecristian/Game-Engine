"""RECURSION: The Year 2005 -- a level per place in the story, built from
what the engine already has (the canyon and lagoon maps, the engine's sky
black hole and its Moon-destruction sequence, boxes, the old models and
textures). Each is a map in Game/recursion/:

  00_archive_ignition     the archive's 2038 footage: the Moon eaten by the black
                          hole AURA made (plays live, slowly), grainy like old footage
  00_archive_ignition_alternative   the same, the black hole as it would really be: the
                          Moon's mass, too small to see; its disk and a point of light
  01_room7                Ch.1 (and the ending): the hospital-like room, bed, CRT, fuse
                          box, keypad door
  02_corridor_subject4    Ch.2: peeling corridor of cells, cell 4's window
  02_twin_levers          Ch.2: two rooms, a lever in each
  02_security_office      Ch.2: wall of CCTV monitors; the cleaning robot's hall
  02_waiting_room         Ch.2 "things that don't add up": fake windows (a
                          night street, a full Moon, the same red car),
                          clocks that never move, the radio, the newspapers
  02_records_room         Ch.2 the tapes: shelves of cassettes, the shutter
  03_stairwell            Ch.3: the EXIT door and the endless stairs up
  03_canyon               Ch.3 and 4: out of the hatch at night, the torch path
                          to the plaza: the AURA monument and its plaque, the
                          dark kiosk, the mirrors round the pool, the concrete hut
                          with its door left open (the way down)
  04_tunnels              Ch.4 to 5, the way down: AURA's tunnels under the
                          canyon, windows into the shelters (people asleep in
                          bunks) and the datacenter, a ladder up to the hill
  05_lagoon               Ch.5: out of the hatch on the hill, your camp from
                          earlier runs (the journal) by the water, the hill where
                          the voice in your head speaks
  06_dawn                 the ending: the last dawn on the hill, the black hole
                          filling the sky; then white, and Room 7 again

The timeline: the archive footage (00) is 2038, AURA's Moon plant making a
black hole that eats the Moon. Everything else is the year 4000, as the black
hole's stretched orbit brings it in for its closest pass yet: no Moon, only
the black hole with the Moon's dust in a ring around it, closer and bigger
each night (HOLE_CANYON, HOLE_LAGOON), until at the last dawn it fills the
sky. The story is in
Game/recursion/STORY.md.

Run after make_canyon.py / make_lagoon.py (it starts from their maps):
  python3 tools/make_recursion.py
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
BEIGE = mat((0.78, 0.72, 0.6), shininess=40, extra={"roughness": 0.5})                    # 2005 computer plastic
DARK_PLASTIC = mat((0.08, 0.08, 0.09), shininess=60, extra={"roughness": 0.4})
STEEL = mat((0.55, 0.56, 0.6), "res/textures/metal.jpg", "res/textures/metal_normal.jpg", extra={"metallic": 0.6, "roughness": 0.45})
RUSTY = mat((0.75, 0.7, 0.65), "res/textures/158.JPG", "res/textures/158_norm.JPG", extra={"roughness": 0.65, "metallic": 0.3})
WOOD = mat((0.42, 0.28, 0.17), extra={"roughness": 0.6})
DOOR_PAINT = mat((0.3, 0.34, 0.33), shininess=60, extra={"roughness": 0.45, "metallic": 0.35})   # painted steel
MATTRESS = mat((0.85, 0.88, 0.9), extra={"roughness": 0.95})
BRONZE = mat((0.62, 0.5, 0.38), shininess=60, extra={"metallic": 0.35, "roughness": 0.5})
MIRROR = mat((0.9, 0.92, 0.95), shininess=200, extra={"metallic": 1.0, "roughness": 0.04})
GLASS_DARK = mat((0.02, 0.03, 0.03), shininess=150, extra={"roughness": 0.05, "specularStrength": 1.0})
GLASS = mat((0.82, 0.9, 0.88), extra={"glass": True, "roughness": 0.0})                  # see-through (glass.fs)
GLASS_WIRED = mat((0.7, 0.8, 0.78), extra={"glass": True, "roughness": 0.25})            # old wired glass, a little frosted
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
    """heights on a terrain mesh (any triangles, as placed by its map entity):
    the triangle under (x, z), interpolated; the highest one where they overlap"""
    def __init__(self, obj, position=(0, 0, 0), scale=(1, 1, 1), cell=60.0):
        verts, tris = [], []
        for line in open(obj):
            if line.startswith('v '):
                x, y, z = map(float, line.split()[1:4])
                verts.append((x * scale[0] + position[0], y * scale[1] + position[1], z * scale[2] + position[2]))
            elif line.startswith('f '):
                idx = [int(t.split('/')[0]) - 1 for t in line.split()[1:]]
                for k in range(1, len(idx) - 1):
                    tris.append((idx[0], idx[k], idx[k + 1]))
        self.verts, self.cell, self.grid = verts, cell, {}
        for t in tris:
            xs = [verts[i][0] for i in t]; zs = [verts[i][2] for i in t]
            for gx in range(int(math.floor(min(xs) / cell)), int(math.floor(max(xs) / cell)) + 1):
                for gz in range(int(math.floor(min(zs) / cell)), int(math.floor(max(zs) / cell)) + 1):
                    self.grid.setdefault((gx, gz), []).append(t)
    def __call__(self, x, z):
        best = None
        for t in self.grid.get((int(math.floor(x / self.cell)), int(math.floor(z / self.cell))), ()):
            (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = (self.verts[i] for i in t)
            d = (z1 - z2) * (x0 - x2) + (x2 - x1) * (z0 - z2)
            if abs(d) < 1e-9: continue
            a = ((z1 - z2) * (x - x2) + (x2 - x1) * (z - z2)) / d
            b = ((z2 - z0) * (x - x2) + (x0 - x2) * (z - z2)) / d
            c = 1 - a - b
            if a >= -1e-6 and b >= -1e-6 and c >= -1e-6:
                y = a * y0 + b * y1 + c * y2
                best = y if best is None else max(best, y)
        return best if best is not None else 0.0

def load(path):
    return json.load(open(path))

CANYON = load('Game/canyon.json')
LAGOON = load('Game/lagoon.json')
canyon_ground = Ground('res/models/canyon_terrain.obj')
_lt = LAGOON['Entities'][0]['Transform']
lagoon_ground = Ground(LAGOON['Entities'][0]['Components']['RenderComponent']['Mesh'], _lt['position'], _lt['scale'])
# the Moon hangs up the valley (seen from the hatch, and over the statue
# from the plaza); the canyon's own moon sits in the east behind a cliff
_m = (0.32, 0.6, -0.73)
CANYON_MOON = [round(c / math.sqrt(sum(v * v for v in _m)), 4) for c in _m]
LAGOON_MOON = LAGOON['Environment']['moonDirection']
CANYON_TOP = 1.5                                    # the plaza paving
PED, POOL_Z0, POOL_Z1 = (0.0, -1022.0), -940.0, -660.0
CANYON_SPAWN = (40.0, 820.0)
CAMPFIRE = (721.0, 26.0, -360.0)

def moon_destruction(direction, collapse, play=False, speed=5.0, delay=0.0):
    """the 2038 footage: AURA's Moon plant making its black hole (a flash on the
    surface, a glowing hole as the black hole sinks into the rock, the cracks, the
    two jets, the Moon pulled into a dark point), held at a stage or playing. It is the engine's Moon-destruction sequence, whose JSON key is
    'kugelblitz'."""
    h = {"sky": True, "physical": True, "direction": direction, "angularRadius": 0.03, "kugelblitz": True,
         "collapse": collapse, "collapseSpeed": speed, "collapseDelay": delay, "collapseSite": [0.36, 0.77],
         "lensing": 0.8, "influence": 11, "colorShift": 0.7, "diskInner": 3, "diskOuter": 16,
         "diskColor": [1.0, 0.55, 0.26], "diskBrightness": 5.0, "diskOpacity": 0.55, "diskSpeed": 0.8,
         "diskTilt": [41.54, -21.34], "glow": 0, "moonStream": 0, "cloudClear": 0.7}
    if play: h["autoPlay"] = True
    return h

def sky_hole(direction, radius):
    """the black hole the Moon became (as in lagoon_bh.json), the Moon's dust in a
    ring around it; the closer its pass to the Earth, the bigger (radius: angular)"""
    return {"sky": True, "physical": True, "direction": direction, "angularRadius": radius, "lensing": 0.8, "influence": 11,
            "colorShift": 0.7, "diskInner": 3.0, "diskOuter": 16.0, "diskColor": [1.0, 0.55, 0.26],
            "diskBrightness": 5.0, "diskOpacity": 0.75, "diskSpeed": 0.8, "diskTilt": [57.01, -9.54],
            "glow": 0.0, "moonStream": 0.0, "cloudClear": 0.7}

def year4000(base, time_of_day, direction, radius, start, extra_env=None):
    """a year-4000 scene: no Moon (eaten in 2038), the black hole in the sky"""
    env = {"moon": False}
    if extra_env: env.update(extra_env)
    return outdoor(base, time_of_day, direction, [sky_hole(direction, radius)], start, env)

# the black hole's close pass is coming: closer, so bigger, by the last night
HOLE_CANYON, HOLE_LAGOON = 0.025, 0.042

def outdoor(base, time_of_day, moon_direction, holes, start, extra_env=None):
    """a story scene on an outdoor map: its time held, its Moon at a stage"""
    g = copy.deepcopy(base)
    g.pop("Sequences", None)               # (the base map's timelines are its own, not the story's)
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
# The lab is built in metres, at the player's scale: the eye is 41 units
# above the floor (ride height 31 + neck 10), a person's ~1.65 m, so one
# metre is ~24.8 units (the first version used 32 units for a whole person,
# and its doors came out a hobbit's 1.6 m tall)
M = 24.8
H_ROOM = 2.9 * M                                    # ceilings
DW, DH = 0.95 * M, 2.1 * M                          # doorways

def interior_env(**over):
    e = {"sky": "night", "zenithColor": [0, 0, 0], "horizonColor": [0, 0, 0], "starIntensity": 0.0,
         "moon": False, "moonColor": [0, 0, 0], "fogDensity": 0.0, "fogEnd": 0, "cloudCoverage": 0.0,
         # horror: dim, a stale haze the tubes glow in, washed-out colour,
         # a heavy vignette and grain
         "ambient": 0.03, "ambientSkyTint": [0.85, 0.95, 0.9], "ambientGroundTint": [0.5, 0.52, 0.48],
         "fogColor": [0.012, 0.016, 0.014], "fogDensity": 0.004, "fogHeightFalloff": 0.0, "fogBaseHeight": -100,
         "fogLightGlow": 1.4,
         "emissiveStrength": 1.3, "pbr": True, "hdr": True, "exposure": 1.0, "bloomStrength": 0.5,
         "bloomThreshold": 0.85, "aoStrength": 1.2, "aoRadius": 16, "saturation": 0.68, "grade": 1.0,
         "dayLength": 0, "lensDirt": 0.55, "chromaticAberration": 0.6, "filmGrain": 0.06}
    e.update(over)
    return e

def room(ents, cx, cz, w, d, h=H_ROOM, walls=PEELING, floor=TERRAZZO, ceiling=OFFWHITE, doors=(), band=True):
    """floor, ceiling and four walls (solid) of a w x d room at (cx, cz); doors:
    (side 'n'|'s'|'e'|'w', offset along the wall from its centre[, width, height])"""
    t = 0.15 * M
    ents.append(box((cx, -t / 2, cz), (w + 2 * t, t, d + 2 * t), floor, True))
    ents.append(box((cx, h + t / 2, cz), (w + 2 * t, t, d + 2 * t), ceiling, True))
    doors = [(dd + (DW, DH))[:4] if len(dd) == 2 else dd for dd in doors]
    for side in 'nsew':
        along = w if side in 'ns' else d
        gaps = sorted([(o - dw / 2, o + dw / 2, dh) for (s_, o, dw, dh) in doors if s_ == side])
        segments, a_ = [], -along / 2
        for (g0, g1, dh) in gaps:
            segments.append((a_, g0, 0.0, h)); segments.append((g0, g1, dh, h)); a_ = g1
        segments.append((a_, along / 2, 0.0, h))
        for (s0, s1, y0, y1) in segments:
            if s1 - s0 < 0.5 or y1 - y0 < 0.5: continue
            mid, length = (s0 + s1) / 2, s1 - s0
            if side == 'n': pos, size = (cx + mid, (y0 + y1) / 2, cz - d / 2 - t / 2), (length, y1 - y0, t)
            elif side == 's': pos, size = (cx + mid, (y0 + y1) / 2, cz + d / 2 + t / 2), (length, y1 - y0, t)
            elif side == 'w': pos, size = (cx - w / 2 - t / 2, (y0 + y1) / 2, cz + mid), (t, y1 - y0, length)
            else: pos, size = (cx + w / 2 + t / 2, (y0 + y1) / 2, cz + mid), (t, y1 - y0, length)
            ents.append(box(pos, size, walls, True))
            if band and y0 < 1.0 and walls is not CONCRETE:
                bh = 0.9 * M                                    # a darker band to waist height
                if side in 'ns':
                    z = pos[2] + (t / 2 + 0.2) * (1 if side == 'n' else -1)
                    ents.append(box((pos[0], bh / 2, z), (length, bh, 0.4), WAINSCOT, cast_shadow=False))
                else:
                    x = pos[0] + (t / 2 + 0.2) * (1 if side == 'w' else -1)
                    ents.append(box((x, bh / 2, pos[2]), (0.4, bh, length), WAINSCOT, cast_shadow=False))

HUMS = []                                           # the tubes' buzz (each scene's Audio sources)

def tube(ents, lights, x, z, h=H_ROOM, along='x', flicker=0.04, color=(0.85, 0.95, 1.0), radius=4.8 * M, shadow=True,
         dead=False):
    """a buzzing 1.2 m fluorescent tube on the ceiling, and its light (dead: burnt out, dark)"""
    size = (1.2 * M, 0.06 * M, 0.12 * M) if along == 'x' else (0.12 * M, 0.06 * M, 1.2 * M)
    ents.append(box((x, h - 0.05 * M, z), size, mat((0.25, 0.25, 0.22)) if dead else TUBE, cast_shadow=False))
    if dead:
        ents.append(box((x, h - 0.015 * M, z), (size[0] + 0.15 * M, 0.03 * M, size[2] + 0.18 * M), STEEL, cast_shadow=False))
        return
    HUMS.append({"sound": "hum_loop", "position": [round(x, 1), round(h - 0.2 * M, 1), round(z, 1)],
                 "volume": 0.35 + 0.4 * min(flicker * 2, 1.0), "radius": round(6 * M)})
    ents.append(box((x, h - 0.015 * M, z), (size[0] + 0.15 * M, 0.03 * M, size[2] + 0.18 * M), STEEL, cast_shadow=False))
    lights.append(light((x, h - 0.15 * M, z), color, "POINT", (radius, radius, radius), cast_shadow=shadow,
                        shadowMapSize=512, diffuse_attenuation=[420, 6, 1.3], specular_attenuation=[420, 6, 1.3],
                        flicker=min(flicker, 0.35), flickerSpeed=7))  # a slow waver, never a strobe

def door(ents, x, z, along='x', locked=True, window=True, lit=False, open_window=False):
    """a closed steel door filling a doorway (along: the wall's direction), a small window in it.
    open_window: a tall observation window, a real opening (the engine has no clear glass), low
    enough to see down onto a bed in the room"""
    size = (DW, DH, 0.06 * M) if along == 'x' else (0.06 * M, DH, DW)
    if open_window:
        ww, wh, wy = 0.3 * M, 0.7 * M, 1.35 * M
        def part(c_along, c_y, s_along, s_y):
            if along == 'x':
                ents.append(box((x + c_along, c_y, z), (s_along, s_y, size[2]), DOOR_PAINT, True))
            else:
                ents.append(box((x, c_y, z + c_along), (size[0], s_y, s_along), DOOR_PAINT, True))
        side = (DW - ww) / 2
        part(-(ww + side) / 2, DH / 2, side, DH)                                  # left of the window
        part((ww + side) / 2, DH / 2, side, DH)                                   # right of it
        part(0, (wy - wh / 2) / 2, ww, wy - wh / 2)                               # below
        part(0, (wy + wh / 2 + DH) / 2, ww, DH - wy - wh / 2)                     # above
        gsize = (ww, wh, 0.01 * M) if along == 'x' else (0.01 * M, wh, ww)
        ents.append(box((x, wy, z), gsize, GLASS_WIRED, True))                    # the pane
        return
    ents.append(box((x, DH / 2, z), size, DOOR_PAINT, True))
    ents.append(box((x + (0.35 * M if along == 'x' else 0), 1.0 * M, z + (0 if along == 'x' else 0.35 * M)),
                    (0.04 * M, 0.04 * M, 0.08 * M) if along == 'x' else (0.08 * M, 0.04 * M, 0.04 * M), STEEL))   # handle
    if window:
        wsize = (0.25 * M, 0.35 * M, 0.08 * M) if along == 'x' else (0.08 * M, 0.35 * M, 0.25 * M)
        ents.append(box((x, 1.6 * M, z), wsize, glow((0.95, 0.85, 0.6)) if lit else GLASS_DARK, cast_shadow=False))

def crt(ents, x, y, z, yaw=0, on=False, screen=None):
    """a beige 15-inch CRT on a surface at height y, facing yaw (0: +z)"""
    a = math.radians(yaw); fx, fz = math.sin(a), math.cos(a)
    ents.append(box((x - fx * 0.05 * M, y + 0.19 * M, z - fz * 0.05 * M), (0.38 * M, 0.36 * M, 0.4 * M), BEIGE, rot=(0, yaw, 0)))
    ents.append(box((x + fx * 0.155 * M, y + 0.2 * M, z + fz * 0.155 * M), (0.28 * M, 0.22 * M, 0.02 * M),
                    screen or (SCREEN_GREEN if on else GLASS_DARK), rot=(0, yaw, 0), cast_shadow=False))

def desk(ents, x, z, w=1.4 * M, d=0.7 * M, yaw=0, top=0.75 * M):
    a = math.radians(yaw)
    ents.append(box((x, top - 0.015 * M, z), (w, 0.03 * M, d), WOOD, True, rot=(0, yaw, 0)))
    for sx in (-1, 1):
        for sz in (-1, 1):
            lx, lz = sx * (w / 2 - 0.05 * M), sz * (d / 2 - 0.05 * M)
            px, pz = x + lx * math.cos(a) + lz * math.sin(a), z - lx * math.sin(a) + lz * math.cos(a)
            ents.append(box((px, (top - 0.03 * M) / 2, pz), (0.04 * M, top - 0.03 * M, 0.04 * M), STEEL))

def papers(ents, x, y, z, n=5, spread=0.25 * M):
    for k in range(n):
        ents.append(box((x + random.uniform(-spread, spread), y + 0.15 + 0.05 * k, z + random.uniform(-spread / 2, spread / 2)),
                        (0.21 * M, 0.1, 0.3 * M), PAPER, rot=(0, random.uniform(-40, 40), 0), cast_shadow=False))

def chair(ents, x, z, yaw=0):
    a = math.radians(yaw)
    ents.append(box((x, 0.45 * M, z), (0.45 * M, 0.04 * M, 0.45 * M), DARK_PLASTIC, rot=(0, yaw, 0)))
    ents.append(box((x - math.sin(a) * 0.22 * M, 0.7 * M, z - math.cos(a) * 0.22 * M), (0.45 * M, 0.5 * M, 0.04 * M),
                    DARK_PLASTIC, rot=(0, yaw, 0)))
    ents.append(box((x, 0.22 * M, z), (0.05 * M, 0.44 * M, 0.05 * M), STEEL))

def scene(ents, lights, env, start_pos, emitters=None, audio=None, footsteps=None, extra=None):
    g = {"ConfigFilePath": "res/configs/config.json", "gravity": -10.0, "PlayerStart": start_pos,
         "Environment": env, "Entities": ents, "Lights": lights, "ParticleEmitters": emitters or []}
    audio = list(audio or []) + HUMS                  # the map's own sounds (Audio/SoundScape.h)
    HUMS.clear()
    if audio: g["Audio"] = audio
    if footsteps: g["Footsteps"] = footsteps
    if extra: g.update(extra)
    return g

EYE = 31.0                                          # the PlayerStart height above the floor (its ride height)
scenes = {}

# --------------------------------------------------------------------------
# 00 the archive's 2038 footage: the Moon eaten by AURA's black hole (live), old footage
# --------------------------------------------------------------------------
moon_yaw = yaw_towards((490, -60), (490 + LAGOON_MOON[0], -60 + LAGOON_MOON[2]))
moon_pitch = round(math.degrees(math.asin(LAGOON_MOON[1])) * 0.8, 1)
def serious_lights(g):
    """the lagoon's lights for the 2038 footage: no show colours (the island's
    pink, cyan, green and purple, its sparkles), ordinary warm-white
    floodlights and dim sodium lamps, smaller halos"""
    def near(pos, x, z, r):
        return math.hypot(pos[0] - x, pos[2] - z) < r
    island_points = 0
    keep = []
    for l in g["Lights"]:
        a, pos = l["Attributes"], l["Transform"]["position"]
        c = a.get("color")
        if a["type"] == "POINT" and c == [1.0, 0.62, 0.28]:
            a["color"] = [0.5, 0.33, 0.16]                                          # the lamps: dim sodium
        elif near(pos, 500, -775, 60) or near(pos, 246, -801, 30):
            if a["type"] == "POINT":
                island_points += 1
                if island_points > 1: continue                                      # one light, not three
            a["color"] = [0.42, 0.38, 0.32]                                         # floodlights, warm white
        elif a["type"] == "SPOT" and near(pos, 515.4, -1214.7, 20):
            a["color"] = [round(v * 0.7, 3) for v in c]                             # the lighthouse, dimmer
        keep.append(l)
    g["Lights"] = keep
    emitters = []
    for p in g.get("ParticleEmitters", []):
        spots = p.get("positions") or [p.get("position") or [0, 0, 0]]
        if p.get("type") in ("drift", "spray") and any(near(q, 500, -720, 120) for q in spots) and \
           "smoke" not in p.get("texture", ""):
            continue                                                                # the island's sparkles
        if p.get("type") == "glow":
            if spots[0][0] in (452.0, 528.0):
                p["startScale"], p["tint"] = 32, [0.7, 0.45, 0.22]
            elif near(spots[0], 500, -793, 10):
                p["startScale"], p["tint"] = 40, [0.6, 0.55, 0.45]
            elif near(spots[0], 515.4, -1214.7, 10):
                p["tint"] = [round(v * 0.7, 3) for v in p["tint"]]
        if p.get("type") == "drift" and "smoke" in p.get("texture", "") and near(spots[0], 500, -775, 10):
            p["tint"] = [0.4, 0.4, 0.42]                                            # the mist, grey
        emitters.append(p)
    g["ParticleEmitters"] = emitters
    return g

scenes['00_archive_ignition'] = serious_lights(outdoor(LAGOON, 0.0, LAGOON_MOON,
    [dict(moon_destruction(LAGOON_MOON, 0.0, play=True, speed=5.0, delay=4.0), silent=True)],              # the footage ends in the storm's dust
    start(490, 40, -60, moon_yaw, moon_pitch),
    {"filmGrain": 0.09, "chromaticAberration": 0.8, "lensDirt": 0.9, "saturation": 0.6, "cloudCoverage": 0.2}))
# the footage ends in the storm's dust: its timeline (Core/Timeline), in its own
# file (written once by tools/make_sequences.py, tuned in tools/timeline_editor.py)
scenes['00_archive_ignition']["Sequences"] = [{"file": "Game/sequences/00_archive_storm.json"}]
# the same footage with the black hole as it would really be (Game/recursion/STORY.md): the Moon's
# mass, a horizon far smaller than a pixel. What is seen is what is around it: the middle of its
# disk clear (the stars through it, no shadow, no bent light) and a point of light where it feeds
scenes['00_archive_ignition_alternative'] = copy.deepcopy(scenes['00_archive_ignition'])
scenes['00_archive_ignition_alternative']['BlackHoles'][0].update({"horizon": 1e-5, "coreGlow": 6.0, "coreSize": 0.25})
scenes['00_archive_ignition_alternative']["Sequences"] = [{"file": "Game/sequences/00_archive_storm_alternative.json"},
                                                          {"file": "Game/sequences/00_archive_blast.json"}]   # its own: the lopsided blast
scenes['00_archive_ignition_alternative']['Environment'].update({"filmGrain": 0.15, "chromaticAberration": 1.0})

# --------------------------------------------------------------------------
# 01 Room 7: the room where you wake, AURA's lab built like a hospital ward (Ch.1), 4 x 5 m
# --------------------------------------------------------------------------
ents, lights = [], []
W, D = 4.0 * M, 5.0 * M
room(ents, 0, 0, W, D, doors=[('s', 0)])
door(ents, 0, D / 2 + 0.1 * M, locked=True, window=True)
ents.append(box((0.75 * M, 1.3 * M, D / 2 - 0.03 * M), (0.1 * M, 0.14 * M, 0.04 * M), DARK_PLASTIC))   # the keypad (no power)
ents.append(box((0.75 * M, 1.39 * M, D / 2 - 0.06 * M), (0.015 * M, 0.015 * M, 0.01 * M), LED_RED, cast_shadow=False))
# the hospital bed along the west wall (0.95 x 2.1 m), an IV stand
bx, bz = -W / 2 + 0.7 * M, -0.6 * M
ents.append(box((bx, 0.45 * M, bz), (0.95 * M, 0.1 * M, 2.1 * M), STEEL, True))
ents.append(box((bx, 0.55 * M, bz), (0.9 * M, 0.14 * M, 2.0 * M), MATTRESS))
ents.append(box((bx, 0.66 * M, bz - 0.8 * M), (0.6 * M, 0.1 * M, 0.35 * M), WHITE))       # pillow
ents.append(box((bx, 0.75 * M, bz - 1.05 * M), (0.95 * M, 0.6 * M, 0.04 * M), STEEL))     # headboard
for sx in (-1, 1):
    for sz in (-1, 1):
        ents.append(box((bx + sx * 0.42 * M, 0.2 * M, bz + sz * 0.98 * M), (0.05 * M, 0.4 * M, 0.05 * M), STEEL))
ents.append(box((bx + 0.7 * M, 0.95 * M, bz - 0.8 * M), (0.03 * M, 1.9 * M, 0.03 * M), STEEL))
ents.append(box((bx + 0.7 * M, 1.72 * M, bz - 0.8 * M), (0.1 * M, 0.17 * M, 0.04 * M),
                mat((0.75, 0.85, 0.9), extra={"glass": True, "roughness": 0.15})))                 # the IV bag
# the desk along the east wall: CRT, keyboard, papers, mug, instant photo, compass, radio; the fuse box under it
dx, dz = W / 2 - 0.4 * M, -0.5 * M
desk(ents, dx, dz, w=0.7 * M, d=1.5 * M)
top = 0.75 * M
crt(ents, dx + 0.08 * M, top, dz - 0.1 * M, yaw=-90)
ents.append(box((dx - 0.2 * M, top + 0.015 * M, dz - 0.1 * M), (0.15 * M, 0.03 * M, 0.45 * M), BEIGE))      # keyboard
papers(ents, dx - 0.05 * M, top, dz + 0.5 * M, n=6, spread=0.15 * M)
ents.append(box((dx - 0.2 * M, top + 0.05 * M, dz - 0.55 * M), (0.08 * M, 0.1 * M, 0.08 * M), mat((0.9, 0.9, 0.85))))   # coffee mug
ents.append(box((dx - 0.1 * M, top + 0.01 * M, dz + 0.25 * M), (0.09 * M, 0.01 * M, 0.11 * M), PAPER, rot=(0, 20, 0)))  # the instant photo
ents.append(entity("res/models/sphere.obj", (dx - 0.18 * M, top + 0.012 * M, dz + 0.05 * M), (0.03 * M, 0.01 * M, 0.03 * M),
                   material=mat((0.7, 0.62, 0.4), shininess=90, extra={"metallic": 0.7, "roughness": 0.3})))  # the pocket compass
ents.append(box((dx + 0.05 * M, top + 0.09 * M, dz - 0.65 * M), (0.3 * M, 0.18 * M, 0.12 * M), DARK_PLASTIC, rot=(0, 80, 0)))  # the radio
ents.append(box((W / 2 - 0.05 * M, 0.3 * M, dz + 0.4 * M), (0.08 * M, 0.4 * M, 0.3 * M), mat((0.5, 0.5, 0.52))))   # fuse box
for k, c in enumerate(((1, 0.1, 0.1), (0.1, 0.3, 1), (1, 0.85, 0.1), (0.1, 0.8, 0.2))):
    ents.append(box((W / 2 - 0.12 * M, 0.2 * M + k * 0.02 * M, dz + 0.3 * M + k * 0.07 * M), (0.12 * M, 0.012 * M, 0.012 * M),
                    mat(c), rot=(0, 0, 30 + k * 10)))
ents.append(box((W / 2 - 0.3 * M, 0.15 * M, dz + 0.95 * M), (0.25 * M, 0.3 * M, 0.25 * M), DARK_PLASTIC))   # the bin...
ents.append(box((W / 2 - 0.3 * M, 0.38 * M, dz + 0.95 * M), (0.19 * M, 0.27 * M, 0.06 * M), mat((0.95, 0.75, 0.15))))  # ...the cereal box
ents.append(box((0, H_ROOM - 0.01 * M, -0.3 * M), (0.25 * M, 0.02 * M, 0.25 * M), mat((0.6, 0.6, 0.6))))   # the ceiling speaker
ents.append(box((0.5 * M, 1.6 * M, -D / 2 + 0.02 * M), (0.4 * M, 0.55 * M, 0.01 * M), PAPER))             # the calendar (Dec 2005)
ents.append(box((0.5 * M, 1.82 * M, -D / 2 + 0.03 * M), (0.4 * M, 0.1 * M, 0.01 * M), mat((0.8, 0.1, 0.1))))
# scratched into the wall by the bed: 31 tally marks (Run 31: every one of you)
SCRATCH = mat((0.05, 0.05, 0.045), extra={"roughness": 0.9})
for k in range(31):
    group, i = divmod(k, 5)
    zz = bz - 0.5 * M + group * 0.26 * M + i * 0.045 * M
    if i == 4:
        ents.append(box((-W / 2 + 0.012 * M, 1.15 * M, zz - 0.09 * M), (0.01 * M, 0.025 * M, 0.24 * M), SCRATCH,
                        rot=(50, 0, 0), cast_shadow=False))
    else:
        ents.append(box((-W / 2 + 0.012 * M, 1.15 * M, zz), (0.01 * M, 0.24 * M, 0.022 * M), SCRATCH, cast_shadow=False))
tube(ents, lights, 0, -0.2 * M, along='z', flicker=0.06)
scenes['01_room7'] = scene(ents, lights, interior_env(), start(-0.3 * M, EYE, -1.2 * M, 180, -6))

# --------------------------------------------------------------------------
# 02 the corridor and Subject 4's door (Ch.2): 2.4 m wide, 24 m long
# --------------------------------------------------------------------------
ents, lights = [], []
CW, CL = 2.4 * M, 24.0 * M
cell_z = [z * M for z in (-9.5, -6, -2.5, 1, 4.5, 8)]
cell_doors = [(side, z) for side in 'ew' for z in cell_z]
room(ents, 0, 0, CW, CL, doors=cell_doors + [('n', 0)])
S4 = cell_z[2]                                                                              # Subject 4's door
for (side, z) in cell_doors:
    x = CW / 2 + 0.08 * M if side == 'e' else -CW / 2 - 0.08 * M
    door(ents, x, z, along='z', window=True, open_window=(side == 'e' and z == S4))
    ents.append(box((x + (-0.1 * M if side == 'e' else 0.1 * M), 2.3 * M, z), (0.01 * M, 0.15 * M, 0.3 * M), PAPER))   # cell plates
# Subject 4's cell behind her window: a small lit room, a woman lying on her
# bed along the far wall, very still, a blanket over her legs
room(ents, CW / 2 + 1.7 * M, S4, 3.0 * M, 2.6 * M, doors=[('w', 0)])
B4 = CW / 2 + 2.6 * M                                                                       # her bed's centre (x)
ents.append(box((B4, 0.4 * M, S4), (0.9 * M, 0.12 * M, 2.0 * M), STEEL, True))              # the frame
for lx in (-0.4, 0.4):
    for lz in (-0.95, 0.95):
        ents.append(box((B4 + lx * M, 0.17 * M, S4 + lz * M), (0.05 * M, 0.34 * M, 0.05 * M), STEEL))
ents.append(box((B4, 0.75 * M, S4 - 1.0 * M), (0.9 * M, 0.7 * M, 0.04 * M), STEEL))        # the headboard
ents.append(box((B4, 0.5 * M, S4), (0.85 * M, 0.1 * M, 1.95 * M), OFFWHITE))                # the mattress
ents.append(box((B4, 0.6 * M, S4 - 0.8 * M), (0.5 * M, 0.1 * M, 0.3 * M), PAPER))           # the pillow
ents.append(entity("res/models/FinalBaseMesh.obj", (B4, 0.66 * M, S4 + 0.85 * M), 1.65 * M / 20.74, (-90, 0, 0),
                   mat((0.85, 0.7, 0.6), extra={"roughness": 0.6})))                        # on her back, head on the pillow
ents.append(box((B4, 0.62 * M, S4 + 0.45 * M), (0.88 * M, 0.16 * M, 1.0 * M), mat((0.45, 0.55, 0.6), extra={"roughness": 0.9})))  # the blanket
tube(ents, lights, CW / 2 + 2.0 * M, S4, flicker=0.02, color=(1.0, 0.92, 0.75), radius=4 * M)
for k, z in enumerate([z * M for z in (-10, -5, 0, 5, 10)]):
    tube(ents, lights, 0, z, along='x', flicker=0.35 if k == 2 else 0.05, dead=(k == 1))
papers(ents, 0, 0, 3 * M, n=4, spread=0.8 * M)
papers(ents, 0.3 * M, 0, -7 * M, n=3, spread=0.6 * M)
ents.append(box((0, 2.35 * M, -CL / 2 + 0.03 * M), (0.6 * M, 0.2 * M, 0.04 * M), EXIT_GREEN, cast_shadow=False))
lights.append(light((0, 2.2 * M, -CL / 2 + 0.4 * M), (0.2, 1.0, 0.35), "POINT", (4 * M, 4 * M, 4 * M), cast_shadow=False,
                    diffuse_attenuation=[900, 14, 4], specular_attenuation=[900, 14, 4]))
door(ents, 0, -CL / 2 - 0.08 * M, window=False)                                          # the next wing
scenes['02_corridor_subject4'] = scene(ents, lights, interior_env(), start(0, EYE, CL / 2 - 1.5 * M, 0, 0))

# --------------------------------------------------------------------------
# 02 the twin levers: two rooms off a hall, a lever in each
# --------------------------------------------------------------------------
ents, lights = [], []
HW_, HD_ = 4.0 * M, 3.5 * M
room(ents, 0, 0, HW_, HD_, doors=[('w', 0), ('e', 0), ('n', 0, 1.3 * M, 2.3 * M)])      # the hall
ents.append(box((0, 1.15 * M, -HD_ / 2 - 0.1 * M), (1.3 * M, 2.3 * M, 0.1 * M), DOOR_PAINT, True))   # the locked exit
for sx in (-0.35, 0.35):
    ents.append(box((sx * M, 2.45 * M, -HD_ / 2 + 0.02 * M), (0.1 * M, 0.1 * M, 0.04 * M), LED_RED, cast_shadow=False))
RW, RD = 4.0 * M, 3.5 * M
for side in (-1, 1):
    rx = side * (HW_ / 2 + RW / 2 + 0.3 * M)
    room(ents, rx, 0, RW, RD, doors=[('e' if side < 0 else 'w', 0)])
    room(ents, side * (HW_ / 2 + 0.15 * M), 0, 0.3 * M + 0.01, DW + 0.2 * M, band=False,
         doors=[('w', 0), ('e', 0)])                                                          # the doorway through the wall
    wall_x = rx + side * RW / 2
    ents.append(box((wall_x - side * 0.04 * M, 1.2 * M, -0.5 * M), (0.06 * M, 0.5 * M, 0.3 * M), STEEL))   # the lever's plate
    ents.append(box((wall_x - side * 0.2 * M, 1.3 * M, -0.5 * M), (0.35 * M, 0.06 * M, 0.06 * M),
                    mat((0.85, 0.12, 0.08), extra={"roughness": 0.4}), rot=(0, 0, side * 35)))     # the handle
    # the lever's conduit: up the wall, along the ceiling, through into the
    # hall to the exit door (the two levers visibly work it together)
    CONDUIT = mat((0.75, 0.62, 0.15), shininess=40, extra={"roughness": 0.5, "metallic": 0.3})
    cx_ = wall_x - side * 0.08 * M
    ents.append(box((cx_, (1.45 * M + H_ROOM) / 2, -0.5 * M), (0.06 * M, H_ROOM - 1.45 * M, 0.06 * M), CONDUIT, cast_shadow=False))
    run_end = -side * 0.4 * M                                                               # over the exit door
    xa, xb = sorted((cx_, run_end))
    ents.append(box(((xa + xb) / 2, H_ROOM - 0.1 * M, -0.5 * M), (xb - xa, 0.06 * M, 0.06 * M), CONDUIT, cast_shadow=False))
    if side < 0:
        ents.append(box((0, H_ROOM - 0.1 * M, -(0.5 * M + HD_ / 2) / 2 - 0.1 * M), (0.86 * M, 0.06 * M, 0.06 * M), CONDUIT,
                        cast_shadow=False))
    for zz in (-HD_ / 2 + 0.1 * M,):
        ents.append(box((-side * 0.4 * M, H_ROOM - 0.1 * M, (zz - 0.5 * M) / 2), (0.06 * M, 0.06 * M, abs(zz + 0.5 * M)),
                        CONDUIT, cast_shadow=False))
        ents.append(box((-side * 0.4 * M, (2.3 * M + H_ROOM) / 2, zz), (0.06 * M, H_ROOM - 2.3 * M, 0.06 * M), CONDUIT,
                        cast_shadow=False))
    # a filing cabinet and a chair
    ents.append(box((rx - side * 1.6 * M, 0.65 * M, -RD / 2 + 0.35 * M), (0.5 * M, 1.3 * M, 0.6 * M),
                    mat((0.45, 0.47, 0.42), shininess=50, extra={"roughness": 0.45, "metallic": 0.4}), True))
    for dy in (0.3, 0.7, 1.1):
        ents.append(box((rx - side * 1.6 * M, dy * M, -RD / 2 + 0.66 * M), (0.12 * M, 0.03 * M, 0.02 * M), STEEL))
    chair(ents, rx + 0.3 * M, RD / 2 - 1.0 * M, yaw=0)
    desk(ents, rx, RD / 2 - 0.5 * M, w=1.2 * M, d=0.6 * M)
    if side < 0:                                     # the broom to wedge one lever down with (you're alone)
        ents.append(box((rx + 0.75 * M, 0.68 * M, RD / 2 - 0.25 * M), (0.03 * M, 1.3 * M, 0.03 * M), WOOD, rot=(0, 0, -12)))
        ents.append(box((rx + 0.88 * M, 0.06 * M, RD / 2 - 0.25 * M), (0.08 * M, 0.1 * M, 0.32 * M),
                        mat((0.55, 0.45, 0.25), extra={"roughness": 0.95})))
    tube(ents, lights, rx, 0, flicker=0.05)
tube(ents, lights, 0, 0, flicker=0.03)
scenes['02_twin_levers'] = scene(ents, lights, interior_env(), start(-(HW_ / 2 + RW / 2 + 0.3 * M), EYE, 0.8 * M, -90, 0))

# --------------------------------------------------------------------------
# 02 the security office: the CCTV wall; the robot's hallway beyond the glass
# --------------------------------------------------------------------------
ents, lights = [], []
OW, OD = 5.0 * M, 4.0 * M
room(ents, 0, 0, OW, OD, doors=[('s', 1.2 * M), ('e', 0, 2.0 * M, 2.2 * M)])   # (east: the glass)
door(ents, 1.2 * M, OD / 2 + 0.08 * M)
for row in range(4):
    for col in range(8):
        x = (-2.0 + col * 0.57) * M
        y = (0.95 + row * 0.42) * M
        ents.append(box((x, y, -OD / 2 + 0.22 * M), (0.52 * M, 0.38 * M, 0.42 * M), DARK_PLASTIC))
        tint = random.choice(((0.55, 0.8, 1.0), (0.6, 1.0, 0.7), (0.9, 0.9, 1.0), (0.4, 0.6, 0.9)))
        k = random.uniform(0.12, 0.4)
        ents.append(box((x, y, -OD / 2 + 0.44 * M), (0.44 * M, 0.32 * M, 0.02 * M), glow(tuple(c * k for c in tint)), cast_shadow=False))
ents.append(box((0, 0.4 * M, -OD / 2 + 0.25 * M), (4.8 * M, 0.8 * M, 0.5 * M), STEEL, True))   # the console under them
lights.append(light((0, 1.6 * M, -0.8 * M), (0.45, 0.65, 0.9), "POINT", (6 * M, 6 * M, 6 * M), cast_shadow=True,
                    shadowMapSize=512, diffuse_attenuation=[500, 8, 2], specular_attenuation=[500, 8, 2],
                    flicker=0.08, flickerSpeed=6))
desk(ents, 0, -0.6 * M, w=3.0 * M, d=0.7 * M)
ents.append(box((0, 0.765 * M, -0.6 * M), (0.45 * M, 0.03 * M, 0.15 * M), BEIGE))
chair(ents, 0, 0.1 * M, yaw=180)
# the hallway behind a glass wall (east), the cleaning robot in it
room(ents, OW / 2 + 2.6 * M, 0, 5.0 * M, 2.4 * M, doors=[('w', 0, 2.0 * M, 2.2 * M)])
# the observation window: glass over a counter
ents.append(box((OW / 2 + 0.05 * M, 0.5 * M, 0), (0.3 * M, 1.0 * M, 2.0 * M), DOOR_PAINT, True))
ents.append(box((OW / 2 + 0.05 * M, 1.02 * M, 0), (0.45 * M, 0.04 * M, 2.1 * M), WOOD))
ents.append(box((OW / 2 + 0.05 * M, 1.6 * M, 0), (0.02 * M, 1.2 * M, 2.0 * M), GLASS, True))   # the glass
rx_ = OW / 2 + 3.0 * M
ents.append(entity("res/models/capsule.obj", (rx_, 0.45 * M, 0.2 * M), (0.3 * M / 1.15, 0.45 * M / 2.12, 0.3 * M / 1.15),
                   material=mat((0.9, 0.85, 0.3), extra={"roughness": 0.4})))
ents.append(box((rx_, 0.95 * M, 0.2 * M), (0.35 * M, 0.18 * M, 0.35 * M), mat((0.9, 0.9, 0.88), extra={"roughness": 0.3})))
ents.append(box((rx_, 0.97 * M, 0.2 * M + 0.18 * M), (0.15 * M, 0.04 * M, 0.01 * M), LED_RED, cast_shadow=False))
ents.append(box((rx_, 0.06 * M, 0.2 * M), (0.5 * M, 0.12 * M, 0.5 * M), DARK_PLASTIC))             # its brush deck
tube(ents, lights, OW / 2 + 2.6 * M, 0, flicker=0.3)
scenes['02_security_office'] = scene(ents, lights, interior_env(ambient=0.035), start(1.2 * M, EYE, 1.2 * M, 20, -6))

# --------------------------------------------------------------------------
# 02 things that don't add up: the waiting room
# --------------------------------------------------------------------------
ents, lights = [], []
WW, WD = 5.0 * M, 4.0 * M
room(ents, 0, 0, WW, WD, walls=mat((0.55, 0.5, 0.4), extra={"roughness": 0.85}), doors=[('s', -1.6 * M)])
door(ents, -1.6 * M, WD / 2 + 0.08 * M)
# the "windows": screens of a quiet street at night, a full Moon over
# the rooftops (the last Moon the player will see: outside there is none). The
# same Moon in the same place in both, and the same red car: they're screens
for k, x in enumerate((-1.2 * M, 1.1 * M)):
    zf = -WD / 2 + 0.02 * M                                                                # the screen's face
    ents.append(box((x, 1.45 * M, -WD / 2 - 0.02 * M), (1.5 * M, 1.1 * M, 0.06 * M), STEEL))
    ents.append(box((x, 1.65 * M, zf), (1.4 * M, 0.55 * M, 0.02 * M), glow((0.04, 0.055, 0.11)), cast_shadow=False))  # night sky
    ents.append(entity("res/models/sphere.obj", (x + 0.38 * M, 1.79 * M, zf + 0.012 * M), (0.075 * M, 0.075 * M, 0.004 * M),
                       material=glow((1.0, 0.97, 0.88)), cast_shadow=False))                                         # the Moon
    for (hx_, hw, hh) in ((-0.5, 0.36, 0.22), (-0.13, 0.3, 0.3), (0.2, 0.34, 0.17), (0.52, 0.3, 0.25)):
        ents.append(box((x + hx_ * M, 1.375 * M + hh / 2 * M, zf + 0.018 * M), (hw * M, hh * M, 0.01 * M),
                        glow((0.006, 0.006, 0.01)), cast_shadow=False))                                              # rooftops
        ents.append(box((x + (hx_ - 0.06) * M, 1.375 * M + hh * 0.45 * M, zf + 0.025 * M), (0.05 * M, 0.04 * M, 0.005 * M),
                        glow((0.9, 0.6, 0.25)), cast_shadow=False))                                                  # a lit window
    ents.append(box((x, 1.2 * M, zf), (1.4 * M, 0.35 * M, 0.02 * M), glow((0.12, 0.13, 0.17)), cast_shadow=False))   # the street
    ents.append(box((x + (-0.25 + k * 0.5) * M, 1.28 * M, zf + 0.015 * M), (0.35 * M, 0.15 * M, 0.02 * M),
                    glow((0.45, 0.03, 0.02)), cast_shadow=False))                                                    # the red car
    ents.append(box((x + (-0.25 + k * 0.5 - 0.17) * M, 1.27 * M, zf + 0.03 * M), (0.03 * M, 0.025 * M, 0.005 * M),
                    glow((1.0, 0.9, 0.6)), cast_shadow=False))                                                       # its headlight
lights.append(light((0, 1.5 * M, -WD / 2 + 0.5 * M), (0.35, 0.45, 0.75), "SPOT", (8 * M, 8 * M, 6 * M), rot=(0, 180, 0),
                    cast_shadow=True, coneAngle=0.7, diffuse_attenuation=[500, 6, 1.5], specular_attenuation=[500, 6, 1.5]))
# clocks that never move
for x, z, yaw in ((-WW / 2 + 0.03 * M, 0, 90), (WW / 2 - 0.03 * M, -0.4 * M, -90), (0.6 * M, WD / 2 - 0.03 * M, 180)):
    a = math.radians(yaw)
    ents.append(entity("res/models/sphere.obj", (x, 2.0 * M, z), (0.15 * M, 0.15 * M, 0.02 * M), (0, yaw, 0), WHITE))
    ents.append(box((x + math.sin(a) * 0.025 * M, 2.06 * M, z + math.cos(a) * 0.025 * M), (0.012 * M, 0.12 * M, 0.01 * M),
                    DARK_PLASTIC, rot=(0, yaw, 0)))
    ents.append(box((x + math.sin(a) * 0.03 * M, 2.04 * M, z + math.cos(a) * 0.03 * M), (0.012 * M, 0.09 * M, 0.01 * M),
                    DARK_PLASTIC, rot=(0, yaw, -6)))
# sofa, coffee table, the newspapers (all the same date), the radio
SOFA = mat((0.35, 0.22, 0.18), extra={"roughness": 0.9})
SX = 0.4 * M                                                                               # (clear of the door)
ents.append(box((SX, 0.22 * M, 1.2 * M), (2.0 * M, 0.44 * M, 0.8 * M), SOFA, True))
ents.append(box((SX, 0.6 * M, 1.55 * M), (2.0 * M, 0.5 * M, 0.15 * M), SOFA))
ents.append(box((SX, 0.42 * M, 0.25 * M), (1.1 * M, 0.04 * M, 0.55 * M), WOOD, True))
for k in range(5):
    ents.append(box((SX - 0.2 * M + k * 0.1 * M, 0.45 * M + k * 0.004 * M, 0.25 * M + random.uniform(-0.05, 0.05) * M),
                    (0.38 * M, 0.004 * M, 0.55 * M), PAPER, rot=(0, random.uniform(-20, 20), 0), cast_shadow=False))
ents.append(box((SX + 0.4 * M, 0.52 * M, 0.25 * M), (0.3 * M, 0.18 * M, 0.12 * M), DARK_PLASTIC))
ents.append(box((1.8 * M, 0.9 * M, 1.6 * M), (0.8 * M, 1.8 * M, 0.35 * M), WOOD, True))                   # shelves ("painted dust")
for y in (0.5, 1.0, 1.5):
    ents.append(box((1.8 * M, y * M, 1.6 * M - 0.18 * M), (0.8 * M, 0.03 * M, 0.02 * M), OFFWHITE))
tube(ents, lights, 0, 0.5 * M, flicker=0.04)
scenes['02_waiting_room'] = scene(ents, lights, interior_env(), start(-1.6 * M, EYE, 1.4 * M, -10, 0))

# --------------------------------------------------------------------------
# 02 the tapes: the records room, the shutter coming down
# --------------------------------------------------------------------------
ents, lights = [], []
RRW, RRD = 6.0 * M, 5.0 * M
room(ents, 0, 0, RRW, RRD, walls=PEELING, doors=[('n', 0, 1.2 * M, 2.2 * M)])
tapes = []
SHELF = mat((0.4, 0.42, 0.44), shininess=60, extra={"roughness": 0.5, "metallic": 0.4})
for k, x in enumerate([v * M for v in (-2.2, -0.8, 0.8, 2.2)]):
    # (2.4 m long: an aisle wide enough to walk round both ends)
    ents.append(box((x, 1.0 * M, -0.2 * M), (0.5 * M, 2.0 * M, 2.4 * M), SHELF, True))
    for y in (0.3, 0.75, 1.2, 1.65):
        for z in [(-1.35 + 0.12 * i) * M for i in range(20)]:
            if random.random() < 0.85:
                for sx in (-1, 1):
                    tapes.append({"position": [round(x + sx * 0.27 * M, 2), round(y * M + 0.035 * M, 2), round(z, 2)],
                                  "rotation": [0, 0, 0], "scale": [0.07 * M, 0.11 * M, 0.017 * M]})
ents.append({"Transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
             "Components": {"InstanceRenderComponent": {"Mesh": "res/models/cube4.obj", "Transforms": tapes,
                                                        "Material": mat((0.12, 0.12, 0.13), extra={"roughness": 0.5})}}})
desk(ents, 0, 1.8 * M, w=1.4 * M, d=0.7 * M)
top = 0.75 * M
ents.append(box((0, top + 0.06 * M, 1.8 * M), (0.4 * M, 0.12 * M, 0.25 * M), DARK_PLASTIC))                  # the tape player
ents.append(box((0.1 * M, top + 0.125 * M, 1.8 * M), (0.1 * M, 0.01 * M, 0.06 * M), mat((0.15, 0.1, 0.08))))  # RUN 30 in it
ents.append(box((-0.5 * M, top + 0.15 * M, 1.8 * M), (0.12 * M, 0.3 * M, 0.12 * M), glow((1.0, 0.8, 0.5))))   # desk lamp
lights.append(light((-0.5 * M, top + 0.4 * M, 1.8 * M), (1.0, 0.75, 0.45), "POINT", (4 * M, 4 * M, 4 * M), cast_shadow=True,
                    shadowMapSize=512, diffuse_attenuation=[600, 10, 3], specular_attenuation=[600, 10, 3]))
# the shutter half way down over the door, a red warning light
SLAT = mat((0.33, 0.33, 0.31), shininess=50, extra={"roughness": 0.55, "metallic": 0.5})
for k in range(12):                                                                          # a roller shutter, half down
    ents.append(box((0, 2.15 * M - k * 0.1 * M, -RRD / 2 - 0.05 * M), (1.2 * M, 0.09 * M, 0.06 * M), SLAT, True,
                    rot=(4 if k % 2 else -4, 0, 0)))
ents.append(box((0, 1.0 * M, -RRD / 2 - 0.05 * M), (1.22 * M, 0.06 * M, 0.1 * M), DARK_PLASTIC, True))     # its bottom bar
# beyond it: the dark corridor out of the wing
room(ents, 0, -RRD / 2 - 1.5 * M, 2.4 * M, 2.6 * M, walls=PEELING, doors=[('s', 0, 1.2 * M, 2.2 * M)])
ents.append(box((0, 2.4 * M, -RRD / 2 + 0.03 * M), (0.15 * M, 0.15 * M, 0.05 * M), LED_RED, cast_shadow=False))
lights.append(light((0, 2.3 * M, -RRD / 2 + 0.4 * M), (1.0, 0.1, 0.05), "POINT", (5 * M, 5 * M, 5 * M), cast_shadow=False,
                    diffuse_attenuation=[600, 10, 4], specular_attenuation=[600, 10, 4], flicker=0.4, flickerSpeed=3))
tube(ents, lights, -1.5 * M, -0.3 * M, along='z', flicker=0.05)
tube(ents, lights, 1.5 * M, -0.3 * M, along='z', flicker=0.35)
scenes['02_records_room'] = scene(ents, lights, interior_env(ambient=0.04), start(0, EYE, 2.2 * M, 0, -5))

# --------------------------------------------------------------------------
# 03 the stairwell: the EXIT door, the stairs going up and up
# --------------------------------------------------------------------------
ents, lights = [], []
FLIGHTS, STEPS = 10, 9
RISER, TREAD = 0.17 * M, 0.28 * M
RISE = STEPS * RISER                                # one flight (~1.5 m)
RUN = STEPS * TREAD                                 # its length (~2.5 m)
SWX, LAND = 2.8 * M, 1.2 * M                        # shaft width; the landings
SWZ = RUN + 2 * LAND
room(ents, 0, 0, SWX, SWZ, h=FLIGHTS * RISE + 2.5 * M, walls=CONCRETE, floor=CONCRETE, ceiling=CONCRETE,
     doors=[('s', 0)])
for f in range(FLIGHTS):
    y0 = f * RISE
    up_north = f % 2 == 0                           # flights climb northwards, then back south
    x = (-0.65 if up_north else 0.65) * M
    z_start = (SWZ / 2 - LAND) if up_north else (-SWZ / 2 + LAND)
    direction = -1 if up_north else 1
    for s_ in range(STEPS):
        z = z_start + direction * (s_ + 0.5) * TREAD
        ents.append(box((x, y0 + (s_ + 1) * RISER / 2, z), (1.2 * M, (s_ + 1) * RISER, TREAD), CONCRETE))
    # the walkable ramp under the steps (invisible) and the landing at the top
    length = math.hypot(RUN, RISE)
    pitch = math.degrees(math.atan2(RISE, RUN)) * (1 if up_north else -1)
    ents.append({"Transform": {"position": [x, y0 + RISE / 2, z_start + direction * RUN / 2], "rotation": [pitch, 0, 0],
                               "scale": [1.2 * M, 0.05 * M, length]},
                 "Components": {"PhysicsComponent": box_physics((1.2 * M, 0.05 * M, length))}})
    zl = -SWZ / 2 + LAND / 2 if up_north else SWZ / 2 - LAND / 2
    ents.append(box((0, y0 + RISE - 0.05 * M, zl), (SWX, 0.1 * M, LAND), CONCRETE, True))
    ents.append(box((0, y0 + RISE / 2, z_start + direction * RUN / 2), (0.1 * M, RISE + 1.0 * M, RUN), CONCRETE, True))  # the middle wall
    lights.append(light((0, y0 + RISE + 2.0 * M, zl), (1.0, 0.72, 0.42), "POINT", (4 * M, 4 * M, 4 * M), cast_shadow=False,
                        diffuse_attenuation=[500, 10, 3], specular_attenuation=[500, 10, 3],
                        flicker=0.3 if f % 3 == 1 else 0.06, flickerSpeed=6))
    ents.append(entity("res/models/sphere.obj", (SWX / 2 - 0.15 * M, y0 + RISE + 2.0 * M, zl), 0.06 * M,
                       material=glow((1.0, 0.75, 0.45)), cast_shadow=False))
    # a narrow window in the west wall: AURA's datacenter, rows of racks and
    # their blinking lights going down further than you can see
    wy = y0 + RISE + 1.3 * M
    ents.append(box((-SWX / 2 + 0.03 * M, wy, zl), (0.06 * M, 0.7 * M, 0.5 * M), STEEL))
    ents.append(box((-SWX / 2 + 0.065 * M, wy, zl), (0.01 * M, 0.6 * M, 0.4 * M), glow((0.01, 0.025, 0.04)), cast_shadow=False))
    for k in range(10):
        ents.append(box((-SWX / 2 + 0.072 * M, wy + random.uniform(-0.27, 0.27) * M, zl + random.uniform(-0.18, 0.18) * M),
                        (0.005 * M, 0.012 * M, 0.012 * M),
                        glow((0.2, 1.0, 0.4) if random.random() < 0.7 else (1.0, 0.6, 0.15)), cast_shadow=False))
ents.append(box((0, 2.35 * M, SWZ / 2 - 0.03 * M), (0.5 * M, 0.17 * M, 0.04 * M), EXIT_GREEN, cast_shadow=False))
door(ents, 0, SWZ / 2 + 0.08 * M, window=False)                                             # the steel door behind you
lights.append(light((0, 2.2 * M, SWZ / 2 - 0.4 * M), (0.2, 1.0, 0.35), "POINT", (3 * M, 3 * M, 3 * M), cast_shadow=False,
                    diffuse_attenuation=[900, 14, 5], specular_attenuation=[900, 14, 5]))
ents.append(box((0.65 * M, FLIGHTS * RISE + 2.48 * M, -SWZ / 2 + LAND / 2), (0.9 * M, 0.05 * M, 0.9 * M),
                mat((0.25, 0.3, 0.45), emissive=0.4)))                                     # the hatch, night beyond
scenes['03_stairwell'] = scene(ents, lights, interior_env(ambient=0.03),
                               start(-0.65 * M, EYE, SWZ / 2 - 0.5 * M, 0, 20))

# --------------------------------------------------------------------------
# 03 the canyon (Ch.3 and Ch.4): out of the hatch at night, down the torch
# path to the plaza: the AURA monument, the dead kiosk, the mirrors round the
# pool (the kiosk's noon archive and its night telemetry are its later states)
# --------------------------------------------------------------------------
def hatch(x, y, z):
    """a hatch up out of AURA's underground: its rusty housing, the dark shaft, the lid thrown open"""
    return [box((x, y + 2, z), (22, 5, 22), RUSTY, True),
            box((x, y + 5, z), (16, 1, 16), mat((0.02, 0.02, 0.02))),
            box((x - 10, y + 14, z), (2, 18, 16), RUSTY, rot=(0, 0, -20))]

hx, hz = CANYON_SPAWN
hy = canyon_ground(hx, hz)
g = year4000(CANYON, 0.0, CANYON_MOON, HOLE_CANYON,
            start(hx + 15, hy + 28, hz + 25, yaw_towards((hx, hz), (hx + CANYON_MOON[0], hz + CANYON_MOON[2])), 22))
g["Entities"] += hatch(hx, hy, hz)
# the road sign, half buried in the hardened flood mud of the west cliff,
# facing the torch path
SIGN_Z = 700.0
SIGN_X = next(x for x in range(40, -200, -1) if canyon_ground(x, SIGN_Z) > 40)
ROAD_GREEN = mat((0.06, 0.3, 0.14), shininess=60, extra={"roughness": 0.4})
g["Entities"] += [box((SIGN_X + 1.5, 40, SIGN_Z), (0.12 * M, 1.4 * M, 2.8 * M), ROAD_GREEN, rot=(0, 0, 9)),
                  box((SIGN_X + 1.4, 41, SIGN_Z), (0.13 * M, 0.25 * M, 1.6 * M), PAPER, rot=(0, 0, 9))]   # its lettering
# the monument (AURA's own): its plaque on the pedestal's west face (its front
# is behind the altar, where nobody fits); a charcoal rubbing half over it, of
# the scratch an earlier run left ("IT DIDN'T COME FROM SPACE. 7")
g["Entities"] += [box((-32.6, 19, PED[1]), (1, 0.4 * M, 0.9 * M), BRONZE),
                  box((-33.2, 18, PED[1] + 4), (0.15, 0.3 * M, 0.21 * M), PAPER, rot=(8, 0, 0), cast_shadow=False),
                  # someone else was reading it: a lantern left on the ground
                  box((-38, CANYON_TOP + 3, PED[1] - 6), (0.15 * M, 0.25 * M, 0.15 * M), DARK_PLASTIC),
                  box((-38, CANYON_TOP + 3.5, PED[1] - 6), (0.1 * M, 0.14 * M, 0.1 * M), glow((1.0, 0.65, 0.3)), cast_shadow=False)]
g["Lights"].append(light((-38, CANYON_TOP + 8, PED[1] - 6), (1.0, 0.62, 0.3), "POINT", (90, 90, 90), cast_shadow=False,
                         diffuse_attenuation=[250, 6, 2.5], specular_attenuation=[250, 6, 2.5], flicker=0.25, flickerSpeed=9))
# the kiosk by the pool, dead for centuries, its solar panel up; the mirrors
# on poles along the pool, to be turned so the noon sun hits its panel
KX, KZ = -55.0, -640.0
g["Entities"] += [box((KX, CANYON_TOP + 16, KZ), (14, 32, 10), STEEL, True),
                  box((KX + 7.2, CANYON_TOP + 22, KZ), (0.4, 10, 8), GLASS_DARK, cast_shadow=False),
                  box((KX, CANYON_TOP + 33, KZ), (20, 1, 14), mat((0.1, 0.12, 0.25), shininess=120, extra={"roughness": 0.15}),
                      rot=(20, 0, 0)),                                                    # its solar panel
                  box((KX + 7.4, CANYON_TOP + 12, KZ), (0.4, 4, 6), DARK_PLASTIC)]         # its buttons
for (x, z, yaw) in ((-38, -700, 60), (38, -760, -60), (-38, -840, 120), (38, -900, -120)):
    g["Entities"] += [box((x, CANYON_TOP + 10, z), (1.5, 20, 1.5), STEEL),
                      box((x, CANYON_TOP + 22, z), (14, 12, 0.6), MIRROR, rot=(15, yaw, 0))]
# the way down to the lagoon: a concrete hut at the edge of the plaza, its steel
# door left open under a green EXIT sign (AURA's maze goes on underground)
HUT = (75.0, -610.0)
hut_y = canyon_ground(*HUT)
g["Entities"] += [box((HUT[0] + 1.6 * M, hut_y + 1.3 * M, HUT[1]), (3.2 * M, 2.6 * M, 3.0 * M), CONCRETE, True),
                  box((HUT[0] - 0.01 * M, hut_y + 1.05 * M, HUT[1]), (0.04 * M, 2.1 * M, 1.0 * M),
                      glow((0.005, 0.005, 0.005)), cast_shadow=False),                              # the dark doorway
                  box((HUT[0] - 0.45 * M, hut_y + 1.05 * M, HUT[1] + 0.85 * M), (0.06 * M, 2.1 * M, 1.0 * M), DOOR_PAINT,
                      rot=(0, 70, 0)),                                                               # the door, swung open
                  box((HUT[0] - 0.03 * M, hut_y + 2.35 * M, HUT[1]), (0.04 * M, 0.17 * M, 0.5 * M), EXIT_GREEN,
                      cast_shadow=False)]
g["Lights"].append(light((HUT[0] - 0.6 * M, hut_y + 2.3 * M, HUT[1]), (0.2, 1.0, 0.35), "POINT", (60, 60, 60),
                         cast_shadow=False, diffuse_attenuation=[300, 8, 3], specular_attenuation=[300, 8, 3]))
scenes['03_canyon'] = g

# --------------------------------------------------------------------------
# 04 the way down: AURA's tunnels under the canyon, out to the lagoon. On one
# side, windows into the shelters (people asleep in bunks, generations who
# have never seen the sky); on the other, the datacenter; at the far end a
# ladder up to the hatch on the hill above the lagoon. And proof that you (or
# something) came this way before: chalk arrows signed 7, drawn over and over;
# an empty bunk with a 7 chalked on its window; a pile of SUBJECT 7 wristbands
# at the foot of the ladder; a page in your handwriting
# --------------------------------------------------------------------------
def chalk(ents, x, z0, y0, right, strokes, material, w=0.03 * M):
    """chalk strokes on a wall facing +-x: (u1, v1, u2, v2) in metres, u to the
    viewer's right (right=+1 if that is +z, -1 if -z), v up, from (z0, y0)"""
    for u1, v1, u2, v2 in strokes:
        du, dv = (u2 - u1) * right, v2 - v1
        ents.append(box((x, y0 + (v1 + v2) / 2 * M, z0 + (u1 + u2) / 2 * right * M),
                        (0.006 * M, math.hypot(du, dv) * M + w, w), material,
                        rot=(math.degrees(math.atan2(du, dv)), 0, 0), cast_shadow=False))
SEVEN = [(0, 0.3, 0.2, 0.3), (0.2, 0.3, 0.06, 0)]
ARROW = [(0, 0.1, 0.6, 0.1), (0.6, 0.1, 0.48, 0.22), (0.6, 0.1, 0.48, -0.02)]
CHALK = [mat((c, c, c * 0.95), extra={"roughness": 1.0}) for c in (0.3, 0.5, 0.9)]   # faded .. fresh
ents, lights = [], []
TW, TL, TH = 2.6 * M, 40.0 * M, 2.8 * M
DORMS = (12.0 * M, 0.0, -12.0 * M)
WIN_W, WIN_H, SILL = 1.8 * M, 2.0 * M, 0.9 * M
WT = 0.15 * M                                       # (room()'s wall thickness)
room(ents, 0, 0, TW, TL, h=TH, walls=CONCRETE, floor=CONCRETE, ceiling=CONCRETE,
     doors=[('s', 0)] + [('e', z, WIN_W, WIN_H) for z in DORMS])
door(ents, 0, TL / 2 + 0.08 * M, window=False)                                             # back to the canyon
SLEEPER = mat((0.62, 0.52, 0.46), extra={"roughness": 0.7})
for z in DORMS:
    ents.append(box((TW / 2 + WT / 2, SILL / 2, z), (WT, SILL, WIN_W), CONCRETE, True))          # the sill
    ents.append(box((TW / 2 + WT / 2, (SILL + WIN_H) / 2, z), (0.03 * M, WIN_H - SILL, WIN_W), GLASS, True))
    DWD, DD = 4.0 * M, 3.4 * M
    cx = TW / 2 + WT + DWD / 2
    room(ents, cx, z, DWD, DD, h=TH, walls=CONCRETE, floor=CONCRETE, ceiling=CONCRETE, doors=[('w', 0, WIN_W, WIN_H)])
    for bx in (cx + 0.2 * M, cx + 1.3 * M):                                                # two bunk beds
        for level in (0.45 * M, 1.5 * M):
            ents.append(box((bx, level, z), (0.9 * M, 0.12 * M, 2.0 * M), STEEL, True))
            ents.append(box((bx, level + 0.1 * M, z), (0.85 * M, 0.08 * M, 1.95 * M), mat((0.3, 0.33, 0.4))))
            asleep = random.random() < 0.8
            if z == DORMS[1] and bx == cx + 0.2 * M and level == 0.45 * M:                  # an empty bunk: yours? (under the strobing tube)
                ents.append(box((bx, level + 0.2 * M, z + 0.75 * M), (0.8 * M, 0.12 * M, 0.4 * M), mat((0.3, 0.33, 0.4)),
                                rot=(0, 6, 0)))                                             # the blanket, pushed back
                chalk(ents, TW / 2 + WT / 2 - 0.02 * M, z - 0.5 * M, 1.25 * M, 1, SEVEN, CHALK[2])
            elif asleep:                                                                    # someone asleep, on their back
                ents.append(entity("res/models/FinalBaseMesh.obj", (bx, level + 0.25 * M, z + 0.85 * M),
                                   1.6 * M / 20.74, (-90, 0, 0), SLEEPER))
        for corner in (-1, 1):
            ents.append(box((bx, 1.0 * M, z + corner * 0.97 * M), (0.06 * M, 2.0 * M, 0.06 * M), STEEL))
    lights.append(light((cx + 0.8 * M, TH - 0.3 * M, z), (0.35, 0.48, 1.0), "POINT", (4 * M, 4 * M, 4 * M),
                        cast_shadow=False, diffuse_attenuation=[700, 6, 1.5], specular_attenuation=[700, 6, 1.5]))
for z in (6.0 * M, -6.0 * M):                                                               # the datacenter, below
    ents.append(box((-TW / 2 + 0.03 * M, 1.5 * M, z), (0.06 * M, 0.7 * M, 1.4 * M), STEEL))
    ents.append(box((-TW / 2 + 0.065 * M, 1.5 * M, z), (0.01 * M, 0.6 * M, 1.3 * M), glow((0.01, 0.025, 0.04)),
                    cast_shadow=False))
    for k in range(18):
        ents.append(box((-TW / 2 + 0.072 * M, 1.5 * M + random.uniform(-0.27, 0.27) * M, z + random.uniform(-0.6, 0.6) * M),
                        (0.005 * M, 0.012 * M, 0.012 * M),
                        glow((0.2, 1.0, 0.4) if random.random() < 0.7 else (1.0, 0.6, 0.15)), cast_shadow=False))
for k, z in enumerate([v * M for v in (16, 8, 0, -8, -16)]):
    tube(ents, lights, 0, z, h=TH, along='z', flicker=0.35 if k == 2 else 0.05, dead=(k == 3))
for rx_ in (-0.25 * M, 0.25 * M):                                                           # the ladder up
    ents.append(box((rx_, TH / 2, -TL / 2 + 0.1 * M), (0.05 * M, TH, 0.05 * M), STEEL))
for k in range(int((TH - 0.3 * M) / (0.3 * M))):
    ents.append(box((0, 0.3 * M + k * 0.3 * M, -TL / 2 + 0.1 * M), (0.5 * M, 0.04 * M, 0.04 * M), STEEL))
ents.append(box((0, TH - 0.02 * M, -TL / 2 + 0.5 * M), (0.9 * M, 0.04 * M, 0.9 * M),
                mat((0.25, 0.3, 0.45), emissive=0.4)))                                     # the hatch, night beyond
lights.append(light((0, TH - 0.4 * M, -TL / 2 + 0.6 * M), (0.3, 0.38, 0.6), "POINT", (4 * M, 4 * M, 4 * M),
                    cast_shadow=False, diffuse_attenuation=[600, 6, 1.5], specular_attenuation=[600, 6, 1.5]))  # night, down the hatch
for z in (14.0 * M, 2.0 * M, -10.0 * M):            # arrows to the ladder, signed 7, drawn again and again
    for k, (dv, du) in enumerate(((0.06, 0.05), (-0.03, -0.04), (0, 0))):
        chalk(ents, -TW / 2 + 0.005 * M + k * 0.002 * M, z + du * M, 1.2 * M + dv * M, -1, ARROW, CHALK[k])
        chalk(ents, -TW / 2 + 0.005 * M + k * 0.002 * M, z + (0.85 + du) * M, 1.15 * M + dv * M, -1, SEVEN, CHALK[k])
bands = random.Random(7)                            # the SUBJECT 7 wristbands at the foot of the ladder
for k in range(30):                                 # runs 1 to 30 (31 is on your wrist)
    ents.append(box((0.75 * M + bands.uniform(-0.25, 0.25) * M, 0.01 * M + k * 0.002 * M,
                     -TL / 2 + 0.55 * M + bands.uniform(-0.25, 0.25) * M), (0.22 * M, 0.004 * M, 0.025 * M),
                    OFFWHITE, rot=(0, bands.uniform(0, 180), 0), cast_shadow=False))
ents.append(box((-TW / 2 + 0.005 * M, 1.5 * M, -TL / 2 + 1.2 * M), (0.005 * M, 0.28 * M, 0.21 * M), PAPER,
                rot=(4, 0, 0), cast_shadow=False))                                          # a page, taped up
ents.append(box((-TW / 2 + 0.009 * M, 1.64 * M, -TL / 2 + 1.2 * M), (0.004 * M, 0.03 * M, 0.08 * M),
                mat((0.75, 0.68, 0.45)), cast_shadow=False))                                # the tape
scenes['04_tunnels'] = scene(ents, lights, interior_env(ambient=0.03), start(0, EYE, TL / 2 - 1.5 * M, 0, 0))

# --------------------------------------------------------------------------
# 05 the lagoon (Ch.5): your camp from earlier runs (the journal pages), the
# hill where AURA speaks in your head and asks its question
# --------------------------------------------------------------------------
# the hill: on the lagoon's east shore above the water (clear of the trees, a
# view across to the lighthouse island). The tunnels come out at a hatch just
# behind it; you start beside it, the black hole ahead over the water
HILL = (820.0, -900.0)
HILL_Y = lagoon_ground(*HILL)

# the black hole's close pass: its tide, far stronger than the Moon's ever
# was, has pulled the lagoon's water out. Steep mud banks under the old
# waterline (the terrain's shore band marks it: the high-tide line), a pool
# left in the bottom; by the last dawn hardly a puddle
WATER_LAGOON, WATER_DAWN = -75.0, -98.0
HULL = mat((0.2, 0.28, 0.33), extra={"roughness": 0.85})             # a fishing boat's faded paint
OLD_WOOD = mat((0.3, 0.24, 0.18), extra={"roughness": 0.95})

def euler_matrix(rx, ry, rz):
    """the rotation matrix of an entity's rot (degrees), as the engine builds
    it (glm::quat from Euler angles): to place the parts of a rigid object"""
    cx, cy, cz = (math.cos(math.radians(a) / 2) for a in (rx, ry, rz))
    sx, sy, sz = (math.sin(math.radians(a) / 2) for a in (rx, ry, rz))
    w = cx * cy * cz + sx * sy * sz
    x = sx * cy * cz - cx * sy * sz
    y = cx * sy * cz + sx * cy * sz
    z = cx * cy * sz - sx * sy * cz
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)],
            [2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)],
            [2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)]]

def rigid(origin, rot, parts):
    """boxes (local centre, size, material) of one object turned by rot, at origin"""
    R = euler_matrix(*rot)
    out = []
    for (lx, ly, lz), size, material in parts:
        wx = origin[0] + R[0][0] * lx + R[0][1] * ly + R[0][2] * lz
        wy = origin[1] + R[1][0] * lx + R[1][1] * ly + R[1][2] * lz
        wz = origin[2] + R[2][0] * lx + R[2][1] * ly + R[2][2] * lz
        out.append(box((wx, wy, wz), size, material, solid=True, rot=rot))
    return out

def drained_lagoon():
    """what two thousand years and the water's going left of the camp's pier and
    boat: a row of rotten stumps in the mud, and the boat's rusted hull, half
    buried where it sank (it was steel: only its outline is left)"""
    ents = []
    rng = random.Random(2035)   # (a seed, kept: the same wreckage)
    ROTTEN = mat((0.17, 0.14, 0.11), extra={"roughness": 1.0})
    RUST = mat((0.3, 0.16, 0.09), extra={"roughness": 0.95, "metallic": 0.2})
    # the pier: out from the camp's old shore towards the middle of the lagoon
    P0, d = (675.0, -410.0), (-0.43, -0.9)
    n = math.hypot(*d); d = (d[0] / n, d[1] / n); side = (-d[1], d[0])
    SPAN = 1.6 * M
    for k in range(8):
        for s_ in (-1, 1):
            if rng.random() < 0.3: continue                                         # rotted away
            t = k * SPAN
            x, z = P0[0] + d[0] * t + side[0] * s_ * 0.9 * M, P0[1] + d[1] * t + side[1] * s_ * 0.9 * M
            h = rng.uniform(0.3, 1.4) * M
            gy = lagoon_ground(x, z)
            ents.append(box((x, gy + h / 2 - 4, z), (0.2 * M, h + 8, 0.2 * M), ROTTEN, True,
                            rot=(rng.uniform(-12, 12), rng.uniform(0, 90), rng.uniform(-12, 12))))
    # the boat's hull, rusted to an outline, sunk in the bank below the pier's end
    BX, BZ = 600.0, -530.0
    by = lagoon_ground(BX, BZ) - 0.45 * M
    boat_yaw = math.degrees(math.atan2(0.92, -0.38))
    ents += rigid((BX, by, BZ), (18, boat_yaw, -12), [
        ((0, 0.75 * M, 0.9 * M), (6.0 * M, 0.9 * M, 0.08 * M), RUST),               # what is left of its sides
        ((-0.5 * M, 0.7 * M, -0.9 * M), (4.4 * M, 0.8 * M, 0.08 * M), RUST),
        ((3.0 * M, 0.75 * M, 0), (0.08 * M, 0.9 * M, 1.7 * M), RUST),               # the bow
        ((-1.6 * M, 0.95 * M, 0.2 * M), (0.08 * M, 0.5 * M, 1.2 * M), RUST),        # a rib
        ((0.6 * M, 0.95 * M, -0.1 * M), (0.08 * M, 0.45 * M, 1.4 * M), RUST),       # another
    ])
    # the camp's old slipway: broken concrete, mossy, nearly buried
    MOSSY = mat((0.27, 0.31, 0.24), extra={"roughness": 0.95})
    for (x, z, sx, sz, rx, ry, rz) in ((700, -385, 2.4, 1.6, 12, 25, -8), (688, -398, 1.8, 1.4, -18, 40, 15),
                                       (712, -372, 1.4, 1.2, 6, 10, 22)):
        ents.append(box((x, lagoon_ground(x, z) - 0.05 * M, z), (sx * M, 0.3 * M, sz * M), MOSSY, True, rot=(rx, ry, rz)))
    return ents

CRATER_RIM = mat((0.24, 0.18, 0.13), extra={"roughness": 0.95})
CRATER_PIT = mat((0.015, 0.013, 0.012), extra={"roughness": 1.0})

def crater(x, z, r):
    """where a piece of the Moon came down: a pit (dark) in a rim of thrown-up
    earth, rocks flung round it (the terrain is one mesh: it can't be dug)"""
    gy = lagoon_ground(x, z)
    out = [entity("res/models/sphere.obj", (x, gy - 0.05 * r, z), (r * 1.25, r * 0.32, r * 1.25), material=CRATER_RIM),
           entity("res/models/sphere.obj", (x, gy + 0.24 * r, z), (r * 0.8, r * 0.04, r * 0.8), material=CRATER_PIT,
                  cast_shadow=False)]
    rng = random.Random(int(x * 7 + z * 13))
    for k in range(7):
        a = rng.uniform(0, 2 * math.pi); d = r * rng.uniform(1.4, 2.6)
        rx_, rz_ = x + math.cos(a) * d, z + math.sin(a) * d
        out.append(entity("res/models/rock.obj", (rx_, lagoon_ground(rx_, rz_), rz_), rng.uniform(1.5, 4.0),
                          (rng.uniform(0, 360), rng.uniform(0, 360), 0), RUSTY))
    return out

def storm_damage(g):
    """2038 to 4000. What the 2038 storm wrecked has had two thousand years:
    wood rotted, steel rusted away, stone and concrete weathered and mossy, a
    young forest grown back. Enough is left to know the place from the archive
    footage: the island and its statue, the dragon, the lighthouse (a broken,
    dark tower), stubs of the lamps, a few fence panels. And the close pass's
    own damage, fresh: trees just blown down, craters where rubble from the
    black hole's dust ring came down"""
    rng = random.Random(4000)
    def near(pos, x, z, r):
        return math.hypot(pos[0] - x, pos[2] - z) < r
    # no power for two thousand years: only the campfire (yours, from earlier runs)
    g["Lights"] = [l for l in g["Lights"]
                   if l["Attributes"]["type"] == "DIR" or near(l["Transform"]["position"], *CAMPFIRE[0::2], 30)]
    def powered(p):
        spots = p.get("positions") or [p.get("position") or [0, 0, 0]]
        return any(near(q, 500, -775, 150) or near(q, 515.4, -1214.7, 30) or (q[0] in (452.0, 528.0)) for q in spots)
    g["ParticleEmitters"] = [p for p in g.get("ParticleEmitters", []) if not powered(p)]
    MOSS_BRONZE = mat((0.3, 0.4, 0.3), extra={"roughness": 0.85, "metallic": 0.2})
    WEATHERED = mat((0.42, 0.4, 0.37), extra={"roughness": 0.95})
    RUSTED = mat((0.28, 0.15, 0.08), extra={"roughness": 0.95, "metallic": 0.2})
    lamps = 0
    kept = []
    for e in g["Entities"]:
        rc = e["Components"].get("RenderComponent", {})
        mesh, t = rc.get("Mesh", ""), e["Transform"]
        pos = t["position"]
        if mesh == "res/models/cube4.obj" and abs(t["scale"][1] - 30) < 1:              # the lamp posts
            lamps += 1
            if lamps % 2: continue                                                      # rusted away
            gy = lagoon_ground(pos[0], pos[2])
            t["scale"] = [1.6, 12, 1.6]                                                 # a stub, rusted, leaning
            t["position"] = [pos[0], gy + 4, pos[2]]
            t["rotation"] = [rng.uniform(-18, 18), 0, rng.uniform(-18, 18)]
            rc["Material"] = RUSTED
        elif mesh == "res/models/sphere.obj" and (abs(t["scale"][0] - 3.2) < 0.1 or near(pos, 515.4, -1214.7, 5)):
            continue                                                                    # the lamps' globes, the lighthouse's lamp
        elif mesh == "res/models/cube4.obj" and near(pos, 515.4, -1214.7, 5):           # the lighthouse: broken off, dark
            top = pos[1] + t["scale"][1] / 2
            t["scale"] = [t["scale"][0], t["scale"][1] * 0.7, t["scale"][2]]
            t["position"] = [pos[0], pos[1] - t["scale"][1] * 0.3 / 0.7 / 2, pos[2]]
            rc["Material"] = WEATHERED
            for k in range(5):                                                          # its top, fallen
                a = rng.uniform(0, 2 * math.pi); r = rng.uniform(14, 40)
                x, z = pos[0] + math.cos(a) * r, pos[2] + math.sin(a) * r
                kept.append(box((x, lagoon_ground(x, z) + 2, z), (rng.uniform(6, 12), rng.uniform(4, 8), rng.uniform(6, 12)),
                                WEATHERED, True, rot=(rng.uniform(-30, 30), rng.uniform(0, 90), rng.uniform(-30, 30))))
        elif mesh == "res/models/dragon.obj":                                           # the dragon: toppled, mossy, sinking
            t["rotation"] = [0, 90, 75]
            t["position"] = [pos[0], pos[1] - 12, pos[2]]
            rc["Material"] = mat((0.2, 0.33, 0.24), extra={"roughness": 0.8})
        elif mesh == "res/models/buddha.obj":                                           # the island's statue: tilted, green
            t["rotation"] = [10, 0, -7]
            rc["Material"] = MOSS_BRONZE
        kept.append(e)
    g["Entities"] = kept
    for e in g["Entities"]:
        ic = e["Components"].get("InstanceRenderComponent")
        if ic and "fence3.obj" in ic["Mesh"]:                                           # a few fence panels, fallen
            ic["Transforms"] = [dict(f, rotation=[rng.uniform(80, 88), f["rotation"][1] + rng.uniform(-30, 30), 0],
                                     position=[f["position"][0], f["position"][1] - 2, f["position"][2]])
                                for f in ic["Transforms"] if rng.random() < 0.25]
        elif ic and "tree" in ic["Mesh"]:
            for t in ic["Transforms"]:                                                  # the close pass: trees just blown down
                fate = rng.random()
                if fate < 0.3:
                    t["rotation"] = [rng.uniform(82, 88), rng.uniform(0, 360), 0]
                    t["position"][1] -= 0.5 * M
                elif fate < 0.45:
                    t["rotation"] = [rng.uniform(25, 40), rng.uniform(0, 360), 0]
            young = []                                                                  # the forest grown back
            while len(young) < 22:
                x, z = rng.uniform(-50, 1050), rng.uniform(-1300, -260)
                y = lagoon_ground(x, z)
                if y is None or not (-2 < y < 130): continue
                if near((x, 0, z), *CAMPFIRE[0::2], 70) or near((x, 0, z), *HILL, 90) or near((x, 0, z), 675, -410, 60):
                    continue
                young.append({"position": [round(x, 1), round(y - 1, 1), round(z, 1)],
                              "rotation": [0, round(rng.uniform(0, 360), 1), 0], "scale": [round(rng.uniform(7, 11), 2)] * 3})
            ic["Transforms"] += young
    for (x, z, r) in ((735, -820, 34), (760, -980, 26), (690, -300, 30), (880, -760, 22), (620, -880, 18),
                      (560, -470, 24)):
        g["Entities"] += crater(x, z, r)
    # the island: its rock, all the way down to the lake bed now the water is gone
    island = next(e for e in g["Entities"] if e["Components"].get("RenderComponent", {}).get("Mesh") == "res/models/rock.obj")
    g["Entities"].append(entity("res/models/rock.obj", (500.0, -78.0, -775.0), (36.0, 62.0, 36.0), (0, 80, 0),
                                copy.deepcopy(island["Components"]["RenderComponent"].get("Material"))))
    g["Water"]["softEdge"] = 14                         # the pool's edge on the coarse terrain: soft, not polygonal

_b = (-LAGOON_MOON[0], -LAGOON_MOON[2])
HILL_HATCH = (HILL[0] + 45 * _b[0] / math.hypot(*_b), HILL[1] + 45 * _b[1] / math.hypot(*_b))
g = year4000(LAGOON, 0.0, LAGOON_MOON, HOLE_LAGOON,
            start(HILL[0], HILL_Y + 28, HILL[1], yaw_towards(HILL, (HILL[0] + LAGOON_MOON[0], HILL[1] + LAGOON_MOON[2])), 12))
g["Entities"] += hatch(HILL_HATCH[0], lagoon_ground(*HILL_HATCH), HILL_HATCH[1])
g["Water"]["height"] = WATER_LAGOON
WRECKAGE = drained_lagoon()
g["Entities"] += WRECKAGE
storm_damage(g)
for k in range(6):
    a = random.uniform(0, 2 * math.pi); r = random.uniform(14, 32)          # (within the firelight)
    x, z = CAMPFIRE[0] + math.cos(a) * r, CAMPFIRE[2] + math.sin(a) * r
    # caught in the grass, tilted by the wind (flat on the ground the grass hides them)
    g["Entities"].append(box((x, lagoon_ground(x, z) + random.uniform(4, 7), z), (0.21 * M, 0.06, 0.3 * M), PAPER,
                             rot=(random.uniform(35, 70), random.uniform(0, 360), random.uniform(-15, 15)), cast_shadow=False))
x, z = CAMPFIRE[0] + 30, CAMPFIRE[2] - 20
g["Entities"].append(box((x, lagoon_ground(x, z) + 3, z), (12, 6, 9), mat((0.3, 0.25, 0.2), extra={"roughness": 0.9})))  # a pack
scenes['05_lagoon'] = g

# --------------------------------------------------------------------------
# 06 the last dawn, on the hill above the lagoon: the black hole fills the
# sky low over the sea, the hatch open behind you. Then white, and you wake in
# Room 7 again (01_room7): the game's one ending
# --------------------------------------------------------------------------
_d = (-0.25, 0.3, -0.92)
DAWN_DIR = [round(c / math.sqrt(sum(v * v for v in _d)), 4) for c in _d]
g = year4000(LAGOON, 0.235, DAWN_DIR, 0.1,
            start(HILL[0], HILL_Y + 28, HILL[1], yaw_towards(HILL, (HILL[0] + DAWN_DIR[0], HILL[1] + DAWN_DIR[2])), 12))
g["Entities"] += hatch(HILL_HATCH[0], lagoon_ground(*HILL_HATCH), HILL_HATCH[1])
g["Water"]["height"] = WATER_DAWN
g["Entities"] += copy.deepcopy(WRECKAGE)
storm_damage(g)
scenes['06_dawn'] = g

# --------------------------------------------------------------------------
os.makedirs(OUT, exist_ok=True)
for name, game in scenes.items():
    path = os.path.join(OUT, name + '.json')
    # which timeline files the map plays is kept as the map has it: tools/timeline_editor.py
    # changes that list ("Save as", "Add a timeline file", "Play this one instead")
    if os.path.exists(path):
        playing = json.load(open(path)).get('Sequences')
        if playing:
            game['Sequences'] = playing
    json.dump(game, open(path, 'w'), indent=1)
    print('%-28s entities %3d lights %2d' % (path, len(game["Entities"]), len(game["Lights"])))
