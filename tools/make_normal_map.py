"""Derive a tileable tangent-space normal map from a color texture.

Brightness is treated as height (dark crevices low, bright tips high); the
gradient wraps around the edges, so the result tiles like the source.
Needs only Pillow.

usage: python3 tools/make_normal_map.py input.bmp output.png [strength] [blur]
"""
import sys
from math import sqrt
from PIL import Image, ImageFilter

src, dst = sys.argv[1], sys.argv[2]
strength = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
blur = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0

image = Image.open(src).convert('L')
w, h = image.size
if blur > 0:
    # tile 3x3 before blurring so the edges stay seamless
    big = Image.new('L', (w * 3, h * 3))
    for y in range(3):
        for x in range(3):
            big.paste(image, (x * w, y * h))
    image = big.filter(ImageFilter.GaussianBlur(blur)).crop((w, h, 2 * w, 2 * h))

height = list(image.getdata())
scale = strength * 8.0 / 255.0 * 0.5
out = bytearray(w * h * 3)
for y in range(h):
    row, up, down = y * w, ((y - 1) % h) * w, ((y + 1) % h) * w
    for x in range(w):
        left, right = (x - 1) % w, (x + 1) % w
        nx = -(height[row + right] - height[row + left]) * scale
        ny = (height[down + x] - height[up + x]) * scale
        inv = 1.0 / sqrt(nx * nx + ny * ny + 1.0)
        i = (row + x) * 3
        out[i] = int((nx * inv * 0.5 + 0.5) * 255)
        out[i + 1] = int((ny * inv * 0.5 + 0.5) * 255)
        out[i + 2] = int((inv * 0.5 + 0.5) * 255)
Image.frombytes('RGB', (w, h), bytes(out)).save(dst)
print(f"{dst}: {w}x{h}, strength {strength}")
