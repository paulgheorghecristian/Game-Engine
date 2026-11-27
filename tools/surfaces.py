"""What the levels' surfaces are made of: the free CC0 texture sets from ambientCG (res/textures/
<set>_1K_{Color,Normal,Roughness}.jpg) tiled by world size ("tileSize": no UVs needed), and the ward's
surfaces made of them, for any level built like it (tools/export_astra_room.py, tools/make_recursion.py).
"""

# mean colour of each Color map: a textured material's diffuse is divided by it, so the surface keeps the
# colour it was given (the texture adds the variation, not a different colour)
SET_MEAN = {
    'PaintedPlaster017': (0.745, 0.748, 0.732),
    'OfficeCeiling001': (0.958, 0.958, 0.958),
    'Metal009': (0.573, 0.583, 0.596),
    'Metal027': (0.180, 0.176, 0.206),
    'Fabric086': (0.662, 0.666, 0.684),
    'Plastic018B': (0.469, 0.469, 0.469),
    'Rubber004': (0.164, 0.170, 0.194),
}


def look(material, texture_set, tile, color=True, normal=1.0, **scalars):
    """material with a texture set tiled every `tile` world units; color=False: only its relief and
    roughness (the material keeps its flat colour)"""
    path = 'res/textures/%s_1K_' % texture_set
    if color:
        mean = SET_MEAN[texture_set]
        material['diffuse'] = [round(c / m, 4) for c, m in zip(material['diffuse'], mean)]
        material['Texture'] = path + 'Color.jpg'
    material['NormalMapTexture'] = path + 'Normal.jpg'
    material['normalMapStrength'] = normal
    material['RoughnessTexture'] = path + 'Roughness.jpg'
    material['tileSize'] = tile
    material.update(scalars)
    return material


# ---- the ward's surfaces (each takes a material of the right colour and returns it textured) ------------
def ward_wall(m):           # painted plaster, a light relief
    return look(m, 'PaintedPlaster017', 70, normal=0.3)


def ward_band(m):           # the wipe-clean lower band: plastic sheet
    return look(m, 'Plastic018B', 40, color=False)


def ward_ceiling(m):        # ceiling tiles
    return look(m, 'OfficeCeiling001', 90, normal=0.35)


def ward_floor(m):          # vinyl tiles with their joints (tools/make_ward_floor.py): 12 unit tiles
    m.update(Texture='Game/recursion/astra/ward_floor_tiles.jpg', diffuse=[.62, .64, .62], roughness=.58,
             specularStrength=.28, tileSize=24)
    return m


def door_coat(m):           # a steel door's powder coat: paint, not metal (its reflection a faint sheen)
    return look(m, 'Metal027', 30, color=False, normal=0.6, metallic=0.0)
