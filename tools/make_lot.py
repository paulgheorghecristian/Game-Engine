"""Map 3, "The Lot": an abandoned film backlot at night, fenced in with
razor wire. On a rusted stage in the middle stands a masked man, lit by
three tripod floodlights; a giant white mask hangs behind him; rows of
mannequins face the stage like an audience. Puppets lie about, the low end
of the lot is flooded and mirrors the lights, and sodium lamps burn at the
corners. You arrive at the gate end, holding a pistol.

Assets from the gfxx folder (exported with tools/export_gfxx.py):
pistol.obj, tripod_light.obj; and the older exports mask_simp.obj (the
splash-screen mask), fance.obj (razor-wire fence), try_walkchange.dae (the
masked man, animated), FinalBaseMesh.obj (mannequin), mothman.obj (puppet).

usage: python3 tools/make_lot.py [Game/lot.json]
"""
import json, math, random, sys

random.seed(3)

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
# layout (yaw 0 looks down -z: from the gate towards the stage)
# --------------------------------------------------------------------------
LOT_X, LOT_Z0, LOT_Z1 = 460.0, -420.0, 420.0     # inside the fence
STAGE = (0.0, -170.0)                             # stage centre
STAGE_W, STAGE_D, STAGE_H = 150.0, 90.0, 6.0
POND = (250.0, 170.0)                             # the flooded low end
POND_R = (170.0, 120.0)
WATER_Y = -0.6
SPAWN = (0.0, 300.0)                              # on the stage axis

X0, X1, Z0, Z1, STEP = -1500.0, 1500.0, -1500.0, 1500.0, 12.0

def ground(x, z):
    """the lot: packed dirt with ruts; the flooded hollow; embankments outside the fence"""
    h = (fbm(x * 0.012, z * 0.012, 4, 2) - 0.5) * 3.0
    h += (fbm(x * 0.08, z * 0.08, 2, 9) - 0.5) * 0.6
    # the hollow that floods
    d = math.hypot((x - POND[0]) / POND_R[0], (z - POND[1]) / POND_R[1])
    h -= 4.5 * (1.0 - smoothstep(0.3, 1.25, d))
    # beyond the fence the ground rises into dark embankments
    out = max(abs(x) - LOT_X, LOT_Z0 - z, z - LOT_Z1, 0.0)
    h += smoothstep(60, 700, out) * 160.0 * (0.6 + 0.8 * fbm(x * 0.004, z * 0.004, 3, 5))
    return h

NX = int((X1 - X0) / STEP) + 1
NZ = int((Z1 - Z0) / STEP) + 1

def write_obj(path):
    hs = [[ground(X0 + i * STEP, Z0 + j * STEP) for i in range(NX)] for j in range(NZ)]
    with open(path, 'w') as f:
        f.write('# lot ground (tools/make_lot.py)\n')
        for j in range(NZ):
            for i in range(NX):
                f.write('v %.2f %.3f %.2f\n' % (X0 + i * STEP, hs[j][i], Z0 + j * STEP))
        for j in range(NZ):
            for i in range(NX):
                f.write('vt %.4f %.4f\n' % (i / (NX - 1) * 40.0, j / (NZ - 1) * 40.0))
        for j in range(NZ):
            for i in range(NX):
                l = hs[j][max(i - 1, 0)]; r = hs[j][min(i + 1, NX - 1)]
                d = hs[max(j - 1, 0)][i]; u = hs[min(j + 1, NZ - 1)][i]
                nx, ny, nz = l - r, 2 * STEP, d - u
                n = math.sqrt(nx * nx + ny * ny + nz * nz)
                f.write('vn %.4f %.4f %.4f\n' % (nx / n, ny / n, nz / n))
        for j in range(NZ - 1):
            for i in range(NX - 1):
                a = j * NX + i + 1; b = (j + 1) * NX + i + 1; c = j * NX + i + 2; d = (j + 1) * NX + i + 2
                f.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (a, a, a, b, b, b, c, c, c))
                f.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (c, c, c, b, b, b, d, d, d))

GROUND_OBJ = 'res/models/lot_ground.obj'
write_obj(GROUND_OBJ)

# --------------------------------------------------------------------------
# helpers (same conventions as make_canyon.py)
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

def instanced(mesh, transforms, material=None):
    irc = {"Mesh": mesh, "Transforms": transforms}
    if material: irc["Material"] = material
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

def yaw_to(frm, to):
    """yaw (degrees) turning a model's front (+z) towards `to` in the xz plane"""
    return round(math.degrees(math.atan2(to[0] - frm[0], to[1] - frm[1])), 2)

entities, lights, emitters = [], [], []

RUST_PLATE = mat((0.85, 0.8, 0.75), "res/textures/158.JPG", "res/textures/158_norm.JPG",
                 extra={"roughness": 0.65, "metallic": 0.3, "specularStrength": 0.5})
BLACK_PAINT = mat((0.55, 0.55, 0.55), "res/textures/Plastic002_2K_Color.jpg",
                  extra={"roughness": 0.45, "specularStrength": 0.6})
STEEL = mat((0.55, 0.56, 0.6), "res/textures/metal.jpg", "res/textures/metal_normal.jpg",
            extra={"metallic": 0.6, "roughness": 0.45})
LAMP_BODY = mat((0.18, 0.18, 0.2), shininess=60, extra={"metallic": 0.5, "roughness": 0.4})
LATEX = mat((0.05, 0.05, 0.055), shininess=120, extra={"roughness": 0.18, "specularStrength": 1.0})
PORCELAIN = mat((0.92, 0.9, 0.86), shininess=90, extra={"roughness": 0.25, "specularStrength": 0.9})
MANNEQUIN = mat((0.8, 0.78, 0.74), shininess=70, extra={"roughness": 0.35, "specularStrength": 0.7})
PUPPET = mat((0.62, 0.5, 0.38), shininess=10, extra={"roughness": 0.85})
SODIUM_GLOBE = mat((1.0, 0.55, 0.18), emissive=1.0)
LAMP_LENS = mat((1.0, 0.97, 0.9), emissive=1.0, extra={"emissiveNightOnly": False})
CONCRETE = mat((0.62, 0.6, 0.58), "res/textures/rock2k.jpg", "res/textures/rock2k_normal.jpg",
               extra={"roughness": 0.9, "specularStrength": 0.2})

# --------------------------------------------------------------------------
# ground
# --------------------------------------------------------------------------
entities.append({"Transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
                 "Components": {"RenderComponent": {"Mesh": GROUND_OBJ,
                                                    "Material": mat((1.0, 1.0, 1.0), shininess=60, extra={
                                                        "Terrain": {"grass": "res/textures/terrain_dirt.bmp",
                                                                    "grass2": "res/textures/182.bmp",
                                                                    "rock": "res/textures/rock2k.jpg",
                                                                    "rockNormal": "res/textures/rock2k_normal.jpg",
                                                                    "dirt": "res/textures/terrain_dirt.bmp",
                                                                    "soil": "res/textures/terrain_dirt.bmp",
                                                                    "grassNormal": "res/textures/terrain_dirt_normal.png",
                                                                    "dirtNormal": "res/textures/terrain_dirt_normal.png",
                                                                    "soilNormal": "res/textures/terrain_dirt_normal.png",
                                                                    "normalStrength": 0.8,
                                                                    "tileSize": 45,
                                                                    "shoreHeight": -1000,   # no sand shore layer
                                                                    # damp after rain, with puddles: the fires,
                                                                    # lamps and projectors reflect in them
                                                                    "wetness": 0.75}})},
                                "PhysicsComponent": {"BoundingBodyType": ["mesh", GROUND_OBJ],
                                                     "scale": [1, 1, 1], "mass": 0}}})

# --------------------------------------------------------------------------
# the fence: razor-wire chain link all round (gaps at the gate behind you)
# --------------------------------------------------------------------------
FENCE_SCALE = 9.0                   # fance.obj: 8.31 wide -> ~75, 6.09 tall -> ~55
SEG = 8.31 * FENCE_SCALE
fence = []
def fence_run(x0, z0, x1, z1, skip=None):
    length = math.hypot(x1 - x0, z1 - z0)
    n = max(1, int(round(length / SEG)))
    yaw = math.degrees(math.atan2(-(z1 - z0), x1 - x0))
    for k in range(n):
        t = (k + 0.5) / n
        x, z = x0 + (x1 - x0) * t, z0 + (z1 - z0) * t
        if skip and skip(x, z):
            continue
        sx = length / n / 8.31
        fence.append(tr((x, ground(x, z) - 1.0, z), (sx, FENCE_SCALE, FENCE_SCALE), (0, yaw, 0)))
fence_run(-LOT_X, LOT_Z0, LOT_X, LOT_Z0)
fence_run(-LOT_X, LOT_Z1, LOT_X, LOT_Z1, skip=lambda x, z: abs(x - SPAWN[0]) < 60)   # the gate
fence_run(-LOT_X, LOT_Z0, -LOT_X, LOT_Z1)
fence_run(LOT_X, LOT_Z0, LOT_X, LOT_Z1)
entities.append(instanced("res/models/fance.obj", fence, STEEL))
# the fence keeps you in (thin walls along it)
for (cx, cz, sx, sz) in ((0, LOT_Z0, 2 * LOT_X, 4), (-LOT_X, 0, 4, LOT_Z1 - LOT_Z0), (LOT_X, 0, 4, LOT_Z1 - LOT_Z0),
                         ((-LOT_X + SPAWN[0] - 60) / 2, LOT_Z1, SPAWN[0] - 60 + LOT_X, 4),
                         ((LOT_X + SPAWN[0] + 60) / 2, LOT_Z1, LOT_X - SPAWN[0] - 60, 4)):
    entities.append({"Transform": {"position": [cx, 30, cz], "rotation": [0, 0, 0], "scale": [sx, 80, sz]},
                     "Components": {"PhysicsComponent": box_physics((sx, 80, sz))}})

# --------------------------------------------------------------------------
# the stage: a rusted steel platform; the masked man; the mask behind him
# --------------------------------------------------------------------------
sg = ground(*STAGE)
stage_top = sg + STAGE_H
# (cube4.obj is a unit cube: its scale is the full size)
entities.append(entity("res/models/cube4.obj", (STAGE[0], stage_top - (STAGE_H + 8) / 2, STAGE[1]),
                       (STAGE_W, STAGE_H + 8, STAGE_D), material=RUST_PLATE,
                       physics=box_physics((STAGE_W, STAGE_H + 8, STAGE_D))))
# two steps up at the front
for k, (dz, h) in enumerate(((STAGE_D / 2 + 6, STAGE_H * 0.66), (STAGE_D / 2 + 18, STAGE_H * 0.33))):
    entities.append(entity("res/models/cube4.obj", (STAGE[0], sg + h / 2 - 2, STAGE[1] + dz), (44, h + 4, 12),
                           material=RUST_PLATE, physics=box_physics((44, h + 4, 12))))

# the masked man: posed from the rig in try.blend (the engine's skeletal
# loader breaks this rig, so he is a still figure: he just stands there),
# the mask and the black suit as two parts with their own materials
MAN = (STAGE[0], STAGE[1] + 8)
MAN_SCALE = 0.24                    # the rig is ~185 tall (cm) -> ~44 (an adult next to the player)
for part, material in (("body", LATEX), ("mask", PORCELAIN)):
    entities.append(entity("res/models/masked_man_%s.obj" % part, (MAN[0], stage_top, MAN[1]), MAN_SCALE,
                           (0, 0, 0), material))

# the giant mask, hung on a black frame behind the stage
MASK_SCALE = 30.0
mask_pos = (STAGE[0], stage_top + 52, STAGE[1] - STAGE_D / 2 - 14)
entities.append(entity("res/models/mask_simp.obj", mask_pos, MASK_SCALE, (-90, 0, 0), PORCELAIN))
for side in (-1, 1):
    x = STAGE[0] + side * 46
    entities.append(entity("res/models/cube4.obj", (x, sg + 60, mask_pos[2] - 12), (5, 120, 5), material=LAMP_BODY))
entities.append(entity("res/models/cube4.obj", (STAGE[0], sg + 118, mask_pos[2] - 12), (98, 5, 5), material=LAMP_BODY))

# a cold uplight from the stage floor on the hung mask
up = (STAGE[0], stage_top + 1, STAGE[1] - 10)
lights.append(light(up, (0.75, 0.82, 1.0), "SPOT", (220, 220, 160), rot=aim(up, (mask_pos[0], mask_pos[1] + 10, mask_pos[2])),
                    cast_shadow=False, coneAngle=0.42, diffuse_attenuation=[300, 2, 0.4],
                    specular_attenuation=[300, 2, 0.6]))

# --------------------------------------------------------------------------
# three tripod floodlights on the man (two through the haze)
# --------------------------------------------------------------------------
TRIPOD_SCALE = 1.9                  # tripod_light.obj: 24.3 tall -> ~46
# in the model the lamp's front (lens, barn doors) faces -z; its pole stands
# at (-0.9, 3); the housing's axis runs through x -1.0, y 21.6 and its front
# housing's front face is at z ~1.75, with the barn doors standing out in
# front of it (z 0..1): the lens is a thin disc flush on that face, framed
# and hidden from the side by the doors (model units)
TRIPOD_POLE = (-0.9, 3.0)
TRIPOD_LENS = (-1.0, 21.6, 1.68)
target = (MAN[0], stage_top + 20, MAN[1])

def turn(local_x, local_z, yaw):
    """a model-space xz offset turned by the engine's yaw (degrees)"""
    a = math.radians(yaw)
    return (local_x * math.cos(a) + local_z * math.sin(a), -local_x * math.sin(a) + local_z * math.cos(a))

for k, (x, z, volumetric) in enumerate(((-95, STAGE[1] + 110, True), (100, STAGE[1] + 95, True), (-15, STAGE[1] + 150, True))):
    g = ground(x, z)
    # yaw turning the model's -z front at the man (the same convention as aim())
    yaw = math.degrees(math.atan2(-(MAN[0] - x), -(MAN[1] - z)))
    px, pz = turn(TRIPOD_POLE[0] * TRIPOD_SCALE, TRIPOD_POLE[1] * TRIPOD_SCALE, yaw)
    origin = (x - px, g, z - pz)       # so the pole, not the model origin, stands at (x, z)
    entities.append(entity("res/models/tripod_light.obj", origin, TRIPOD_SCALE, (0, round(yaw, 2), 0), LAMP_BODY))
    lx, lz = turn(TRIPOD_LENS[0] * TRIPOD_SCALE, TRIPOD_LENS[2] * TRIPOD_SCALE, yaw)
    head = (origin[0] + lx, g + TRIPOD_LENS[1] * TRIPOD_SCALE, origin[2] + lz)
    attrs = dict(cast_shadow=True, coneAngle=0.2, shadowMapSize=1024,
                 diffuse_attenuation=[380, 2, 0.25], specular_attenuation=[380, 2, 0.4])
    if volumetric:
        attrs.update(volumetric=True, numSamplePoints=40, coef1=0.8, coef2=0.02, beamDensity=0.018,
                     beamFalloff=220, beamHaze=0.5)
    lights.append(light(head, (1.0, 0.96, 0.9), "SPOT", (300, 300, 340), rot=aim(head, target), **attrs))
    # the glowing lens: a flat bright disc centred in the lamp's opening,
    # facing out the way the lamp faces (emissive: it blooms like the sodium globes)
    entities.append(entity("res/models/sphere.obj", head, (2.2, 2.2, 0.06), (0, round(yaw, 2), 0), LAMP_LENS,
                           cast_shadow=False))

# --------------------------------------------------------------------------
# the audience: mannequins in rows, facing the stage (a few turned round)
# --------------------------------------------------------------------------
MANNEQUIN_SCALE = 2.15              # FinalBaseMesh: 20.7 tall -> ~44 (1.78 m: the player's eye is at 41)
audience = []
for row in range(4):
    for col in range(7):
        x = (col - 3) * 38 + (row % 2) * 12 + random.uniform(-5, 5)
        z = STAGE[1] + 120 + row * 42 + random.uniform(-4, 4)
        if abs(x + 15) < 30 and row == 0:
            continue                # room for the front light
        yaw = yaw_to((x, z), MAN) + random.uniform(-6, 6)
        if random.random() < 0.12:
            yaw += 180              # one looks back at you
        audience.append(tr((x, ground(x, z), z), MANNEQUIN_SCALE, (0, yaw, 0)))
entities.append(instanced("res/models/FinalBaseMesh.obj", audience, MANNEQUIN))

# puppets: slumped against the fence, in the dirt, one at the water
for (x, z, yaw, pitch) in ((-430, 120, 80, 0), (-400, -300, 30, 0), (420, -260, -100, 0),
                           (150, 300, 200, 85), (-250, 260, 10, 0), (300, 60, -40, 0)):
    entities.append(entity("res/models/mothman.obj", (x, ground(x, z), z), 2.6, (pitch, yaw, 0), PUPPET))

# a mask dropped in the dirt in front of you
entities.append(entity("res/models/mask_simp.obj", (SPAWN[0] + 8, ground(SPAWN[0] + 8, SPAWN[1] - 70) + 1.0,
                                                    SPAWN[1] - 70), 4.0, (0, 25, 0), PORCELAIN))

# --------------------------------------------------------------------------
# sodium lamps at the corners and by the gate
# --------------------------------------------------------------------------
for (x, z) in ((-410, -370), (410, -370), (-410, 370), (410, 300), (SPAWN[0] + 75, LOT_Z1 - 20), (-420, -60), (420, 20)):
    g = ground(x, z)
    entities.append(entity("res/models/cube4.obj", (x, g + 45, z), (3.2, 90, 3.2), material=LAMP_BODY,
                           physics=box_physics((3.2, 90, 3.2))))
    entities.append(entity("res/models/sphere.obj", (x, g + 92, z), 3.2, material=SODIUM_GLOBE, cast_shadow=False))
    lights.append(light((x, g + 88, z), (1.0, 0.5, 0.16), "POINT", (480, 480, 480), cast_shadow=True,
                        shadowMapSize=512, diffuse_attenuation=[260, 4, 0.9], specular_attenuation=[200, 4, 1.4],
                        flicker=0.06 if x > 0 else 0.0, flickerSpeed=7))

# a faint cold key light from the overcast sky (the map has no moon; with a
# fixed time of day this light keeps its color), with shadows
lights.append(light((0, 0, 0), (0.3, 0.33, 0.42), "DIR", (0, 0, 0), dir=[0.45, 0.55, -0.7], cast_shadow=True,
                    diffuse_attenuation=[0, 0, 0], specular_attenuation=[0, 0, 0]))

# fire in steel bins among the audience: warm flickering light on the
# mannequins, their shadows dancing (one between them and the gate, so
# you see them lit as you come in)
FIRES = ((-150.0, 10.0), (155.0, 30.0), (15.0, 118.0))
fire_sounds = []
for (x, z) in FIRES:
    g = ground(x, z)
    entities.append(entity("res/models/cube4.obj", (x, g + 6.0, z), (11, 13, 11), (0, random.uniform(0, 45), 0),
                           RUST_PLATE, physics=box_physics((11, 13, 11))))
    top = g + 12.5
    lights.append(light((x, top + 14, z), (1.0, 0.45, 0.12), "POINT", (300, 300, 300), cast_shadow=True,
                        shadowMapSize=1024, diffuse_attenuation=[900, 14, 4.5], specular_attenuation=[260, 14, 4],
                        flicker=0.35, flickerSpeed=11))
    emitters += [
        {"type": "smoke", "texture": "res/textures/fire-texture-atlas.jpg", "heat": 1.0, "atlas": [4, 4], "atlasTopDown": True,
         "count": 40, "lifetimeMs": 850, "position": [x, top, z], "radius": 4, "direction": [0, 1, 0], "spread": 0.15,
         "speed": [14, 24], "gravity": 0, "buoyancy": 0.4, "startScale": 13, "growth": -6, "blend": "additive",
         "tint": [1.0, 0.85, 0.7], "brightness": 0.0},
        {"type": "spray", "texture": "res/textures/glowWhite.png", "count": 50, "lifetimeMs": 2200,
         "position": [x, top + 3, z], "radius": 3, "direction": [0, 1, 0], "spread": 0.45, "speed": [25, 60],
         "gravity": -8, "startScale": 1.5, "blend": "additive", "tint": [1.0, 0.55, 0.2]},
        {"type": "smoke", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 30, "lifetimeMs": 4500,
         "position": [x, top + 26, z], "radius": 3, "direction": [0.15, 1, 0], "spread": 0.2, "speed": [12, 20],
         "gravity": -2, "buoyancy": 0.5, "startScale": 14, "growth": 16, "blend": "alpha", "tint": [0.4, 0.3, 0.24],
         "brightness": 0.4, "fadeIn": 0.35},
    ]
    fire_sounds.append({"sound": "fire_loop", "position": [x, top, z], "volume": 0.9, "radius": 380})

# a crate stack and a concrete block by the stage
for (x, z, s) in ((-150, -230, 14), (-128, -238, 10), (-146, -208, 9), (170, -120, 16)):
    entities.append(entity("res/models/cube4.obj", (x, ground(x, z) + s / 2 - 1, z), s, (0, random.uniform(0, 40), 0),
                           CONCRETE, physics=box_physics((s, s, s))))

# --------------------------------------------------------------------------
# haze in the lights, sparks from a failing lamp
# --------------------------------------------------------------------------
emitters += [
    {"type": "drift", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 40, "lifetimeMs": 9000,
     "position": [STAGE[0], stage_top + 25, STAGE[1] + 40], "radius": 140, "height": 30, "speed": [3, 8],
     "wander": 6, "startScale": 60, "blend": "alpha", "tint": [0.32, 0.33, 0.36], "brightness": 0.5, "fadeIn": 0.45},
    {"type": "spray", "texture": "res/textures/glowWhite.png", "count": 30, "lifetimeMs": 900,
     "position": [410, ground(410, 300) + 92, 300], "radius": 2, "direction": [0, -0.2, 1], "spread": 0.6,
     "speed": [20, 60], "gravity": -90, "startScale": 1.2, "blend": "additive", "tint": [1.0, 0.65, 0.3]},
]

# --------------------------------------------------------------------------
# a black hole in the sky (as in lagoon_bh.json, without the moon), centred
# over the stage and the mask as seen from the gate
# --------------------------------------------------------------------------
EYE_Y = ground(*SPAWN) + 27.0
mask_top = mask_pos[1] + 0.5 * 2.87 * MASK_SCALE
mask_angle = math.atan2(mask_top - EYE_Y, SPAWN[1] - mask_pos[2])
hole_angle = mask_angle + math.radians(12)
HOLE_DIR = [0.0, round(math.sin(hole_angle), 4), round(-math.cos(hole_angle), 4)]
START_PITCH = round(math.degrees(hole_angle) * 0.45, 1)
# the Milky Way crosses the sky at a slant with its bright core right behind
# the hole, so the lensing visibly bends it
def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
_slant = [1.0, 0.55, 0.0]
_pole = cross(HOLE_DIR, _slant)
_len = math.sqrt(sum(c * c for c in _pole))
GALAXY_POLE = [round(c / _len, 4) for c in _pole]
black_holes = [{"sky": True, "physical": True, "direction": HOLE_DIR, "angularRadius": 0.03, "lensing": 0.8, "influence": 11,
                "colorShift": 0.7, "diskInner": 3.0, "diskOuter": 16.0, "diskColor": [1.0, 0.55, 0.26],
                "diskBrightness": 2.0, "diskOpacity": 0.55, "diskSpeed": 0.8, "diskTilt": [39.01, -14.54],
                "glow": 0.0, "moonStream": 0.0, "cloudClear": 0.7}]

# --------------------------------------------------------------------------
game = {
    "ConfigFilePath": "res/configs/config.json",
    "gravity": -10.0,
    "PlayerStart": {"position": [SPAWN[0], ground(*SPAWN) + 25, SPAWN[1]], "yaw": 0, "pitch": START_PITCH},
    "Environment": {"sky": "night", "zenithColor": [0.004, 0.005, 0.01], "horizonColor": [0.035, 0.035, 0.045],
                    # no moon (the black hole owns the sky); the faint cold key
                    # light is the directional light in "Lights"
                    "starIntensity": 0.7, "moonDirection": [0.45, 0.55, -0.7], "moonColor": [0.42, 0.46, 0.58],
                    "moonSize": 0.02, "moon": False,
                    "milkyWay": 1.4, "milkyWayNormal": GALAXY_POLE, "milkyWayCenter": HOLE_DIR,
                    "fogColor": [0.045, 0.045, 0.055], "fogDensity": 0.0016, "fogHeightFalloff": 0.006,
                    "fogBaseHeight": -10.0, "fogMoonGlow": 0.0, "fogLightGlow": 3.4, "fogStart": 150, "fogEnd": 1600,
                    "cloudCoverage": 0.3, "cloudSpeed": 6, "cloudHeight": 12000, "wind": 0.6,   # clear enough for the Milky Way
                    "ambient": 0.1, "emissiveStrength": 2.2,
                    "pbr": True, "hdr": True, "exposure": 1.3, "bloomStrength": 0.6, "bloomThreshold": 0.85,
                    "aoStrength": 1.0, "aoRadius": 14, "saturation": 0.8, "grade": 0.7,
                    "ambientSkyTint": [0.8, 0.85, 1.1], "ambientGroundTint": [0.6, 0.55, 0.5]},
    "Water": {"height": WATER_Y, "center": [POND[0], POND[1]], "size": [POND_R[0] * 2.2, POND_R[1] * 2.2],
              "tiling": 10, "waveStrength": 0.004, "waveSpeed": 0.015, "waveHeight": 0.08,
              "shallowColor": [0.25, 0.24, 0.22], "deepColor": [0.01, 0.01, 0.012],
              "murkDepth": 3, "softEdge": 1.2, "reflectivity": 1.0, "reflectionGlow": 1.6},
    "Audio": [{"sound": "wind_loop", "volume": 0.3, "follows": "wind"},
              {"sound": "crickets_loop", "volume": 0.12, "follows": "night"}] + fire_sounds,
    "Footsteps": {"sound": "step_sand", "variants": 6, "volume": 0.9, "dustPerStep": 4,
                  "wade": "wade", "wadeVariants": 3, "splash": "splash",   # the pond
                  "dust": {"type": "spray", "burst": True, "texture": "res/textures/smoke.png", "atlas": [8, 5],
                           "count": 60, "lifetimeMs": 900, "position": [0, 0, 0], "radius": 4,
                           "direction": [0, 1, 0], "spread": 0.9, "speed": [8, 22], "gravity": -22,
                           "startScale": 6, "growth": 18, "scaleJitter": 0.5, "blend": "alpha",
                           "tint": [0.3, 0.27, 0.24], "brightness": 0.5, "fadeIn": 0.1}},
    "BlackHoles": black_holes,
    "Entities": entities,
    "Lights": lights,
    "ParticleEmitters": emitters,
}
out = sys.argv[1] if len(sys.argv) > 1 else 'Game/lot.json'
json.dump(game, open(out, 'w'), indent=1)
print("ground %dx%d" % (NX, NZ), "entities", len(entities), "lights", len(lights), "emitters", len(emitters),
      "fence", len(fence), "audience", len(audience), "stage top", round(stage_top, 1))
