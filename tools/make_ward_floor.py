"""The ward's floor texture: linoleum tiles with their joints drawn in, 2 x 2 tiles a picture
(Game/recursion/astra/ward_floor_tiles.jpg, from astra's ward_linoleum.jpg). A tile is 12 world
units, so the material tiles it with "tileSize": 24. The joints were thin strips of geometry before:
thinner than a pixel from across the room, they broke into dashes and vanished; in the texture they
fade evenly with distance.

    python3 tools/make_ward_floor.py
"""
import os, random
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SOURCE = os.path.join(ROOT, 'Game/recursion/astra/ward_linoleum.jpg')
OUT = os.path.join(ROOT, 'Game/recursion/astra/ward_floor_tiles.jpg')
SIZE, TILE = 1024, 512
JOINT = 5               # pixels: 0.1 of a 12 unit tile

random.seed(7)
linoleum = Image.open(SOURCE).convert('RGB')
floor = Image.new('RGB', (SIZE, SIZE))
for row in range(2):
    for column in range(2):
        # each tile its own piece of the linoleum, turned, a touch lighter or darker
        x, y = random.randrange(0, 1024 - TILE), random.randrange(0, 1024 - TILE)
        tile = linoleum.crop((x, y, x + TILE, y + TILE)).rotate(90 * random.randrange(4))
        tile = ImageEnhance.Brightness(tile).enhance(random.uniform(0.97, 1.03))
        floor.paste(tile, (column * TILE, row * TILE))
# the joints: along every tile's edges (the picture's own edges too: it repeats there)
joints = Image.new('L', (SIZE, SIZE), 0)
draw = ImageDraw.Draw(joints)
for edge in (0, TILE, SIZE):
    draw.rectangle((edge - JOINT // 2, 0, edge + JOINT // 2, SIZE), fill=255)
    draw.rectangle((0, edge - JOINT // 2, SIZE, edge + JOINT // 2), fill=255)
joints = joints.filter(ImageFilter.GaussianBlur(0.8))
dark = Image.eval(floor, lambda v: int(v * 0.3))
floor = Image.composite(dark, floor, joints)
floor.save(OUT, quality=92)
print('wrote', os.path.relpath(OUT, ROOT), floor.size)
