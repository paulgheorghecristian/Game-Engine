# Generates res/models/tree_leafy.obj: tree.obj with leaves that read as foliage, not paper.
#
# tree.obj's leaf cards each show one leaf (res/textures/leaf.bmp) a metre wide, in a neon
# lime green, glossy: the crowns looked like a few big cut-outs. This writes:
#   res/textures/leaf_cluster.png            many small leaves a card, gaps between them (black:
#                                            cut out, "-blka"), muted greens, each leaf its own shade
#   res/textures/leaf_cluster_normal.png     each leaf tilted its own way, folded along its midrib:
#                                            the sun catches some and not others
#   res/textures/leaf_cluster_roughness.png  matte (the PBR roughness, read from map_Ks)
#   res/models/tree_leafy.mtl / .obj         tree.obj's mesh with those leaves (the bark as it was)
#
# Usage (from the repository root):  python3 tools/make_leafy_tree.py
import math
import random
from PIL import Image, ImageDraw

random.seed(7)
W, H = 512, 1024                     # the leaf cards are about twice as tall as wide (leaf.bmp: 460x936)
LEAVES = 300
SS = 2                               # drawn at twice the size, then reduced (smooth edges)

color = Image.new('RGB', (W * SS, H * SS), (0, 0, 0))
normal = Image.new('RGB', (W * SS, H * SS), (128, 128, 255))
rough = Image.new('RGB', (W * SS, H * SS), (220, 220, 220))
dc, dn, dr = ImageDraw.Draw(color), ImageDraw.Draw(normal), ImageDraw.Draw(rough)


def leaf_outline(cx, cy, length, width, angle, side):
    """one half of a leaf (side -1 / +1 of its midrib): base to tip, as a polygon"""
    pts = []
    steps = 10
    for i in range(steps + 1):
        t = i / steps
        along = (t - 0.5) * length
        across = side * width * 0.5 * math.sin(math.pi * t) ** 0.8 * (1.0 - 0.25 * t)
        pts.append((along, across))
    pts.append((0.5 * length, 0.0))
    pts.append((-0.5 * length, 0.0))
    ca, sa = math.cos(angle), math.sin(angle)
    return [(cx + x * ca - y * sa, cy + x * sa + y * ca) for x, y in pts]


def encode(n):
    l = math.sqrt(sum(c * c for c in n))
    return tuple(int(round((c / l * 0.5 + 0.5) * 255)) for c in n)


# the card's outline (as leaf.bmp's: an ellipse inside the card); leaves are placed inside it,
# the ones further back first, so the front ones overlap them
for k in range(LEAVES):
    while True:
        u, v = random.uniform(-1, 1), random.uniform(-1, 1)
        if u * u + v * v < 0.82:
            break
    cx = (0.5 + 0.44 * u) * W * SS
    cy = (0.5 + 0.46 * v) * H * SS
    length = random.uniform(0.12, 0.17) * H * SS * 0.5
    width = length * random.uniform(0.42, 0.55)
    # leaves point outwards and down-ish from the twig, with some scatter
    angle = math.atan2(v, u) * 0.6 + random.uniform(-0.9, 0.9) + math.pi * 0.5
    # muted, varied greens: some yellower, some bluer, some in the shade of the others
    shade = random.uniform(0.62, 1.12) * (0.82 + 0.18 * k / LEAVES)
    hue = random.uniform(-1, 1)
    base = (92 + 14 * hue, 128 + 6 * hue, 34 - 8 * hue)
    tilt = (random.uniform(-0.55, 0.55), random.uniform(-0.55, 0.55))
    for side in (-1, 1):
        poly = leaf_outline(cx, cy, length, width, angle, side)
        lit = 1.0 + 0.07 * side                       # the two halves of the fold catch the light apart
        c = tuple(max(36, min(255, int(ch * shade * lit))) for ch in base)
        dc.polygon(poly, fill=c)
        # folded along the midrib: each half tilted away from it, on top of the leaf's own tilt
        fold = 0.35 * side
        nx = tilt[0] - fold * math.sin(angle)
        ny = tilt[1] + fold * math.cos(angle)
        dn.polygon(poly, fill=encode((nx, ny, 1.0)))
        r = int(random.uniform(0.78, 0.92) * 255)
        dr.polygon(poly, fill=(r, r, r))
    # the midrib, a little lighter
    ca, sa = math.cos(angle), math.sin(angle)
    a = (cx - 0.5 * length * ca, cy - 0.5 * length * sa)
    b = (cx + 0.45 * length * ca, cy + 0.45 * length * sa)
    rib = tuple(max(40, min(255, int(ch * shade * 1.18))) for ch in base)
    dc.line([a, b], fill=rib, width=max(1, SS))

color.resize((W, H), Image.LANCZOS).save('res/textures/leaf_cluster.png')
normal.resize((W, H), Image.LANCZOS).save('res/textures/leaf_cluster_normal.png')
rough.resize((W, H), Image.LANCZOS).save('res/textures/leaf_cluster_roughness.png')

# the material: the leaves' textures swapped, matte; the bark as tree.mtl has it
mtl = open('res/models/tree.mtl').read()
leaves, bark = mtl.split('newmtl Material.004')
leaves = leaves.replace('map_Kd -blka -dcul res/textures/leaf.bmp',
                        'map_Kd -blka -dcul res/textures/leaf_cluster.png')
leaves = leaves.replace('map_Bump -bm 0.500000 res/textures/leaf_normal.png',
                        'map_Bump -bm 1.000000 res/textures/leaf_cluster_normal.png\n'
                        'map_Ks res/textures/leaf_cluster_roughness.png')
leaves = leaves.replace('Ks 0.500000 0.500000 0.500000', 'Ks 0.050000 0.050000 0.050000')
# (full diffuse: the texture has the leaves' colour, and its gaps darken it further off, in its mipmaps)
leaves = leaves.replace('Kd 0.640000 0.640000 0.640000', 'Kd 1.000000 1.000000 1.000000')
assert 'Kd 1.0' in leaves and 'leaf_cluster.png' in leaves and 'leaf_cluster_roughness.png' in leaves and 'Ks 0.05' in leaves
open('res/models/tree_leafy.mtl', 'w').write(leaves + 'newmtl Material.004' + bark)

obj = open('res/models/tree.obj').read()
assert obj.count('mtllib tree.mtl') == 1
open('res/models/tree_leafy.obj', 'w').write(obj.replace('mtllib tree.mtl', 'mtllib tree_leafy.mtl'))
print('wrote res/models/tree_leafy.obj, .mtl and res/textures/leaf_cluster*.png')
