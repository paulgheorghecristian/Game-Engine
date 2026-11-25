# Makes a smooth, finer copy of a terrain mesh (an OBJ): every triangle split
# into n x n smaller ones, the new points bowed out along the surface's
# curvature (Phong tessellation, Boubekeur & Alexa 2008), so a low-poly
# terrain's hills are round instead of made of flat facets and straight
# outlines. The original vertices stay where they are (the shape and heights
# are kept; only what lies between them is rounded), and triangles that share
# an edge compute it the same way (no cracks).
#
# Usage (from the repository root):
#   python3 tools/make_smooth_terrain.py res/models/terrain4.obj res/models/terrain4_smooth.obj \
#       [--split 4] [--scale 4800 1600 4800]
#
# --scale: how the maps scale the mesh (its entity's Transform). The curvature
# is worked out at that scale, so a mesh stretched sideways (4800 x 1600 x
# 4800) is rounded as it is seen, not as its unit-sized file.
import argparse, math

ap = argparse.ArgumentParser()
ap.add_argument('source')
ap.add_argument('target')
ap.add_argument('--split', type=int, default=4, help='each triangle edge into this many (n x n triangles each)')
ap.add_argument('--scale', type=float, nargs=3, default=(4800.0, 1600.0, 4800.0))
ap.add_argument('--alpha', type=float, default=0.75, help='how far points bow out (0: flat, 1: fully)')
args = ap.parse_args()
S = args.scale
N = args.split

def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def normalize(a):
    l = math.sqrt(dot(a, a))
    return mul(a, 1.0 / l) if l > 0 else (0.0, 1.0, 0.0)

# --- read (positions at the maps' scale) ------------------------------------
V, T, F = [], [], []          # F: [(v, vt), (v, vt), (v, vt)] (0-based; vt -1: none)
for line in open(args.source):
    p = line.split()
    if not p:
        continue
    if p[0] == 'v':
        V.append((float(p[1]) * S[0], float(p[2]) * S[1], float(p[3]) * S[2]))
    elif p[0] == 'vt':
        T.append((float(p[1]), float(p[2])))
    elif p[0] == 'f':
        corners = []
        for c in p[1:]:
            parts = c.split('/')
            corners.append((int(parts[0]) - 1, int(parts[1]) - 1 if len(parts) > 1 and parts[1] else -1))
        for i in range(1, len(corners) - 1):     # (polygons as fans)
            F.append([corners[0], corners[i], corners[i + 1]])

# --- the surface's normal at each vertex (area-weighted, at the maps' scale) --
VN = [(0.0, 0.0, 0.0)] * len(V)
for f in F:
    a, b, c = (V[f[k][0]] for k in range(3))
    n = cross(sub(b, a), sub(c, a))           # (length: twice the area)
    for k in range(3):
        VN[f[k][0]] = add(VN[f[k][0]], n)
VN = [normalize(n) for n in VN]

# --- tessellate ---------------------------------------------------------------
out_v, out_t, keys = [], [], {}
moved = []                    # how far up / down each new point moved from the flat facets
def point(f, i, j):
    """the point at barycentric (i, j, N - i - j) / N of face f, made once per place: corners by
    their vertex, edge points by their edge (the same from both faces), inner ones by their face"""
    w = (i / N, j / N, (N - i - j) / N)
    idx = [f[0][0], f[1][0], f[2][0]]
    zero = [k for k in range(3) if (i, j, N - i - j)[k] == 0]
    if len(zero) == 2:
        key = ('v', idx[[k for k in range(3) if k not in zero][0]])
    elif len(zero) == 1:
        a, b = [k for k in range(3) if k not in zero]
        if idx[a] > idx[b]:
            a, b = b, a
        key = ('e', idx[a], idx[b], round(w[b], 6))
    else:
        key = ('f', id(f), i, j)
    if key in keys:
        return keys[key]
    p = (0.0, 0.0, 0.0)
    for k in range(3):
        p = add(p, mul(V[idx[k]], w[k]))
    # Phong tessellation: the flat point projected onto each corner's tangent plane, blended
    q = (0.0, 0.0, 0.0)
    for k in range(3):
        n = VN[idx[k]]
        q = add(q, mul(sub(p, mul(n, dot(sub(p, V[idx[k]]), n))), w[k]))
    flat = p
    p = add(mul(p, 1.0 - args.alpha), mul(q, args.alpha))
    moved.append(abs(p[1] - flat[1]))
    uv = (0.0, 0.0)
    if all(f[k][1] >= 0 for k in range(3)):
        uv = (sum(T[f[k][1]][0] * w[k] for k in range(3)), sum(T[f[k][1]][1] * w[k] for k in range(3)))
    keys[key] = len(out_v)
    out_v.append(p)
    out_t.append(uv)
    return keys[key]

out_f = []
for f in F:
    grid = {}
    for i in range(N + 1):
        for j in range(N + 1 - i):
            grid[(i, j)] = point(f, i, j)
    # (i, j): weight of corners 0 and 1; triangles keep the face's winding
    for i in range(N):
        for j in range(N - i):
            a, b, c = grid[(i + 1, j)], grid[(i, j + 1)], grid[(i, j)]
            out_f.append((a, b, c))
            if i + j < N - 1:
                out_f.append((grid[(i + 1, j)], grid[(i + 1, j + 1)], grid[(i, j + 1)]))

# winding as the source's (corner 0, 1, 2 order): checked on the first face
f0 = F[0]
src_n = cross(sub(V[f0[1][0]], V[f0[0][0]]), sub(V[f0[2][0]], V[f0[0][0]]))
a, b, c = out_f[0]
if dot(cross(sub(out_v[b], out_v[a]), sub(out_v[c], out_v[a])), src_n) < 0:
    out_f = [(a, c, b) for a, b, c in out_f]

# --- normals of the new surface (area-weighted), back to the file's scale ----
ON = [(0.0, 0.0, 0.0)] * len(out_v)
for a, b, c in out_f:
    n = cross(sub(out_v[b], out_v[a]), sub(out_v[c], out_v[a]))
    for k in (a, b, c):
        ON[k] = add(ON[k], n)
# a normal under a scale S transforms by S^-1: in the file it is S times the scaled one
ON = [normalize((n[0] * S[0], n[1] * S[1], n[2] * S[2])) for n in ON]

with open(args.target, 'w') as out:
    out.write('# %s: %s, each triangle split %dx%d, Phong-tessellated (tools/make_smooth_terrain.py)\n'
              % (args.target, args.source, N, N))
    for p in out_v:
        out.write('v %.6f %.6f %.6f\n' % (p[0] / S[0], p[1] / S[1], p[2] / S[2]))
    for t in out_t:
        out.write('vt %.6f %.6f\n' % t)
    for n in ON:
        out.write('vn %.6f %.6f %.6f\n' % n)
    for a, b, c in out_f:
        out.write('f %d/%d/%d %d/%d/%d %d/%d/%d\n' % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1))
print('%s: %d triangles -> %s: %d vertices, %d triangles' % (args.source, len(F), args.target, len(out_v), len(out_f)))
moved.sort()
print('height change from the flat facets (map units): median %.2f, 99%% %.2f, most %.2f'
      % (moved[len(moved) // 2], moved[int(len(moved) * 0.99)], moved[-1]))
