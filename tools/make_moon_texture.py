"""Generate res/textures/moon.png: a full-moon albedo map for the sky shader
(dark maria, craters with bright rims, a couple of ray systems, fine grain).
Only the inscribed circle is used. Needs only Pillow.

usage: python3 tools/make_moon_texture.py
"""
import math, random
from PIL import Image, ImageFilter

N = 512
random.seed(1969)


def lattice(n, seed):
    rnd = random.Random(seed)
    return [[rnd.random() for _ in range(n + 1)] for _ in range(n + 1)]


def noise(grid, n, x, y):
    x, y = x * n, y * n
    ix, iy = int(x), int(y)
    fx, fy = x - ix, y - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    ix, iy = min(ix, n - 1), min(iy, n - 1)
    a = grid[iy][ix] + (grid[iy][ix + 1] - grid[iy][ix]) * fx
    b = grid[iy + 1][ix] + (grid[iy + 1][ix + 1] - grid[iy + 1][ix]) * fx
    return a + (b - a) * fy


grids = [(4, lattice(4, 1)), (8, lattice(8, 2)), (16, lattice(16, 3)), (32, lattice(32, 4)),
         (64, lattice(64, 5)), (128, lattice(128, 6))]


def fbm(x, y, octaves, first=0):
    t, amp, norm = 0.0, 1.0, 0.0
    for n, g in grids[first:first + octaves]:
        t += noise(g, n, x, y) * amp
        norm += amp
        amp *= 0.5
    return t / norm


# --- base: highlands with maria (dark basalt plains), mostly on one side --------
albedo = [[0.0] * N for _ in range(N)]
for y in range(N):
    v = y / N
    for x in range(N):
        u = x / N
        m = fbm(u, v, 3)
        # the near side's maria cluster towards the upper left
        bias = 0.16 * (1.0 - math.hypot(u - 0.38, v - 0.36) * 1.6)
        mare = min(1.0, max(0.0, (m + bias - 0.5) / 0.16))
        mare = mare * mare * (3 - 2 * mare)
        grain = fbm(u, v, 3, 3)
        mottle = fbm(u, v, 2, 2)
        a = 0.70 - 0.30 * mare + 0.10 * (grain - 0.5) + 0.08 * (mottle - 0.5)
        albedo[y][x] = a

# --- craters: power-law sizes, darker floor, bright rim, faint ejecta ----------
def crater(cx, cy, r, depth):
    rr = int(r * 2.2) + 2
    for y in range(max(0, int(cy - rr)), min(N, int(cy + rr))):
        for x in range(max(0, int(cx - rr)), min(N, int(cx + rr))):
            d = math.hypot(x - cx, y - cy) / r
            if d < 0.85:
                albedo[y][x] -= depth * 0.035 * (1.0 - d / 0.85) ** 0.5
            elif d < 1.15:
                albedo[y][x] += depth * 0.05 * (1.0 - abs(d - 1.0) / 0.15)
            elif d < 2.2:
                albedo[y][x] += depth * 0.02 * (2.2 - d) / 1.05


for _ in range(420):
    r = 1.0 + 18.0 * random.random() ** 5.0
    crater(random.uniform(0, N), random.uniform(0, N), r, random.uniform(0.6, 1.0))

# --- ray systems (young craters, like Tycho / Copernicus), blurred soft ---------
rays_layer = [[0.0] * N for _ in range(N)]
for cx, cy, r, rays in ((300, 395, 9, 22), (215, 205, 7, 14)):
    for k in range(rays):
        ang = random.uniform(0, 2 * math.pi)
        length = random.uniform(70, 210)
        width = random.uniform(2.0, 4.0)
        for s in range(int(length)):
            t = s / length
            px = cx + math.cos(ang) * (r + s)
            py = cy + math.sin(ang) * (r + s)
            for w in range(-3, 4):
                qx = int(px - math.sin(ang) * w)
                qy = int(py + math.cos(ang) * w)
                if 0 <= qx < N and 0 <= qy < N:
                    rays_layer[qy][qx] += 0.05 * (1.0 - t) ** 1.5 * math.exp(-(w * w) / (width * width))
    crater(cx, cy, r, 2.5)

rimg = Image.new('L', (N, N))
rimg.putdata([max(0, min(255, int(rays_layer[y][x] * 600))) for y in range(N) for x in range(N)])
rimg = rimg.filter(ImageFilter.GaussianBlur(2.5))
rp = rimg.load()
for y in range(N):
    for x in range(N):
        albedo[y][x] += rp[x, y] / 600.0

img = Image.new('L', (N, N))
img.putdata([max(0, min(255, int(albedo[y][x] * 255))) for y in range(N) for x in range(N)])
img = img.filter(ImageFilter.GaussianBlur(0.8))
img.convert('RGB').save('res/textures/moon.png')
print('res/textures/moon.png')
