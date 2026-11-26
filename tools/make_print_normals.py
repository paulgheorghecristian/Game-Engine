"""Normal maps (OpenGL's: green up) for the hospital room's printed pictures, made after
tools/export_astra_room.py drew them (it runs this at its end):

- calendar_normal.png: the calendar's page crumpled: straight fold creases across it, flat facets
  between them (a sum of tent-shaped folds, each along a random line).

    python3 tools/make_print_normals.py
"""
import math, os, random
from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DIR = os.path.join(ROOT, 'Game/recursion/astra')


def normal_from_height(height, strength):
    """height: an 'L' image (lighter = higher). Returns the RGB normal map: x = -dh/du, y = -dh/dv,
    z up (the engine normalises it). Image rows run down, v up: dh/dv = -dh/drow"""
    sobel_x = ImageFilter.Kernel((3, 3), [1, 0, -1, 2, 0, -2, 1, 0, -1], scale=8.0 / strength, offset=128)
    sobel_y = ImageFilter.Kernel((3, 3), [-1, -2, -1, 0, 0, 0, 1, 2, 1], scale=8.0 / strength, offset=128)
    red = height.filter(sobel_x)                 # 128 - k dh/dcol
    green = height.filter(sobel_y)               # 128 + k dh/drow = 128 - k dh/dv
    blue = Image.new('L', height.size, 255)
    return Image.merge('RGB', (red, green, blue))


def calendar():
    page = Image.open(os.path.join(DIR, 'calendar.png'))
    w, h = page.size
    # worked out small and scaled up, the page's own proportions. Each fold tilts the paper one way on
    # one side of its line and the other way on the other: the slope is summed exactly (not taken from
    # a height picture: in 8 bits the facets' gentle slopes rounded away to nothing)
    sw, sh = 300, max(8, round(300 * h / w))
    random.seed(11)
    folds = []
    for _ in range(26):
        angle = random.uniform(0, math.pi)
        dx, dy = math.cos(angle), math.sin(angle)
        folds.append((dx, dy, random.uniform(-0.45, 0.45), random.uniform(0.3, 1.0) * random.choice((-1, 1))))
    TILT = 0.055            # how steep the facets lean (a sheet that was screwed up and smoothed out)
    red, green, blue = [], [], []
    for y in range(sh):
        py = (y / (sh - 1) - 0.5) * sh / sw            # down the page
        for x in range(sw):
            px = x / (sw - 1) - 0.5
            gx = gy = 0.0
            for dx, dy, offset, depth in folds:
                side = 1.0 if px * dx + py * dy - offset > 0 else -1.0
                gx += depth * side * dx
                gy += depth * side * dy
            nx, ny, nz = -gx * TILT, gy * TILT, 1.0    # (rows run down, v up: dh/dv = -dh/dpy)
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            red.append(int(128 + 127 * nx / length))
            green.append(int(128 + 127 * ny / length))
            blue.append(int(128 + 127 * nz / length))
    bands = []
    for values in (red, green, blue):
        band = Image.new('L', (sw, sh))
        band.putdata(values)
        bands.append(band)
    normal = Image.merge('RGB', bands).resize((w, h), Image.BICUBIC).filter(ImageFilter.GaussianBlur(1.2))
    normal.save(os.path.join(DIR, 'calendar_normal.png'))

calendar()
print('wrote calendar_normal.png')
