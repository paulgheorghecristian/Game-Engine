# Generates "Sunset Canyon": a red-rock desert canyon whose winding valley
# leads to a ruined sanctum on a cliff-edge overlook. A bronze colossus stands
# at the edge, backlit by the sun setting over a hazy basin; at night braziers,
# a crystal altar with a beam of light into the sky, and lanterns drifting up
# over the basin take over.
#
# Writes res/models/canyon_terrain.obj (the procedural terrain mesh, also used
# for collision) and Game/canyon.json.
#
# Usage (from the repository root):
#   python3 tools/make_desert_textures.py   # once: sand / sandstone textures
#   python3 tools/make_canyon.py
#   ./ge Game/canyon.json
import json, math, random, sys

random.seed(5)

# --------------------------------------------------------------------------
# noise
# --------------------------------------------------------------------------
def _hash(ix, iz, seed):
    h = (ix * 374761393 + iz * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFFFF) / float(0xFFFFFF)

def noise(x, z, seed=0):
    ix, iz = math.floor(x), math.floor(z)
    fx, fz = x - ix, z - iz
    ux, uz = fx * fx * (3 - 2 * fx), fz * fz * (3 - 2 * fz)
    a = _hash(ix, iz, seed); b = _hash(ix + 1, iz, seed)
    c = _hash(ix, iz + 1, seed); d = _hash(ix + 1, iz + 1, seed)
    return (a + (b - a) * ux) + ((c + (d - c) * ux) - (a + (b - a) * ux)) * uz

def fbm(x, z, octaves=4, seed=0):
    t, amp, norm = 0.0, 1.0, 0.0
    for o in range(octaves):
        t += noise(x, z, seed + o * 17) * amp
        norm += amp; amp *= 0.5; x *= 2.03; z *= 2.03
    return t / norm

def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)

# --------------------------------------------------------------------------
# layout
# --------------------------------------------------------------------------
X0, X1, Z0, Z1, STEP = -1800.0, 1800.0, -3000.0, 1500.0, 15.0
SANCTUM = (0.0, -800.0)        # plaza centre
SANCTUM_R = 270.0              # amphitheatre radius
EDGE_Z = -1065.0               # overlook edge (cliff down to the basin)
BASIN_Y = -175.0
SPAWN = (40.0, 820.0)

# valley centre line (Catmull-Rom through these points), spawn -> sanctum
CONTROL = [(40, 1050), (40, 820), (170, 560), (-60, 260), (120, -60), (-30, -330), (0, -560), (0, -800)]

def catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return tuple(0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2 +
                        (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3) for i in range(2))

PATH = []
for k in range(1, len(CONTROL) - 2):
    for s in range(24):
        PATH.append(catmull(CONTROL[k - 1], CONTROL[k], CONTROL[k + 1], CONTROL[k + 2], s / 24.0))
PATH.append(CONTROL[-2])

def dist_path(x, z):
    best = 1e9
    for (ax, az), (bx, bz) in zip(PATH, PATH[1:]):
        dx, dz = bx - ax, bz - az
        t = max(0.0, min(1.0, ((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz)))
        px, pz = ax + dx * t - x, az + dz * t - z
        d = px * px + pz * pz
        if d < best: best = d
    return math.sqrt(best)

BUTTES = [(-520, -1520, 115, 235), (330, -1700, 85, 300), (930, -1380, 150, 175),
          (-1150, -1300, 210, 150), (120, -2350, 160, 260), (-300, -2100, 90, 200)]

def raw_height(x, z):
    # valley / amphitheatre: distance outside the open ground
    width = 95 + 40 * (fbm(x / 260, z / 260, 3, 3) - 0.5)
    radius = SANCTUM_R + 30 * (fbm(x / 200, z / 200, 2, 7) - 0.5)
    d_open = min(dist_path(x, z) - width, math.hypot(x - SANCTUM[0], z - SANCTUM[1]) - radius)

    dunes = 5 * (fbm(x / 240, z / 240, 3, 11) - 0.5) + 1.5 * (noise(x / 45, z / 45, 13) - 0.5)
    in_sanctum = math.hypot(x - SANCTUM[0], z - SANCTUM[1]) < radius - 30
    floor_y = 0.0 if in_sanctum else dunes * smoothstep(-10, -60, -d_open)

    # terraced canyon walls: a first cliff, a ledge, a second cliff, then
    # uneven mesa tops
    t = d_open + 12 * (fbm(x / 90, z / 90, 3, 21) - 0.5)
    wall = 92 * smoothstep(-4, 42, t) + 115 * smoothstep(70, 128, t)
    wall += 55 * (fbm(x / 650, z / 650, 4, 31) - 0.5) * smoothstep(100, 170, t)
    wall += 8 * (fbm(x / 60, z / 60, 2, 41) - 0.5) * smoothstep(0, 40, t)
    canyon = floor_y + wall

    # the basin beyond the overlook (sharp escarpment), with buttes and far ridges
    edge = EDGE_Z + 45 * (fbm(x / 300, 0.5, 3, 51) - 0.5) * smoothstep(140, 420, abs(x))
    drop = smoothstep(edge + 10, edge - 30, z)
    basin = BASIN_Y + 8 * (fbm(x / 300, z / 300, 3, 61) - 0.5)
    for bx, bz, br, bh in BUTTES:
        d = math.hypot(x - bx, z - bz) + 25 * (fbm(x / 70, z / 70, 2, 71) - 0.5)
        basin += bh * smoothstep(br + 30, br, d) * (0.8 + 0.2 * smoothstep(br * 0.8, br * 0.3, d))
    basin += 260 * smoothstep(-2500, -2950, z) * (0.7 + 0.6 * fbm(x / 500, 0.3, 3, 81))
    return canyon + (basin - canyon) * drop

# reflecting pool basin (under the water; the rim and paving hide the edges)
POOL_X, POOL_Z0, POOL_Z1 = 25.0, -940.0, -660.0
WATER_Y = 0.8

def height_at_vertex(x, z):
    if abs(x) <= POOL_X + 12 and POOL_Z0 - 12 <= z <= POOL_Z1 + 12:
        return -9.0
    return raw_height(x, z)

# --------------------------------------------------------------------------
# terrain mesh
# --------------------------------------------------------------------------
NX = int(round((X1 - X0) / STEP)) + 1
NZ = int(round((Z1 - Z0) / STEP)) + 1
HEIGHTS = [[height_at_vertex(X0 + i * STEP, Z0 + j * STEP) for i in range(NX)] for j in range(NZ)]

def H(x, z):
    """height of the terrain mesh (same triangulation as the OBJ)"""
    fi, fj = (x - X0) / STEP, (z - Z0) / STEP
    i, j = max(0, min(NX - 2, int(fi))), max(0, min(NZ - 2, int(fj)))
    u, v = fi - i, fj - j
    h00, h10, h01, h11 = HEIGHTS[j][i], HEIGHTS[j][i + 1], HEIGHTS[j + 1][i], HEIGHTS[j + 1][i + 1]
    if u + v <= 1.0:                      # triangle (i,j) (i,j+1) (i+1,j)
        return h00 + (h10 - h00) * u + (h01 - h00) * v
    return h11 + (h01 - h11) * (1 - u) + (h10 - h11) * (1 - v)

def write_obj(path):
    with open(path, 'w') as f:
        f.write('# Sunset Canyon terrain, generated by tools/make_canyon.py\no canyon\n')
        for j in range(NZ):
            for i in range(NX):
                f.write('v %.2f %.2f %.2f\n' % (X0 + i * STEP, HEIGHTS[j][i], Z0 + j * STEP))
        for j in range(NZ):
            for i in range(NX):
                f.write('vt %.4f %.4f\n' % (i * STEP / 60.0, j * STEP / 60.0))
        for j in range(NZ):
            for i in range(NX):
                hl = HEIGHTS[j][max(i - 1, 0)]; hr = HEIGHTS[j][min(i + 1, NX - 1)]
                hd = HEIGHTS[max(j - 1, 0)][i]; hu = HEIGHTS[min(j + 1, NZ - 1)][i]
                nx, ny, nz = hl - hr, 2 * STEP, hd - hu
                n = math.sqrt(nx * nx + ny * ny + nz * nz)
                f.write('vn %.4f %.4f %.4f\n' % (nx / n, ny / n, nz / n))
        for j in range(NZ - 1):
            for i in range(NX - 1):
                a = j * NX + i + 1; b = (j + 1) * NX + i + 1; c = j * NX + i + 2; d = (j + 1) * NX + i + 2
                f.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (a, a, a, b, b, b, c, c, c))
                f.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (c, c, c, b, b, b, d, d, d))

TERRAIN_OBJ = 'res/models/canyon_terrain.obj'
write_obj(TERRAIN_OBJ)

# --------------------------------------------------------------------------
# helpers (same conventions as make_lagoon.py)
# --------------------------------------------------------------------------
def mat(diffuse, texture=None, normal=None, rough=None, emissive=0.0, shininess=20.0, extra=None):
    m = {"ambient": [0, 0, 0], "diffuse": list(diffuse), "specular": [0.2, 0.2, 0.2], "shininess": shininess}
    if texture: m["Texture"] = texture
    if normal: m["NormalMapTexture"] = normal
    if rough: m["RoughnessTexture"] = rough
    if emissive: m["emissive"] = emissive
    if extra: m.update(extra)
    return m

def entity(mesh, pos, scale, rot=(0, 0, 0), material=None, physics=None, cast_shadow=True, extra_components=None):
    rc = {"Mesh": mesh}
    if not cast_shadow: rc["castShadow"] = False
    if material: rc["Material"] = material
    comps = {"RenderComponent": rc}
    if physics: comps["PhysicsComponent"] = physics
    if extra_components: comps.update(extra_components)
    return {"Transform": {"position": [round(v, 2) for v in pos],
                          "rotation": [round(v, 2) for v in rot],
                          "scale": [round(v, 3) for v in (scale if isinstance(scale, (list, tuple)) else (scale,) * 3)]},
            "Components": comps}

def box_physics(size):
    return {"BoundingBodyType": ["cube"], "scale": [round(s, 2) for s in size], "mass": 0}

def tr(pos, scale, rot=(0, 0, 0)):
    return {"position": [round(v, 2) for v in pos], "rotation": [round(v, 2) for v in rot],
            "scale": [round(v, 3) for v in (scale if isinstance(scale, (list, tuple)) else (scale,) * 3)]}

def instanced(mesh, transforms, material=None, wind=0.0):
    irc = {"Mesh": mesh, "Transforms": transforms}
    if material: irc["Material"] = material
    if wind: irc["wind"] = wind          # sway (trees): bends more higher up
    return {"Transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
            "Components": {"InstanceRenderComponent": irc}}

def aim(frm, to):
    d = [to[i] - frm[i] for i in range(3)]
    n = math.sqrt(sum(c * c for c in d)); d = [c / n for c in d]
    return (round(math.degrees(math.asin(d[1])), 2), round(math.degrees(math.atan2(-d[0], -d[2])), 2), 0)

def light(pos, color, kind, scale, rot=(0, 0, 0), **attrs):
    a = {"color": list(color), "type": kind}
    a.update(attrs)
    return {"Transform": {"position": [round(v, 2) for v in pos], "rotation": list(rot),
                          "scale": list(scale)}, "Attributes": a}

entities, lights, emitters = [], [], []

SANDSTONE = mat((0.95, 0.9, 0.84), "res/textures/desert_sand2.png", "res/textures/desert_sand2_normal.png",
                extra={"roughness": 0.85, "specularStrength": 0.3})
PAVING = mat((0.95, 0.85, 0.72), "res/textures/196.bmp", "res/textures/196_norm.bmp",
             extra={"roughness": 0.8, "specularStrength": 0.35, "normalMapStrength": 0.45})
RED_ROCK = mat((1.0, 0.68, 0.5), "res/textures/rock2k.jpg", "res/textures/rock2k_normal.jpg",
               extra={"roughness": 0.9, "specularStrength": 0.25})
# weathered bronze with a patina: partly metallic so it still takes the
# uplights (pure metal has no diffuse and turns black under spot lights)
BRONZE = mat((0.62, 0.5, 0.38), shininess=60, extra={"metallic": 0.35, "roughness": 0.5})
FIRE_BOWL = mat((1.0, 0.5, 0.15), emissive=1.0)
CRYSTAL = mat((0.45, 0.9, 1.0), emissive=1.0, extra={"emissiveNightOnly": False})

# --------------------------------------------------------------------------
# terrain
# --------------------------------------------------------------------------
entities.append({"Transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
                 "Components": {"RenderComponent": {"Mesh": TERRAIN_OBJ,
                                                    "Material": mat((1.0, 1.0, 1.0), shininess=100, extra={
                                                        "Terrain": {"grass": "res/textures/desert_sand.png",
                                                                    "grass2": "res/textures/desert_sand2.png",
                                                                    "rock": "res/textures/desert_sandstone.png",
                                                                    "rockNormal": "res/textures/desert_sandstone_normal.png",
                                                                    "dirt": "res/textures/terrain_dirt.bmp",
                                                                    "soil": "res/textures/desert_sand2.png",
                                                                    "grassNormal": "res/textures/desert_sand_normal.png",
                                                                    "dirtNormal": "res/textures/terrain_dirt_normal.png",
                                                                    "soilNormal": "res/textures/desert_sand2_normal.png",
                                                                    "normalStrength": 0.9,
                                                                    "tileSize": 70,
                                                                    # no shoreline: the only water is the pool
                                                                    "shoreHeight": -1000}})},
                                "PhysicsComponent": {"BoundingBodyType": ["mesh", TERRAIN_OBJ],
                                                     "scale": [1, 1, 1], "mass": 0}}})

# --------------------------------------------------------------------------
# sanctum: paving, reflecting pool, colonnade, gate
# --------------------------------------------------------------------------
TOP = 1.5                                  # paving surface height
tiles = []
xs_side = [41 + 20 * k for k in range(5)]  # 41 .. 121
for zc in range(-604, -1005, -20):
    row_x = [-x for x in xs_side] + xs_side
    if zc > POOL_Z1 + 6 or zc < POOL_Z0 - 6:
        row_x += [-20, 0, 20]
    for xc in row_x:
        outer = abs(xc) > 100 or zc < -990 or zc > -610
        if outer and random.random() < 0.12:
            continue                       # missing tiles at the ruined fringe
        sink = random.uniform(0.3, 1.2) if outer and random.random() < 0.3 else random.uniform(0, 0.15)
        tiles.append(tr((xc, TOP - 1 - sink, zc), (20.4 if abs(xc) <= 20 else 20, 2, 20),
                        (random.uniform(-1.5, 1.5) if outer else 0, 90 * random.randrange(4), 0)))
entities.append(instanced("res/models/cube4.obj", tiles, PAVING))
# walkable: one collision slab under the paving
entities.append({"Transform": {"position": [0, TOP - 1, -804], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
                 "Components": {"PhysicsComponent": box_physics((262, 2, 420))}})

# pool rim (tall blocks hide the carved basin edge)
rim = []
RIM_W, RIM_TOP = 6.0, 2.6
for z in range(int(POOL_Z1) - 10, int(POOL_Z0), -20):
    for side in (-1, 1):
        rim.append(tr((side * (POOL_X + RIM_W / 2), RIM_TOP - 6, z), (RIM_W, 12, 20.2)))
for side in (-1, 1):
    z = POOL_Z1 + RIM_W / 2 if side > 0 else POOL_Z0 - RIM_W / 2
    rim.append(tr((0, RIM_TOP - 6, z), (2 * POOL_X + 2 * RIM_W, 12, RIM_W)))
entities.append(instanced("res/models/cube4.obj", rim, SANDSTONE))

# colonnade: two rows of square pillars, some broken, some fallen
COL_X, COL_H = 64.0, 58.0
COL_Z = [-632 - 58 * k for k in range(7)]
pillars, caps = [], []
broken = {(-1, 2): 31.0, (1, 4): 18.0, (-1, 6): 40.0}
fallen = {(1, 5)}
for side in (-1, 1):
    for k, z in enumerate(COL_Z):
        x = side * COL_X
        caps.append(tr((x, TOP + 2, z), (14, 4, 14)))                     # base
        if (side, k) in fallen:
            # lying across the paving, toppled outwards
            # the drums of a toppled pillar, scattered outwards
            for d in range(5):
                pillars.append(tr((x + side * (14 + d * 10.5), TOP + 4.0 + random.uniform(-0.6, 0.3), z + d * 2.5 + random.uniform(-2, 2)),
                                  (9, 9.4, 9), (random.uniform(-8, 8), 12 + random.uniform(-15, 15), 90)))
            entities.append({"Transform": {"position": [x + side * 34, TOP + 4.5, z + 6], "rotation": [0, 12, 90],
                                           "scale": [1, 1, 1]},
                             "Components": {"PhysicsComponent": box_physics((9, COL_H - 4, 9))}})
            continue
        h = broken.get((side, k), COL_H)
        drums = max(1, int(round(h / 9.7)))
        for d in range(drums):
            dh = h / drums
            pillars.append(tr((x + random.uniform(-0.3, 0.3), TOP + 4 + dh * (d + 0.5), z + random.uniform(-0.3, 0.3)),
                              (9, dh - 0.25, 9), (0, random.uniform(-6, 6), 0)))
        entities.append({"Transform": {"position": [x, TOP + 4 + h / 2, z], "rotation": [0, 0, 0], "scale": [1, 1, 1]},
                         "Components": {"PhysicsComponent": box_physics((9, h, 9))}})
        if h == COL_H:
            caps.append(tr((x, TOP + 4 + h + 2.5, z), (15, 5, 15)))      # capital
# architraves over the intact spans
for side, k in ((-1, 0), (-1, 4), (1, 0), (1, 1), (1, 2)):
    if (side, k) in broken or (side, k + 1) in broken or (side, k) in fallen or (side, k + 1) in fallen:
        continue
    z = (COL_Z[k] + COL_Z[k + 1]) / 2
    caps.append(tr((side * COL_X, TOP + 4 + COL_H + 8, z), (12, 6, 70)))
# a fallen architrave block
caps.append(tr((-COL_X + 22, TOP + 3, -800), (12, 6, 50), (8, 35, 0)))
entities.append(instanced("res/models/cube4.obj", pillars, SANDSTONE))
entities.append(instanced("res/models/cube4.obj", caps, SANDSTONE))

# gate at the plaza entrance
GATE_Z = -592.0
gate = []
for side in (-1, 1):
    for d in range(7):
        gate.append(tr((side * 44, TOP + 72 / 7 * (d + 0.5), GATE_Z), (13, 72 / 7 - 0.3, 13), (0, random.uniform(-3, 3), 0)))
    entities.append({"Transform": {"position": [side * 44, TOP + 36, GATE_Z], "rotation": [0, 0, 0], "scale": [1, 1, 1]},
                     "Components": {"PhysicsComponent": box_physics((13, 72, 13))}})
for d in range(4):
    gate.append(tr((-42 + d * 28, TOP + 77, GATE_Z), (27.7, 10, 16)))
entities.append(instanced("res/models/cube4.obj", gate, SANDSTONE))

# --------------------------------------------------------------------------
# colossus on the overlook, altar with a floating crystal
# --------------------------------------------------------------------------
PED = (0.0, -1022.0)
ped_h = 26.0
entities.append(entity("res/models/cube4.obj", (PED[0], ped_h / 2, PED[1]), (64, ped_h, 52), material=SANDSTONE,
                       physics=box_physics((64, ped_h, 52))))
entities.append(entity("res/models/cube4.obj", (PED[0], ped_h + 2, PED[1]), (52, 4, 42), material=SANDSTONE))
COL_SCALE = 8.6
colossus_y = ped_h + 4 + 0.06 * COL_SCALE
entities.append(entity("res/models/NF.obj", (PED[0], colossus_y, PED[1]), COL_SCALE, (0, 0, 0), BRONZE))

ALTAR = (0.0, -968.0)
entities.append(entity("res/models/cube4.obj", (ALTAR[0], TOP + 5, ALTAR[1]), (22, 10, 22), material=SANDSTONE,
                       physics=box_physics((22, 10, 22))))
entities.append(entity("res/models/cube4.obj", (ALTAR[0], TOP + 11, ALTAR[1]), (16, 2, 16), material=SANDSTONE))
CRYSTAL_Y = TOP + 27
entities.append(entity("res/models/sphere.obj", (ALTAR[0], CRYSTAL_Y, ALTAR[1]), (3.6, 9.5, 3.6), (0, 0, 18),
                       CRYSTAL, cast_shadow=False,
                       extra_components={"TempAnimationComponent": {"addRotation": [0, 0.6, 0]}}))

# --------------------------------------------------------------------------
# braziers
# --------------------------------------------------------------------------
# (the gate pair stands well inside the gate, away from its pillars)
BRAZIERS = [(side * 44, z) for side in (-1, 1) for z in (-661, -777, -893)] + [(-22, -625), (22, -625)]
brazier_tops = []
for bx, bz in BRAZIERS:
    entities.append(entity("res/models/stone2.obj", (bx, TOP, bz), (6.5, 6.0, 6.5), material=BRONZE))
    top = TOP + 1.71 * 6.0
    entities.append(entity("res/models/sphere.obj", (bx, top + 0.6, bz), (4.6, 1.5, 4.6), material=FIRE_BOWL,
                           cast_shadow=False))
    brazier_tops.append((bx, top + 1.5, bz))
    lights.append(light((bx, top + 9, bz), (1.0, 0.62, 0.34), "POINT", (180, 180, 180),
                        cast_shadow=True, shadowMapSize=512, diffuse_attenuation=[1300, 10, 7],
                        specular_attenuation=[90, 10, 2], flicker=0.3, flickerSpeed=9 + len(lights) % 4))
    emitters += [
        {"type": "smoke", "texture": "res/textures/fire-texture-atlas.jpg", "atlas": [4, 4], "atlasTopDown": True,
         "count": 18, "lifetimeMs": 700, "position": [bx, top + 1.5, bz], "radius": 2.2, "direction": [0, 1, 0],
         "spread": 0.12, "speed": [10, 18], "gravity": 0, "buoyancy": 0.4, "startScale": 8, "growth": -4,
         "blend": "additive", "tint": [1.0, 0.82, 0.62], "brightness": 0.0},
        {"type": "spray", "texture": "res/textures/glowWhite.png", "count": 22, "lifetimeMs": 2400,
         "position": [bx, top + 3, bz], "radius": 2, "direction": [0, 1, 0], "spread": 0.4, "speed": [18, 40],
         "gravity": -5, "startScale": 1.1, "blend": "additive", "tint": [1.0, 0.55, 0.2]},
    ]

# --------------------------------------------------------------------------
# the valley: standing stones with torches, boulders at the cliff feet
# --------------------------------------------------------------------------
def path_point(t):
    """point and direction along the valley centre line, t in [0, 1]"""
    k = min(int(t * (len(PATH) - 1)), len(PATH) - 2)
    a, b = PATH[k], PATH[k + 1]
    d = (b[0] - a[0], b[1] - a[1]); n = math.hypot(*d)
    return a, (d[0] / n, d[1] / n)

stones, torches = [], []
for i in range(1, 15):
    (px, pz), (dx, dz) = path_point(i / 15.0 * 0.86)
    if pz < -560:
        break
    side = 1 if i % 2 else -1
    sx, sz = px - dz * side * 55, pz + dx * side * 55
    stones.append(tr((sx, H(sx, sz) - 1, sz), (8.5, 9.5, 8.5), (random.uniform(-4, 4), random.uniform(0, 360), random.uniform(-3, 3))))
    if i % 2 == 1:
        torches.append((sx - dz * side * -9, H(sx, sz) + 19, sz + dx * side * -9))
entities.append(instanced("res/models/stone2.obj", stones, SANDSTONE))
for tx, ty, tz in torches:
    entities.append(entity("res/models/cube4.obj", (tx, ty - 10, tz), (1.3, 18, 1.3), material=BRONZE))
    entities.append(entity("res/models/sphere.obj", (tx, ty, tz), 1.8, material=FIRE_BOWL, cast_shadow=False))
    lights.append(light((tx, ty + 3, tz), (1.0, 0.55, 0.22), "POINT", (190, 190, 190), cast_shadow=False,
                        diffuse_attenuation=[600, 10, 3.6], specular_attenuation=[90, 10, 2],
                        flicker=0.25, flickerSpeed=7, nightOnly=True))
    emitters.append({"type": "smoke", "texture": "res/textures/fire-texture-atlas.jpg", "atlas": [4, 4],
                     "atlasTopDown": True, "count": 10, "lifetimeMs": 600, "position": [tx, ty + 1, tz],
                     "radius": 0.8, "direction": [0, 1, 0], "spread": 0.1, "speed": [7, 12], "gravity": 0,
                     "buoyancy": 0.4, "startScale": 4.5, "growth": -3, "blend": "additive",
                     "tint": [1.0, 0.8, 0.6], "brightness": 0.0})

boulders = []
for i in range(130):
    (px, pz), (dx, dz) = path_point(random.random() * 0.97)
    side = random.choice((-1, 1))
    off = random.uniform(55, 100)
    bx, bz = px - dz * side * off, pz + dx * side * off
    if H(bx, bz) > 12:
        continue                           # on the cliff face: skip
    if math.hypot(bx - SANCTUM[0], bz - SANCTUM[1]) < SANCTUM_R - 20:
        continue
    s = random.uniform(5, 16) if random.random() < 0.85 else random.uniform(18, 30)
    boulders.append(tr((bx, H(bx, bz) - s * 0.35, bz), (s, s * random.uniform(0.6, 1.0), s),
                       (random.uniform(-20, 20), random.uniform(0, 360), random.uniform(-15, 15))))
# around the amphitheatre rim
for a in range(0, 360, 7):
    r = SANCTUM_R + random.uniform(-25, 5)
    bx, bz = SANCTUM[0] + math.cos(math.radians(a)) * r, SANCTUM[1] + math.sin(math.radians(a)) * r
    if bz < EDGE_Z + 60 or abs(bx) < 80 and bz > -640:
        continue                           # keep the overlook and the gate clear
    s = random.uniform(6, 20)
    boulders.append(tr((bx, H(bx, bz) - s * 0.35, bz), s, (random.uniform(-20, 20), random.uniform(0, 360), 0)))
entities.append(instanced("res/models/rock.obj", boulders, RED_ROCK))

# a few hardy trees near the sanctum gate
trees = []
for (tx, tz) in ((-150, -520), (175, -470), (-120, -420), (150, -600), (-210, -650), (230, -760)):
    trees.append(tr((tx, H(tx, tz) - 1, tz), random.uniform(10, 14), (0, random.uniform(0, 360), 0)))
entities.append(instanced("res/models/tree.obj", trees, wind=0.6))

# --------------------------------------------------------------------------
# lights
# --------------------------------------------------------------------------
moon = [0.86, 0.45, 0.2]       # the moon rises in the east; the sun sets almost due north, behind the colossus
lights.append(light((0, 0, 0), (0.22, 0.26, 0.38), "DIR", (0, 0, 0), dir=moon, cast_shadow=True,
                    diffuse_attenuation=[0, 0, 0], specular_attenuation=[0, 0, 0]))
# crystal glow
lights.append(light((ALTAR[0], CRYSTAL_Y, ALTAR[1] + 4), (0.4, 0.85, 1.0), "POINT", (260, 260, 260),
                    cast_shadow=True, shadowMapSize=512, diffuse_attenuation=[500, 10, 3],
                    specular_attenuation=[90, 10, 2], flicker=0.12, flickerSpeed=2))
# two crossed volumetric beams behind the colossus: lamps behind it on either
# side aim up past its shoulders and cross above its head, so the statue
# stands in silhouette against the beams (which rim-light it) instead of the
# beams passing in front of it; a warm key light from the plaza lights its front
BEAM_ANGLE, BEAM_RANGE, BEAM_FALLOFF = 0.14, 420, 300
beam_volume = round(2 * BEAM_RANGE * math.tan(BEAM_ANGLE) * 1.08, 1)
CROSS = (PED[0], colossus_y + 20.67 * COL_SCALE + 45, PED[1] - 8)
for side in (-1, 1):
    src = (side * 72, TOP + 5, PED[1] - 22)
    entities.append(entity("res/models/cube4.obj", (src[0], TOP + 2.5, src[2]), (7, 5, 7), aim(src, CROSS),
                           BRONZE, cast_shadow=False))
    entities.append(entity("res/models/sphere.obj", (src[0], TOP + 5.2, src[2]), 2.2,
                           material=mat((1.0, 0.85, 0.6), emissive=1.0, extra={"emissiveNightOnly": True}),
                           cast_shadow=False))
    beam_light = len(lights)            # index in "Lights": the motes follow this light
    lights.append(light(src, (1.0, 0.8, 0.55), "SPOT", (beam_volume, beam_volume, BEAM_RANGE), rot=aim(src, CROSS),
                        cast_shadow=True, volumetric=True, numSamplePoints=48, coef1=0.8, coef2=0.02,
                        beamDensity=0.04, beamFalloff=BEAM_FALLOFF, beamHaze=0.8, coneAngle=BEAM_ANGLE,
                        shadowMapSize=1024,
                        diffuse_attenuation=[60, 1, 0.15], specular_attenuation=[60, 1, 0.3], nightOnly=True))
    # dust motes in the beam: linked to the light, so they move and rotate with
    # it and take its cone, falloff and brightness (they switch off with it)
    emitters.append({"type": "beam", "light": beam_light, "texture": "res/textures/glowWhite.png",
                     "count": 240, "lifetimeMs": 9000, "length": BEAM_RANGE * 0.8,
                     "speed": [0.5, 2.0], "wander": 1.0, "startScale": 3.3, "scaleJitter": 0.6,
                     "blend": "additive", "tint": [1.0, 0.88, 0.66], "fadeIn": 0.25})
# warm key light on the colossus' front, from the plaza (no volumetric haze)
key = (PED[0], TOP + 30, PED[1] + 150)
lights.append(light(key, (1.15, 0.85, 0.55), "SPOT", (500, 500, 380),
                    rot=aim(key, (PED[0], colossus_y + 12.0 * COL_SCALE, PED[1])), cast_shadow=True, volumetric=False,
                    diffuse_attenuation=[220, 4, 0.8], specular_attenuation=[300, 4, 1.2], nightOnly=True))
# spirit lights circling the crystal
for i, color in enumerate(((0.4, 0.9, 1.0), (0.7, 0.55, 1.0))):
    lights.append(light((ALTAR[0], CRYSTAL_Y, ALTAR[1]), color, "POINT", (120, 120, 120), cast_shadow=False,
                        diffuse_attenuation=[600, 10, 4], specular_attenuation=[90, 10, 2],
                        orbit={"center": [ALTAR[0], CRYSTAL_Y + i * 6, ALTAR[1]], "radius": 30 + i * 12,
                               "speed": 35 * (1 if i == 0 else -1), "bob": 5, "phase": i * 180}))

# --------------------------------------------------------------------------
# particles
# --------------------------------------------------------------------------
emitters += [
    # halos in the dusty air
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": [list(p) for p in brazier_tops],
     "startScale": 38, "blend": "additive", "tint": [1.0, 0.5, 0.18]},
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": [[t[0], t[1], t[2]] for t in torches],
     "startScale": 26, "blend": "additive", "tint": [1.0, 0.55, 0.2], "nightOnly": True},
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": [[ALTAR[0], CRYSTAL_Y, ALTAR[1]]],
     "startScale": 75, "blend": "additive", "tint": [0.35, 0.75, 1.0]},
    # motes spiralling up around the crystal
    {"type": "drift", "texture": "res/textures/glowWhite.png", "count": 60, "lifetimeMs": 4000,
     "position": [ALTAR[0], CRYSTAL_Y, ALTAR[1]], "radius": 22, "height": 30, "speed": [4, 10], "wander": 10,
     "startScale": 1.4, "blend": "additive", "tint": [0.5, 0.9, 1.0], "fadeIn": 0.3},
    # sky lanterns drifting up over the basin at night
    {"type": "spray", "texture": "res/textures/glowWhite.png", "count": 110, "lifetimeMs": 26000,
     "position": [60, BASIN_Y + 30, -1380], "radius": 420, "direction": [0.08, 1, 0.05], "spread": 0.1,
     "speed": [5, 9], "gravity": 0.8, "startScale": 30, "scaleJitter": 0.3, "blend": "additive",
     "tint": [1.0, 0.68, 0.32], "fadeIn": 0.15, "nightOnly": True},
    # dust drifting through the valley and over the plaza
    {"type": "drift", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 40, "lifetimeMs": 9000,
     "position": [40, 12, 100], "radius": 420, "height": 10, "speed": [6, 12], "wander": 5,
     "startScale": 55, "scaleJitter": 0.4, "blend": "alpha", "tint": [0.75, 0.55, 0.38], "brightness": 0.1,
     "fadeIn": 0.4},
    {"type": "drift", "texture": "res/textures/glowWhite.png", "count": 120, "lifetimeMs": 6000,
     "position": [0, 30, -790], "radius": 220, "height": 40, "speed": [2, 6], "wander": 6,
     "startScale": 0.9, "blend": "additive", "tint": [1.0, 0.85, 0.6], "fadeIn": 0.35},
]

# --------------------------------------------------------------------------
game = {
    "ConfigFilePath": "res/configs/config.json",
    "gravity": -10.0,
    # yaw: 0 looks down -z; the valley first heads north-east
    "PlayerStart": {"position": [SPAWN[0], H(*SPAWN) + 25, SPAWN[1]], "yaw": -24, "pitch": 2},
    "Environment": {"sky": "night", "zenithColor": [0.006, 0.008, 0.028], "horizonColor": [0.07, 0.055, 0.085],
                    "starIntensity": 1.0, "moonDirection": moon, "moonColor": [0.8, 0.84, 1.0],
                    "fogColor": [0.06, 0.05, 0.065], "fogDensity": 0.0011, "fogHeightFalloff": 0.0042,
                    "fogBaseHeight": BASIN_Y, "fogMoonGlow": 0.3,
                    # distance cut-off: the fog closes in from 400 and is opaque at
                    # 2400, which is also the draw distance (far plane)
                    "fogStart": 400, "fogEnd": 2400,
                    # a few desert clouds (and their shadows on the mesas)
                    "cloudCoverage": 0.25, "cloudSpeed": 10, "ambient": 0.085, "emissiveStrength": 1.7,
                    "pbr": True, "hdr": True, "exposure": 1.15, "bloomStrength": 0.5, "bloomThreshold": 0.9,
                    "dayExposure": 0.92,
                    "aoStrength": 0.9, "aoRadius": 18,
                    "saturation": 0.92, "grade": 0.6,
                    "ambientSkyTint": [0.85, 0.9, 1.3], "ambientGroundTint": [0.6, 0.45, 0.4],
                    # golden hour: the sun sets about 30 s after you arrive (T speeds time up)
                    "dayLength": 600, "timeOfDay": 0.70,
                    "dayZenithColor": [0.2, 0.36, 0.68], "dayHorizonColor": [0.86, 0.72, 0.58],
                    "sunsetColor": [1.0, 0.42, 0.14], "sunColor": [0.9, 0.78, 0.62],
                    "dayFogColor": [0.78, 0.62, 0.46], "dayFogDensityScale": 0.6, "dayAmbient": 0.38,
                    "dayAmbientSkyTint": [0.8, 0.88, 1.15], "dayAmbientGroundTint": [0.95, 0.7, 0.5]},
    "Water": {"height": WATER_Y, "center": [0, (POOL_Z0 + POOL_Z1) / 2], "size": [2 * POOL_X, POOL_Z1 - POOL_Z0],
              "tiling": 12, "waveStrength": 0.006, "waveSpeed": 0.02, "waveHeight": 0.15,
              "shallowColor": [0.55, 0.7, 0.72], "deepColor": [0.0, 0.03, 0.045],
              "murkDepth": 14, "softEdge": 1.5, "reflectivity": 1.0,
              # bright reflections (crystal, braziers) bloom into soft rippling streaks
              "reflectionGlow": 1.8},
    # a black hole hanging over the basin behind the colossus: it bends the
    # light of the sky, the buttes and its own accretion disk
    "BlackHoles": [{"position": [250, 380, -1750], "radius": 45, "lensing": 1.0, "influence": 9,
                    "diskInner": 2.6, "diskOuter": 7.5, "diskColor": [1.0, 0.62, 0.3], "diskBrightness": 3.0,
                    "diskOpacity": 0.85, "diskSpeed": 1.0, "diskTilt": [14, 8], "glow": 0.6}],
    "Entities": entities,
    "Lights": lights,
    "ParticleEmitters": emitters,
}
out = sys.argv[1] if len(sys.argv) > 1 else 'Game/canyon.json'
json.dump(game, open(out, 'w'), indent=1)
print("terrain %dx%d" % (NX, NZ), "entities", len(entities), "lights", len(lights), "emitters", len(emitters),
      "tiles", len(tiles), "boulders", len(boulders))
print("spawn", game["PlayerStart"]["position"], "sanctum floor", round(H(*SANCTUM), 1),
      "basin", round(H(0, -1400), 1), "colossus top", round(colossus_y + 20.67 * COL_SCALE, 1))
