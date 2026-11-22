"""Generate tileable desert textures for the canyon map (needs only Pillow).

  res/textures/desert_sand.png        wind-rippled sand
  res/textures/desert_sand2.png       coarser, darker sand with pebbles
  res/textures/desert_sandstone.png   layered red sandstone (horizontal strata)

Normal maps are derived afterwards with tools/make_normal_map.py (see the
end of this file). Everything wraps at the edges, so the textures tile.

usage: python3 tools/make_desert_textures.py
"""
import math, random, subprocess, sys
from PIL import Image

N = 512
random.seed(11)


def lattice(period, seed):
    rnd = random.Random(seed)
    return [[rnd.random() for _ in range(period)] for _ in range(period)]


def value_noise(grid, period, u, v):
    """tileable value noise; u, v in [0, 1)"""
    x, y = u * period, v * period
    x0, y0 = int(x) % period, int(y) % period
    x1, y1 = (x0 + 1) % period, (y0 + 1) % period
    fx, fy = x - int(x), y - int(y)
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a = grid[y0][x0] + (grid[y0][x1] - grid[y0][x0]) * fx
    b = grid[y1][x0] + (grid[y1][x1] - grid[y1][x0]) * fx
    return a + (b - a) * fy


def fbm_maker(base_period, octaves, seed):
    grids = [(base_period << o, lattice(base_period << o, seed + o)) for o in range(octaves)]
    def fbm(u, v):
        total, amp, norm = 0.0, 1.0, 0.0
        for period, grid in grids:
            total += value_noise(grid, period, u, v) * amp
            norm += amp
            amp *= 0.5
        return total / norm
    return fbm


def clamp8(c):
    return tuple(max(0, min(255, int(round(x * 255)))) for x in c)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def sand(path, base, dark, ripple_strength, pebbles, seed):
    warp = fbm_maker(4, 3, seed)
    grain = fbm_maker(32, 3, seed + 10)
    tone = fbm_maker(4, 4, seed + 20)
    img = Image.new('RGB', (N, N))
    px = img.load()
    for y in range(N):
        v = y / N
        for x in range(N):
            u = x / N
            # ripples: integer wave numbers keep them seamless, warped by noise
            phase = 2 * math.pi * (3 * u + 7 * v) + 6.0 * warp(u, v)
            ripple = 0.5 + 0.5 * math.sin(phase)
            ripple = ripple ** 3  # sharp crests, wide troughs
            g = grain(u, v)
            t = tone(u, v)
            c = mix(dark, base, 0.55 + 0.45 * t)
            shade = 0.86 + ripple_strength * ripple + 0.22 * (g - 0.5)
            px[x, y] = clamp8(tuple(ch * shade for ch in c))
    if pebbles:
        rnd = random.Random(seed + 30)
        for _ in range(pebbles):
            cx, cy = rnd.randrange(N), rnd.randrange(N)
            r = rnd.uniform(1.0, 3.2)
            col = mix((0.35, 0.25, 0.18), (0.75, 0.62, 0.5), rnd.random())
            for dy in range(-4, 5):
                for dx in range(-4, 5):
                    d = math.hypot(dx, dy)
                    if d <= r:
                        xx, yy = (cx + dx) % N, (cy + dy) % N
                        lit = 1.0 + 0.25 * (-dx - dy) / (r + 1)   # lit from the top left
                        px[xx, yy] = clamp8(tuple(ch * lit for ch in col))
    img.save(path)


def sandstone(path, seed):
    """strata run along u (they become horizontal bands on cliff faces with
    the terrain's triplanar mapping, whose side projections put y on v)"""
    warp = fbm_maker(2, 3, seed)
    grain = fbm_maker(16, 5, seed + 5)
    crack = fbm_maker(8, 4, seed + 9)
    palette = [(0.62, 0.30, 0.18), (0.78, 0.45, 0.26), (0.88, 0.66, 0.46), (0.55, 0.24, 0.15),
               (0.72, 0.40, 0.24), (0.84, 0.58, 0.38), (0.48, 0.22, 0.14), (0.80, 0.52, 0.32)]
    rnd = random.Random(seed)
    bands = 24                                     # bands per tile (must stay an integer)
    band_colors = [palette[rnd.randrange(len(palette))] for _ in range(bands)]
    img = Image.new('RGB', (N, N))
    px = img.load()
    for y in range(N):
        v = y / N
        for x in range(N):
            u = x / N
            s = (v + 0.035 * (warp(u, v) - 0.5)) * bands
            i0 = int(math.floor(s)) % bands
            f = s - math.floor(s)
            c = mix(band_colors[i0], band_colors[(i0 + 1) % bands], max(0.0, (f - 0.8) / 0.2))
            g = grain(u, v)
            k = crack(u, v)
            shade = 0.86 + 0.38 * (g - 0.5)
            if abs(k - 0.5) < 0.005:               # faint hairline fissures
                shade *= 0.82
            # eroded lips at the band boundaries
            shade *= 0.9 + 0.1 * math.sin(f * math.pi)
            px[x, y] = clamp8(tuple(ch * shade for ch in c))
    img.save(path)


if __name__ == '__main__':
    sand('res/textures/desert_sand.png', (0.90, 0.70, 0.47), (0.74, 0.52, 0.33), 0.16, 0, 1)
    sand('res/textures/desert_sand2.png', (0.80, 0.56, 0.37), (0.62, 0.40, 0.26), 0.06, 900, 2)
    sandstone('res/textures/desert_sandstone.png', 3)
    for name, strength, blur in (('desert_sand', 2.0, 1.2), ('desert_sand2', 2.5, 0.8),
                                 ('desert_sandstone', 3.0, 1.0)):
        subprocess.check_call([sys.executable, 'tools/make_normal_map.py', 'res/textures/%s.png' % name,
                               'res/textures/%s_normal.png' % name, str(strength), str(blur)])
    print('done')
