# Generates Game/lagoon.json: a night-time lagoon showcase for the engine.
#
# Usage (from the repository root):  python3 tools/make_lagoon.py [Game/lagoon.json]
#
# Objects are placed on the terrain by sampling terrain4.obj's height, so edit
# positions/counts here and regenerate rather than hand-editing the JSON.
import json, math, random, sys

# Terrain height sampler for terrain4.obj as placed in the maps (scale 4800,1600,4800, pos y=3)
SX,SY,SZ,OY=4800,1600,4800,3.0
V=[];F=[]
for l in open('res/models/terrain4.obj'):
    if l.startswith('v '):
        a=l.split(); V.append((float(a[1])*SX,float(a[2])*SY+OY,float(a[3])*SZ))
    elif l.startswith('f '):
        F.append([int(p.split('/')[0])-1 for p in l.split()[1:]])
def height(x,z):
    for f in F:
        for i in range(1,len(f)-1):
            a,b,c=V[f[0]],V[f[i]],V[f[i+1]]
            d=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2])
            if abs(d)<1e-9: continue
            w1=((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/d
            w2=((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/d
            w3=1-w1-w2
            if w1>=-1e-6 and w2>=-1e-6 and w3>=-1e-6:
                return w1*a[1]+w2*b[1]+w3*c[1]
    return None


random.seed(7)
C = (500.0, -775.0)          # lagoon centre (natural depression in terrain4)
WATER = -10.0

def H(x, z):
    h = height(x, z)
    return h if h is not None else 0.0

def polar(angle_deg, r):
    a = math.radians(angle_deg)
    return C[0] + math.cos(a) * r, C[1] + math.sin(a) * r

def shore_radius(angle_deg, level=WATER + 2):
    r = 150
    while r < 900:
        x, z = polar(angle_deg, r)
        if H(x, z) > level:
            return r
        r += 5
    return r

def mat(diffuse, texture=None, normal=None, rough=None, emissive=0.0, shininess=20.0, extra=None):
    m = {"ambient": [0, 0, 0], "diffuse": list(diffuse), "specular": [0.2, 0.2, 0.2], "shininess": shininess}
    if texture: m["Texture"] = texture
    if normal: m["NormalMapTexture"] = normal
    if rough: m["RoughnessTexture"] = rough
    if emissive: m["emissive"] = emissive
    if extra: m.update(extra)
    return m

def entity(mesh, pos, scale, rot=(0, 0, 0), material=None, physics=None, cast_shadow=True):
    rc = {"Mesh": mesh}
    if not cast_shadow: rc["castShadow"] = False
    if material: rc["Material"] = material
    comps = {"RenderComponent": rc}
    if physics: comps["PhysicsComponent"] = physics
    return {"Transform": {"position": [round(v, 2) for v in pos],
                          "rotation": [round(v, 2) for v in rot],
                          "scale": [round(v, 3) for v in (scale if isinstance(scale, (list, tuple)) else (scale,) * 3)]},
            "Components": comps}

def instanced(mesh, transforms, material=None):
    irc = {"Mesh": mesh, "Transforms": transforms}
    if material: irc["Material"] = material
    return {"Transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
            "Components": {"InstanceRenderComponent": irc}}

def aim(frm, to):
    """euler (pitch, yaw, 0) in degrees so that a spot light at frm points at to
    (glm::quat(euler) * (0,0,-1) == (-cos p sin y, sin p, -cos p cos y))"""
    d = [to[i] - frm[i] for i in range(3)]
    n = math.sqrt(sum(c * c for c in d)); d = [c / n for c in d]
    return (round(math.degrees(math.asin(d[1])), 2), round(math.degrees(math.atan2(-d[0], -d[2])), 2), 0)

def light(pos, color, kind, scale, rot=(0, 0, 0), **attrs):
    a = {"color": list(color), "type": kind}
    a.update(attrs)
    return {"Transform": {"position": [round(v, 2) for v in pos], "rotation": list(rot),
                          "scale": list(scale)}, "Attributes": a}

entities, lights, emitters = [], [], []
ROCK_MAT = mat((0.8, 0.8, 0.85), "res/textures/rock2k.jpg", "res/textures/rock2k_normal.jpg", "res/textures/rock2k_rough.jpg")

# --- terrain + collision ---------------------------------------------------
entities.append({"Transform": {"position": [0, 3, 0], "rotation": [0, 0, 0, 1], "scale": [4800, 1600, 4800]},
                 "Components": {"RenderComponent": {"Mesh": "res/models/terrain4.obj",
                                                    "Material": mat((0.9, 0.9, 0.9), shininess=100, extra={
                                                        # world-space tiled, slope / height blended layers
                                                        "Terrain": {"grass": "res/textures/terrain_grass.bmp",
                                                                    "grass2": "res/textures/terrain_grass2.bmp",
                                                                    "rock": "res/textures/165.bmp",
                                                                    "rockNormal": "res/textures/165_norm.bmp",
                                                                    "dirt": "res/textures/terrain_dirt.bmp",
                                                                    "soil": "res/textures/terrain_soil.bmp",
                                                                    "grassNormal": "res/textures/terrain_grass2_normal.png",
                                                                    "dirtNormal": "res/textures/terrain_dirt_normal.png",
                                                                    "soilNormal": "res/textures/terrain_soil_normal.png",
                                                                    "normalStrength": 1.0,
                                                                    "tileSize": 60, "shoreHeight": WATER}})},
                                "PhysicsComponent": {"BoundingBodyType": ["mesh", "res/models/terrain4.obj"],
                                                     "scale": [4800, 1600, 4800], "mass": 0}}})
# invisible floor just under the surface: you wade instead of sinking into the lagoon
entities.append({"Transform": {"position": [C[0], WATER - 12, C[1]], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
                 "Components": {"PhysicsComponent": {"BoundingBodyType": ["cube"], "scale": [1300, 10, 1300], "mass": 0}}})

# --- island with statue -------------------------------------------------------
island_rock_scale = 38.0
island_y = -22.0
entities.append(entity("res/models/rock.obj", (C[0], island_y, C[1]), island_rock_scale, (0, 35, 0), ROCK_MAT))
rock_top = island_y + 1.31 * island_rock_scale
buddha_scale = 0.95
buddha_pos = (C[0], rock_top - 7 + 30 * buddha_scale, C[1])
entities.append(entity("res/models/buddha.obj", buddha_pos, buddha_scale, (0, 0, 0),
                       mat((0.95, 0.78, 0.45), shininess=60, emissive=0.04,
                           extra={"metallic": 1.0, "roughness": 0.3})))       # gold
# smaller rocks around the island
for i in range(7):
    a = i * 360 / 7 + random.uniform(-15, 15)
    x, z = C[0] + math.cos(math.radians(a)) * 42, C[1] + math.sin(math.radians(a)) * 42
    entities.append(entity("res/models/rock.obj", (x, WATER - 4, z), random.uniform(9, 14),
                           (random.uniform(0, 30), random.uniform(0, 360), 0), ROCK_MAT))

# --- dragon on the west shore ---------------------------------------------
dr = shore_radius(185) + 25
dx, dz = polar(185, dr)
entities.append(entity("res/models/dragon.obj", (dx, H(dx, dz) + 21 * 1.7 - 4, dz), 1.7, (0, 90, 0),
                       mat((0.25, 0.55, 0.42), shininess=80, emissive=0.03,
                           extra={"roughness": 0.22})))                        # polished jade

# --- lighthouse on the north-west hill -------------------------------------
lx, lz = polar(272, 440)   # north shore, straight ahead of the spawn
lh = H(lx, lz)
tower_h = 110
entities.append(entity("res/models/cube4.obj", (lx, lh + tower_h / 2 - 5, lz), (18, tower_h, 18), (0, 20, 0),
                       mat((0.75, 0.72, 0.7), "res/textures/brick_wall.jpg", "res/textures/brick_wall_normal.jpg",
                           extra={"roughness": 0.85})))
lamp = (lx, lh + tower_h - 2, lz)
entities.append(entity("res/models/sphere.obj", lamp, 7, material=mat((1.0, 0.95, 0.8), emissive=1.0, extra={"emissiveNightOnly": True}), cast_shadow=False))

# --- lantern path from the spawn to the shore --------------------------------
SPAWN = (490.0, -60.0)
LANTERN_MAT = mat((1.0, 0.78, 0.45), emissive=1.0, extra={"emissiveNightOnly": True})
POST_MAT = mat((0.55, 0.45, 0.35), "res/textures/Bark002_2K_Color.jpg", "res/textures/Bark002_2K_Normal.jpg",
               extra={"roughness": 0.85})
for i, z in enumerate([-150, -230, -310, -390]):
    x = SPAWN[0] + (-38 if i % 2 == 0 else 38)
    g = H(x, z)
    # thin post ending well below the globe: a small dark spot under each lantern
    entities.append(entity("res/models/cube4.obj", (x, g + 13, z), (1.4, 30, 1.4), material=POST_MAT))
    entities.append(entity("res/models/sphere.obj", (x, g + 34, z), 3.2, material=LANTERN_MAT, cast_shadow=False))
    lights.append(light((x, g + 34, z), (1.0, 0.62, 0.28), "POINT", (170, 170, 170),
                        cast_shadow=True, shadowMapSize=512, diffuse_attenuation=[600, 10, 3.9], specular_attenuation=[90, 10, 2],
                        flicker=0.08, flickerSpeed=6 + i, nightOnly=True))

# iron fences along the path, between the lanterns and the walkway: their bars
# throw striped point-light shadows across the path
FENCE_SCALE = 4.2
fences = []
for z in range(-120, -420, -21):
    for side in (-31, 31):
        x = SPAWN[0] + side
        fences.append({"position": [x, round(H(x, z) + 2.41 * FENCE_SCALE - 0.5, 2), z],
                       "rotation": [0, 90, 0], "scale": [FENCE_SCALE] * 3})
entities.append(instanced("res/models/fence3.obj", fences))

# --- campfire on the east shore --------------------------------------------
fx, fz = polar(62, shore_radius(62) + 70)
fy = H(fx, fz)
for k, a in enumerate((0, 60, 120)):
    entities.append(entity("res/models/cube4.obj", (fx, fy + 2.0, fz), (2.4, 2.4, 15), (12 if k == 1 else -8, a, 0), POST_MAT))
entities.append(entity("res/models/sphere.obj", (fx, fy + 0.5, fz), (7, 1.5, 7), material=mat((1.0, 0.45, 0.12), emissive=1.0), cast_shadow=False))
# log benches around the fire (long radial shadows)
for a in (20, 140, 260):
    bx, bz = fx + math.cos(math.radians(a)) * 30, fz + math.sin(math.radians(a)) * 30
    entities.append(entity("res/models/cube4.obj", (bx, H(bx, bz) + 2.5, bz), (4, 4, 26), (0, -a, 0), POST_MAT))
ring = [{"position": [round(fx + math.cos(t) * 13, 2), round(H(fx + math.cos(t) * 13, fz + math.sin(t) * 13) - 0.5, 2),
                      round(fz + math.sin(t) * 13, 2)],
         "rotation": [0, round(math.degrees(t), 1), 0], "scale": [3.2, 2.2, 3.2]}
        for t in [i * 2 * math.pi / 10 for i in range(10)]]
lights.append(light((fx, fy + 20, fz), (1.0, 0.45, 0.12), "POINT", (260, 260, 260),
                    cast_shadow=True, shadowMapSize=1024, diffuse_attenuation=[450, 10, 3], specular_attenuation=[90, 10, 2],
                    flicker=0.35, flickerSpeed=11))
emitters += [
    {"type": "smoke", "texture": "res/textures/fire-texture-atlas.jpg", "atlas": [4, 4], "atlasTopDown": True,
     "count": 45, "lifetimeMs": 900, "position": [fx, fy + 1, fz], "radius": 5, "direction": [0, 1, 0], "spread": 0.15,
     "speed": [14, 26], "gravity": 0, "buoyancy": 0.4, "startScale": 15, "growth": -6, "blend": "additive",
     "tint": [1.0, 0.85, 0.7], "brightness": 0.0},
    {"type": "spray", "texture": "res/textures/glowWhite.png", "count": 70, "lifetimeMs": 2200,
     "position": [fx, fy + 4, fz], "radius": 4, "direction": [0, 1, 0], "spread": 0.45, "speed": [25, 60],
     "gravity": -8, "startScale": 1.6, "blend": "additive", "tint": [1.0, 0.55, 0.2]},
    {"type": "smoke", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 35, "lifetimeMs": 4500,
     "position": [fx, fy + 30, fz], "radius": 3, "direction": [0.15, 1, 0], "spread": 0.2, "speed": [12, 20],
     "gravity": -2, "buoyancy": 0.5, "startScale": 14, "growth": 16, "blend": "alpha", "tint": [0.5, 0.36, 0.26],
     "brightness": 0.45, "fadeIn": 0.35},
]

# --- trees, shore rocks, path stones (instanced) -----------------------------
trees = []
for a in range(0, 360, 14):
    if 70 <= a <= 112 or 40 <= a <= 80 and random.random() < 0.6:
        continue                       # keep the path and campfire view open
    r = shore_radius(a) + random.uniform(70, 190)
    x, z = polar(a + random.uniform(-5, 5), r)
    if H(x, z) > 260:
        continue
    trees.append({"position": [round(x, 2), round(H(x, z) - 1, 2), round(z, 2)],
                  "rotation": [0, round(random.uniform(0, 360), 1), 0], "scale": [round(random.uniform(12, 17), 2)] * 3})
entities.append(instanced("res/models/tree.obj", trees))

rocks = []
for a in range(0, 360, 9):
    r = shore_radius(a) + random.uniform(-25, 15)
    x, z = polar(a + random.uniform(-3, 3), r)
    s = random.uniform(5, 13)
    rocks.append({"position": [round(x, 2), round(H(x, z) - s * 0.3, 2), round(z, 2)],
                  "rotation": [round(random.uniform(-20, 20), 1), round(random.uniform(0, 360), 1), 0], "scale": [round(s, 2)] * 3})
entities.append(instanced("res/models/rock.obj", rocks, ROCK_MAT))

# flattened rocks: fire ring + path edging
stones = [{"position": r["position"], "rotation": r["rotation"], "scale": [3.0, 1.6, 3.0]} for r in ring]
for z in range(-110, -440, -22):
    for side in (-24, 24):
        x = SPAWN[0] + side + random.uniform(-4, 4)
        s_ = random.uniform(2.2, 3.4)
        stones.append({"position": [round(x, 2), round(H(x, z) - 0.4, 2), z],
                       "rotation": [0, round(random.uniform(0, 360), 1), 0], "scale": [round(s_, 2), round(s_ * 0.45, 2), round(s_, 2)]})
entities.append(instanced("res/models/rock.obj", stones, ROCK_MAT))

# --- lights --------------------------------------------------------------------
moon = [-0.25, 0.5, -0.83]   # towards the moon (~28 degrees up, above the northern hills)
lights.append(light((0, 0, 0), (0.16, 0.2, 0.32), "DIR", (0, 0, 0), dir=moon, cast_shadow=True,
                    diffuse_attenuation=[0, 0, 0], specular_attenuation=[0, 0, 0]))
# lighthouse: sweeping volumetric beam. coneAngle decouples the beam width
# from its range (the light volume, scale.x, just has to enclose the cone), so
# it reaches the hills and shore and leaves a pool of light where it lands
BEAM_ANGLE, BEAM_RANGE = 0.13, 1150
beam_volume = round(2 * BEAM_RANGE * math.tan(BEAM_ANGLE) * 1.08, 1)
lights.append(light((lamp[0], lamp[1] + 1, lamp[2]), (1.0, 0.93, 0.75), "SPOT", (beam_volume, beam_volume, BEAM_RANGE), rot=(-11, 180, 0),
                    cast_shadow=True, volumetric=True, numSamplePoints=48, coef1=0.8, coef2=0.02,
                    beamDensity=0.035, beamFalloff=280, coneAngle=BEAM_ANGLE, shadowMapSize=1024,
                    diffuse_attenuation=[30, 1, 0.1], specular_attenuation=[40, 1, 0.2], spin=20, nightOnly=True))
# statue uplights
for color, side in (((0.95, 0.3, 0.85), -1), ((0.3, 0.85, 1.0), 1)):
    src = (C[0] + side * 22, rock_top + 2, C[1] + 26)
    lights.append(light(src, color, "SPOT", (420, 420, 160), rot=aim(src, (buddha_pos[0], buddha_pos[1] + 8, buddha_pos[2])),
                        cast_shadow=True, volumetric=False,
                        diffuse_attenuation=[700, 10, 4], specular_attenuation=[500, 10, 4]))
# dragon uplight
dsrc = (dx + 45, H(dx + 45, dz) + 3, dz)
lights.append(light(dsrc, (0.35, 1.0, 0.55), "SPOT", (500, 500, 170), rot=aim(dsrc, (dx, H(dx, dz) + 40, dz)),
                    cast_shadow=True, volumetric=False,
                    diffuse_attenuation=[500, 10, 3], specular_attenuation=[400, 10, 3]))
# wisps circling over the lagoon
for i, color in enumerate(((0.3, 1.0, 0.75), (0.45, 0.6, 1.0), (0.9, 0.5, 1.0))):
    lights.append(light((C[0], WATER + 16, C[1]), color, "POINT", (190, 190, 190), cast_shadow=False,
                        diffuse_attenuation=[600, 10, 3.9], specular_attenuation=[90, 10, 2],
                        orbit={"center": [C[0], WATER + 16 + i * 4, C[1]], "radius": 150 + i * 45,
                               "speed": 12 + i * 5 * (-1 if i == 1 else 1), "bob": 6, "phase": i * 120}))
# fountain glow on the island
lights.append(light((C[0], rock_top + 4, C[1] - 18), (0.35, 0.75, 1.0), "POINT", (150, 150, 150), cast_shadow=True,
                    diffuse_attenuation=[600, 10, 3.9], specular_attenuation=[90, 10, 2], flicker=0.1, flickerSpeed=3))

# --- halos: light sources glowing in the haze -----------------------------------
lantern_globes = [l["Transform"]["position"] for l in lights if l["Attributes"].get("flicker") == 0.08]
emitters += [
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": lantern_globes, "startScale": 60,
     "blend": "additive", "tint": [1.0, 0.6, 0.28], "nightOnly": True},
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": [[fx, fy + 14, fz]], "startScale": 55,
     "blend": "additive", "tint": [1.0, 0.45, 0.15]},
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": [[lamp[0], lamp[1], lamp[2]]], "startScale": 110,
     "blend": "additive", "tint": [1.0, 0.92, 0.75], "nightOnly": True},
    {"type": "glow", "texture": "res/textures/glowWhite.png", "positions": [[C[0], rock_top + 6, C[1] - 18]], "startScale": 70,
     "blend": "additive", "tint": [0.3, 0.65, 0.95]},
]

# --- particles over the lagoon -----------------------------------------------
emitters += [
    {"type": "drift", "texture": "res/textures/glowWhite.png", "count": 160, "lifetimeMs": 5000,
     "position": [C[0], WATER + 22, C[1] + 120], "radius": 330, "height": 22, "speed": [4, 12], "wander": 14,
     "startScale": 2.6, "blend": "additive", "tint": [0.65, 1.0, 0.35], "fadeIn": 0.35, "nightOnly": True},
    {"type": "spray", "texture": "res/textures/glowWhite.png", "count": 260, "lifetimeMs": 2600,
     "position": [C[0], rock_top + 2, C[1] - 18], "radius": 2, "direction": [0, 1, 0], "spread": 0.18,
     "speed": [70, 105], "gravity": -70, "startScale": 2.2, "blend": "additive", "tint": [0.45, 0.8, 1.0]},
    {"type": "drift", "texture": "res/textures/flowerBurst.bmp", "count": 40, "lifetimeMs": 4000,
     "position": [buddha_pos[0], buddha_pos[1] + 5, buddha_pos[2]], "radius": 40, "height": 30, "speed": [3, 8],
     "wander": 8, "startScale": 6, "scaleJitter": 0.5, "blend": "additive", "tint": [0.9, 0.6, 1.0], "fadeIn": 0.4},
    {"type": "drift", "texture": "res/textures/smoke.png", "atlas": [8, 5], "count": 45, "lifetimeMs": 9000,
     "position": [C[0], WATER + 6, C[1]], "radius": 300, "height": 4, "speed": [3, 6], "wander": 4,
     "startScale": 70, "scaleJitter": 0.4, "blend": "alpha", "tint": [0.35, 0.42, 0.55], "brightness": 0.05,
     "fadeIn": 0.4},
]

game = {
    "ConfigFilePath": "res/configs/config.json",
    "gravity": -10.0,
    "PlayerStart": {"position": [SPAWN[0], H(*SPAWN) + 25, SPAWN[1]], "yaw": 0, "pitch": -4},
    "Environment": {"sky": "night", "zenithColor": [0.004, 0.008, 0.03], "horizonColor": [0.05, 0.07, 0.14],
                    "starIntensity": 1.0, "moonDirection": moon, "moonColor": [0.6, 0.7, 0.95],
                    "fogColor": [0.055, 0.07, 0.11], "fogDensity": 0.0024, "fogHeightFalloff": 0.0035,
                    "fogBaseHeight": WATER, "fogMoonGlow": 0.35, "ambient": 0.05, "emissiveStrength": 1.6,
                    # HDR + filmic tone mapping, bloom, ambient occlusion
                    "pbr": True,
                    "hdr": True, "exposure": 1.25, "bloomStrength": 0.5, "bloomThreshold": 0.9,
                    "aoStrength": 0.9, "aoRadius": 10,
                    # hemisphere ambient: moonlit sky above, dark ground below
                    "ambientSkyTint": [0.85, 0.95, 1.35], "ambientGroundTint": [0.45, 0.45, 0.5],
                    # day-night cycle: 8 minute days starting at midnight (T speeds it up)
                    "dayLength": 480, "timeOfDay": 0.0},
    "Water": {"height": WATER, "center": list(C), "size": [1150, 1150], "tiling": 90, "waveStrength": 0.016,
              "waveSpeed": 0.03, "waveHeight": 0.5, "shallowColor": [0.7, 0.85, 0.9], "deepColor": [0.0, 0.025, 0.05],
              "murkDepth": 45, "softEdge": 3, "reflectivity": 1.0},
    "Entities": entities,
    "Lights": lights,
    "ParticleEmitters": emitters,
}
out = sys.argv[1] if len(sys.argv) > 1 else 'Game/lagoon.json'
json.dump(game, open(out, 'w'), indent=1)
print("entities", len(entities), "trees", len(trees), "rocks", len(rocks), "stones", len(stones),
      "lights", len(lights), "emitters", len(emitters))
print("spawn", game["PlayerStart"], "campfire", (round(fx), round(fy), round(fz)), "lighthouse", [round(v) for v in lamp],
      "buddha", [round(v) for v in buddha_pos], "dragon", (round(dx), round(dz)))
