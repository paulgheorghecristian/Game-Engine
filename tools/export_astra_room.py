"""Exports astra's hospital room (Game/recursion/astra/hospital_room.blend) into the level
Game/recursion/01_beginning_astra.json, built on Game/recursion/01_beginning.json (its walls, floor,
collision, lights, timeline): the old box furniture is hidden and the room's modelled props drawn instead.

    blender -b --python tools/export_astra_room.py

- The room's meshes are written per material (the engine draws one material per entity) to
  Game/recursion/astra/room_<material>.obj. Things the player will use are kept apart, each its own
  entities (room_<thing>_<material>.obj), so they can move or react on their own: the door's fittings
  (they move with the door) and the keypad.
- Materials get textures (free CC0 sets from ambientCG, in res/textures) tiled by world size
  ("tileSize": no UVs needed), and the old walls, ceiling and door get the same.
- The old entities replaced by the model are found by where they are, not by their place in the list:
  if 01_beginning.json moves one, the export stops and says which.
"""
import bpy, json, os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'Game/recursion/astra')
BASE = os.path.join(ROOT, 'Game/recursion/01_beginning.json')
LEVEL = os.path.join(ROOT, 'Game/recursion/01_beginning_astra.json')

# ---- textures: res/textures/<set>_1K_{Color,Normal,Roughness}.jpg (ambientCG, CC0) -------------------
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


# per Blender material: what it is made of
MATERIAL_LOOK = {
    'steel': lambda m: look(m, 'Metal009', 24, metallic=0.5),   # fully metal goes black: little to reflect
    'R7_chrome': lambda m: m.update(metallic=0.8, roughness=0.25) or m,
    'blanket': lambda m: look(m, 'Fabric086', 10),
    'linen': lambda m: look(m, 'Fabric086', 8),
    'ivory': lambda m: look(m, 'Plastic018B', 30, color=False),
    'R7_bone': lambda m: look(m, 'Plastic018B', 30, color=False),
    'R7_white': lambda m: look(m, 'Plastic018B', 30, color=False),
    'R7_blue': lambda m: look(m, 'Plastic018B', 30, color=False),
    'rubber': lambda m: look(m, 'Rubber004', 12, color=False),
    'R7_wall': lambda m: look(m, 'PaintedPlaster017', 70, normal=0.3),
    'glass': lambda m: m.update(glass=True, roughness=0.05) or m,
    'screen': lambda m: m.update(roughness=0.08) or m,
}

# ---- what is kept apart (Blender coordinates, z up: the centre of each object's bounds) ---------------
GROUPS = {
    'door': ((-11.4, -90.0, 0.0), (11.4, -85.5, 53.0)),     # fittings on the door leaf, not its frame
    'keypad': ((16.0, -86.5, 29.0), (21.5, -84.0, 35.5)),
}

# ---- left out of the level: the window (its wall pieces, frame, sill, blinds) and the street outside it;
# the old level's plain wall stands there instead
LEFT_OUT_NAMES = ('Window ', 'Partially raised blind', 'Blind ladder', 'Blind wand',
                  # drawn into textures instead (as geometry, thinner than a pixel from across the room,
                  # they broke into dashes and vanished): the floor's joints (tools/make_ward_floor.py)
                  # and the calendar's print (bake_calendar)
                  'Floor tile seam', 'Calendar', 'Weekday')
OUTSIDE_Y = 90.6        # beyond the window wall's outer face (Blender y)


def left_out(obj, centre):
    return obj.name.startswith(LEFT_OUT_NAMES) or centre[1] > OUTSIDE_Y


# ---- the old level's entities: where they are (engine coordinates) -------------------------------------
HIDE = [  # replaced by the model
    (8.68, 24.8, 89.28), (18.6, 32.24, 86.06), (18.6, 34.47, 85.31),   # door handle, keypad, its light
    # the bed and the drip stand
    (-57.04, 11.16, -14.88), (-57.04, 13.64, -14.88), (-57.04, 16.37, -34.72), (-57.04, 18.6, -40.92),
    (-67.46, 4.96, -39.18), (-67.46, 4.96, 9.42), (-46.62, 4.96, -39.18), (-46.62, 4.96, 9.42),
    (-39.68, 23.56, -34.72), (-39.68, 42.66, -34.72),
    # the desk and what is on it
    (64.48, 18.23, -12.4), (57.04, 8.93, -29.76), (57.04, 8.93, 4.96), (71.92, 8.93, -29.76),
    (71.92, 8.93, 4.96), (67.7, 23.31, -14.88), (62.62, 23.56, -14.88), (59.52, 18.97, -14.88),
    (62.86, 18.75, 1.66), (65.24, 18.8, 1.08), (61.41, 18.85, 1.1), (60.55, 18.9, 0.95),
    (65.62, 18.95, -1.32), (64.83, 19.0, -0.58), (59.52, 19.84, -26.04), (62.0, 18.85, -6.2),
    (65.72, 20.83, -28.52),
    # the vent, the calendar, the ceiling lights
    (0, 71.67, -7.44), (12.4, 39.68, -86.3), (12.4, 45.14, -86.06),
    (0, 70.68, 57.04), (0, 71.55, 57.04), (0, 70.68, 0.0), (0, 71.55, 0.0), (0, 70.68, -57.04), (0, 71.55, -57.04),
]
RETEXTURE = [  # kept, textured like the model
    ((0, -1.86, 0), lambda m: m.update(Texture='Game/recursion/astra/ward_floor_tiles.jpg', diffuse=[.62, .64, .62],
                                       roughness=.58, specularStrength=.28, tileSize=24) or m),   # floor: 12 unit tiles
    ((0, 73.78, 0), lambda m: look(m, 'OfficeCeiling001', 90, normal=0.35)),                         # ceiling
    ((0, 26.04, 89.28), lambda m: look(m, 'Metal027', 30, color=False, normal=0.6)),  # door leaf: powder coat
] + [(p, lambda m: look(m, 'PaintedPlaster017', 70, normal=0.3)) for p in (                       # walls
    (0.0, 35.96, -88.66), (-43.09, 35.96, 88.66), (0.0, 62.0, 88.66), (43.09, 35.96, 88.66), (76.26, 35.96, 0.0), (-76.26, 35.96, 0.0))
] + [(p, lambda m: look(m, 'Plastic018B', 40, color=False)) for p in (                             # the wipe-clean lower band
    (0.0, 11.16, -86.6), (-43.09, 11.16, 86.6), (43.09, 11.16, 86.6), (74.2, 11.16, 0.0), (-74.2, 11.16, 0.0))]


def at(entity, position):
    return all(abs(a - b) < 0.05 for a, b in zip(entity['Transform']['position'], position))


def find(entities, position, what):
    found = [e for e in entities if at(e, position)]
    assert len(found) == 1, '%s: %d entities at %s in 01_beginning.json' % (what, len(found), position)
    return found[0]


def centre_of(obj):
    corners = [obj.matrix_world @ v.co for v in obj.data.vertices] if obj.type == 'MESH' else \
              [obj.matrix_world.translation]
    return [(min(c[i] for c in corners) + max(c[i] for c in corners)) / 2 for i in range(3)]


def group_of(centre):
    for name, (low, high) in GROUPS.items():
        if all(low[i] <= centre[i] <= high[i] for i in range(3)):
            return name
    return None


def bake_calendar():
    """the wall calendar (its backing, heading and every printed number, each a mesh of its own) drawn
    once, straight on, into Game/recursion/astra/calendar.png, and a flat page with that picture put
    in its place"""
    import math
    scene = bpy.context.scene
    parts = [o for o in scene.objects if o.name.startswith(('Calendar', 'Weekday'))]
    backing = bpy.data.objects['Calendar backing']
    corners = [backing.matrix_world @ v.co for v in backing.data.vertices]
    x0, x1 = min(c.x for c in corners), max(c.x for c in corners)
    z0, z1 = min(c.z for c in corners), max(c.z for c in corners)
    front = min(c.y for c in corners)           # the face towards the room
    for o in scene.objects:
        o.hide_render = o not in parts
    data = bpy.data.cameras.new('Calendar camera')
    data.type = 'ORTHO'
    data.ortho_scale = max(x1 - x0, z1 - z0)
    camera = bpy.data.objects.new('Calendar camera', data)
    scene.collection.objects.link(camera)
    camera.location = ((x0 + x1) / 2, front - 20.0, (z0 + z1) / 2)
    camera.rotation_euler = (math.radians(90), 0, 0)        # looking at the wall, up is up
    scene.camera = camera
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'FLAT'
    scene.display.shading.color_type = 'MATERIAL'
    scene.view_settings.view_transform = 'Standard'
    height = 1024
    scene.render.resolution_x = round(height * (x1 - x0) / (z1 - z0))
    scene.render.resolution_y = height
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = os.path.join(OUT, 'calendar.png')
    bpy.ops.render.render(write_still=True)
    for o in scene.objects:
        o.hide_render = False
    # the page: one flat face where the backing's front was, the picture across it
    mesh = bpy.data.meshes.new('Printed calendar')
    y = front
    mesh.from_pydata([(x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1)], [], [(0, 1, 2, 3)])
    uv = mesh.uv_layers.new(name='Game UV')
    for loop, coordinate in zip(mesh.loops, [(0, 0), (1, 0), (1, 1), (0, 1)]):
        uv.data[loop.index].uv = coordinate
    material = bpy.data.materials.new('Calendar print')
    material.diffuse_color = (1, 1, 1, 1)
    mesh.materials.append(material)
    page = bpy.data.objects.new('Printed calendar', mesh)
    scene.collection.objects.link(page)


def export_meshes():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(OUT, 'hospital_room.blend'))
    bake_calendar()
    # one material per entity: split multi-material meshes (in this session only; the file is not saved)
    for obj in list(bpy.context.scene.objects):
        if obj.type == 'MESH' and len(obj.data.materials) > 1:
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.separate(type='MATERIAL')
            bpy.ops.object.mode_set(mode='OBJECT')
    # the loader needs UVs on every mesh (tiled materials ignore them)
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or obj.get('preview_shell') or obj.data.uv_layers:
            continue
        layer = obj.data.uv_layers.new(name='Game UV')
        for face in obj.data.polygons:
            axis = max(range(3), key=lambda index: abs(face.normal[index]))
            for index in face.loop_indices:
                point = obj.data.vertices[obj.data.loops[index].vertex_index].co
                layer.data[index].uv = (point[(axis + 1) % 3] * .1, point[(axis + 2) % 3] * .1)

    batches = {}
    for obj in list(bpy.context.scene.objects):
        if obj.type not in {'MESH', 'FONT'} or obj.get('preview_shell', False) or not obj.data.materials:
            continue
        centre = centre_of(obj)
        if left_out(obj, centre):
            continue
        batches.setdefault((group_of(centre), obj.data.materials[0].name), []).append(obj)

    for old in os.listdir(OUT):        # this export's own files: none left over from an earlier one
        if old.startswith('room_') and old.endswith('.obj'):
            os.remove(os.path.join(OUT, old))
    entities = []
    for (group, name), objects in sorted(batches.items(), key=lambda kv: (kv[0][0] or '', kv[0][1])):
        bpy.ops.object.select_all(action='DESELECT')
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        stem = re.sub(r'[^a-z0-9_]+', '_', name.lower())
        filename = 'room_' + (group + '_' if group else '') + stem + '.obj'
        bpy.ops.wm.obj_export(filepath=os.path.join(OUT, filename), export_selected_objects=True,
                              forward_axis='NEGATIVE_Z', up_axis='Y', export_materials=False,
                              apply_modifiers=True, export_triangulated_mesh=True)
        source = bpy.data.materials[name]
        material = {'ambient': [0, 0, 0], 'diffuse': [round(c, 4) for c in source.diffuse_color[:3]],
                    'specular': [.25, .25, .25], 'shininess': 24, 'roughness': .76}
        if name in ('steel', 'R7_chrome'):
            material.update(shininess=50, roughness=.32)
        if name in ('Night cobalt', 'Night masonry', 'Night asphalt'):
            material['emissive'] = .55
        if name in ('Sodium light', 'Occupied rooms', 'Moonlight'):
            material['emissive'] = 1.1
        if name == 'R7_glow':
            material['emissive'] = 1.0
        if name == 'Calendar print':
            material.update(Texture='Game/recursion/astra/calendar.png', roughness=.88)
        if name == 'Ward records print':
            material.update(Texture='Game/recursion/astra/ward_records.png', roughness=.88)
        if name in MATERIAL_LOOK:
            material = MATERIAL_LOOK[name](material)
        entity = {'Transform': {'position': [0, 0, 0], 'rotation': [0, 0, 0], 'scale': [1, 1, 1]},
                  'Components': {'RenderComponent': {'Mesh': 'Game/recursion/astra/' + filename, 'Material': material}}}
        entities.append(entity)    # glass casts no shadow (the engine's rule for glass)
    return entities


def main():
    entities = export_meshes()
    with open(BASE) as f:
        scene = json.load(f)
    old = scene['Entities']
    for position in HIDE:
        find(old, position, 'replaced by the model')['Components'].pop('RenderComponent', None)
    for position, change in RETEXTURE:
        component = find(old, position, 'retextured')['Components']['RenderComponent']
        component['Material'] = change(component['Material'])
    scene['Entities'] = [e for e in old if e.get('Components')] + entities
    # collision for the model's solid furniture (the old boxes' bodies stay where they match)
    for position, size in [([-34, 8, 19], [13, 16, 12]), ([42, 9, -12], [13, 18, 12])]:
        scene['Entities'].append({'Transform': {'position': position, 'rotation': [0, 0, 0], 'scale': [1, 1, 1]},
                                  'Components': {'PhysicsComponent': {'BoundingBodyType': ['cube'], 'scale': size, 'mass': 0}}})
    scene['Environment'].update(filmGrain=.016, chromaticAberration=.18)
    with open(LEVEL, 'w') as f:
        json.dump(scene, f, indent=2)
        f.write('\n')
    print('Exported %d meshes into %s' % (len(entities), os.path.relpath(LEVEL, ROOT)))


main()
