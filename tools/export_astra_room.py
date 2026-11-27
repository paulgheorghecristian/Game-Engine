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
import bpy, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ward_interaction     # the keypad's code, the door swinging open, the way out (tools/ward_interaction.py)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'Game/recursion/astra')
BASE = os.path.join(ROOT, 'Game/recursion/01_beginning.json')
LEVEL = os.path.join(ROOT, 'Game/recursion/01_beginning_astra.json')

# ---- textures: the CC0 sets and the ward's surfaces (tools/surfaces.py) -----------------------------------
from surfaces import SET_MEAN, look, ward_wall, ward_band, ward_ceiling, ward_floor, door_coat


# per Blender material: what it is made of
MATERIAL_LOOK = {
    'steel': lambda m: look(m, 'Metal009', 24, metallic=0.5),   # fully metal goes black: little to reflect
    'R7_chrome': lambda m: m.update(metallic=0.8, roughness=0.25) or m,
    # (single sheets, one-sided in the Blender file: both sides drawn, or the curtain vanished from behind)
    'blanket': lambda m: look(m, 'Fabric086', 10, disableCulling=True),
    'linen': lambda m: look(m, 'Fabric086', 8, disableCulling=True),
    'ivory': lambda m: look(m, 'Plastic018B', 30, color=False),
    'R7_bone': lambda m: look(m, 'Plastic018B', 30, color=False),
    'R7_white': lambda m: look(m, 'Plastic018B', 30, color=False),
    'R7_blue': lambda m: look(m, 'Plastic018B', 30, color=False),
    'rubber': lambda m: look(m, 'Rubber004', 12, color=False),
    'R7_wall': lambda m: look(m, 'PaintedPlaster017', 70, normal=0.3),
    'glass': lambda m: m.update(glass=True, roughness=0.05) or m,
    'screen': lambda m: m.update(roughness=0.08) or m,
}

# the ceiling lights: inside their fittings' housings, just above the tubes (the housing's underside then
# faces away from its own light: below it, 0.4 from its underside, the light lit a spot there and the
# fitting's emission left a black dot over it)
TUBES_Y = 70.3

# ---- what is kept apart (Blender coordinates, z up: the centre of each object's bounds) ---------------
GROUPS = {
    # fittings on the door leaf, not its frame (the frame's head is centred at 52.8, above the leaf's top)
    'door': ((-11.4, -90.0, 0.0), (11.4, -85.5, 52.0)),
    'keypad': ((16.0, -86.5, 29.0), (21.5, -84.0, 35.5)),
    # the ceiling fittings and their tubes: part of the lights (no shadow of their own: each light is
    # inside one, and sends its light out as the fitting does, its "emission")
    'fittings': ((-4.0, -80.0, 68.5), (4.0, 80.0, 72.5)),
}

# ---- left out of the level: the window (its wall pieces, frame, sill, blinds) and the street outside it;
# the old level's plain wall stands there instead
LEFT_OUT_NAMES = ('Window ', 'Partially raised blind', 'Blind ladder', 'Blind wand',
                  # drawn into the floor's texture instead (tools/make_ward_floor.py)
                  'Floor tile seam')
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
    ((0, -1.86, 0), ward_floor),                                                                     # floor
    ((0, 73.78, 0), ward_ceiling),                                                                   # ceiling
    ((0, 26.04, 89.28), door_coat),                                                  # door leaf: powder coat
] + [(p, ward_wall) for p in (                                                                     # walls
    (0.0, 35.96, -88.66), (-43.09, 35.96, 88.66), (0.0, 62.0, 88.66), (43.09, 35.96, 88.66), (76.26, 35.96, 0.0), (-76.26, 35.96, 0.0))
] + [(p, ward_band) for p in (                                                                     # the wipe-clean lower band
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


# ---- small print and thin lines drawn into pictures ---------------------------------------------------
# As geometry (every number a mesh, wires and joints as hair-thin strips) they are thinner than a pixel
# from across the room: broken into dashes, then gone. Each "carrier" below (a page, a plate, a face)
# is drawn straight on, with everything that sits on its face, into Game/recursion/astra/<file>.png;
# the carrier then wears that picture and the small parts are left out of the level.
#   carrier object, file, the way the camera looks at it, picture's right and up, pixels (long side),
#   roughness, parts to keep as geometry all the same, and more of its material (its relief: a real
#   paper's normal and roughness maps for the calendar)
PRINTS = [
    ('Calendar backing', 'calendar', (0, 1, 0), (1, 0, 0), (0, 0, 1), 1024, .88, (),
     {'NormalMapTexture': 'res/textures/Paper003_1K_Normal.jpg', 'normalMapStrength': 1.0,
      'RoughnessTexture': 'res/textures/Paper003_1K_Roughness.jpg'}),       # crumpled paper (ambientCG)
    # a real clock: the hour marks printed on the dial, the hands and the pin standing off it
    ('Clock face', 'clock_face', (0, 1, 0), (1, 0, 0), (0, 0, 1), 512, .5,
     ('Clock minute hand', 'Clock hour hand', 'Clock pin'), {}),
    ('Observation glass', 'door_window', (0, -1, 0), (-1, 0, 0), (0, 0, 1), 512, .08, (), {}),
] + [
    # the keypad's buttons stay raised keys: each one's digit printed on its own top
    ('Key ' + key, 'keypad_key_' + {'*': 'star', '#': 'hash'}.get(key, key), (0, -1, 0), (-1, 0, 0), (0, 0, 1),
     64, .45, (), {}) for key in '1234567890*#'
]
# (not the keyboard: flattened into a picture its keys lost their relief; its letters stay geometry)
PRINT_LOOK = {}     # material name -> (picture, roughness, more of its material), filled by bake_prints



def bake_prints():
    from mathutils import Matrix, Vector
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'FLAT'
    scene.display.shading.color_type = 'MATERIAL'
    # (raw: the material's own values, as the engine uses a texture's. Through the display transform
    # every dark colour came out pale: a baby-blue sticker for a sign, a pale square for dark glass)
    scene.view_settings.view_transform = 'Raw'
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'

    def corners(obj):
        if obj.type == 'MESH':
            return [obj.matrix_world @ v.co for v in obj.data.vertices]
        return [obj.matrix_world @ Vector(c) for c in obj.bound_box]

    for name, stem, forward, right, up, pixels, roughness, keep, extra in PRINTS:
        carrier = bpy.data.objects.get(name)
        assert carrier is not None, 'no object "%s" in the Blender file' % name
        f, r, u = Vector(forward), Vector(right), Vector(up)
        points = corners(carrier)
        r0, r1 = min(p.dot(r) for p in points), max(p.dot(r) for p in points)
        u0, u1 = min(p.dot(u) for p in points), max(p.dot(u) for p in points)
        front = max(p.dot(-f) for p in points)          # its face towards the camera
        # what sits on that face: inside its outline, up to a unit in front of it
        parts = []
        for o in scene.objects:
            if o is carrier or o.type not in {'MESH', 'FONT'} or o.get('preview_shell') or o.name.startswith(keep):
                continue
            ps = corners(o)
            centre = sum(ps, Vector()) / len(ps)
            if r0 <= centre.dot(r) <= r1 and u0 <= centre.dot(u) <= u1 and -0.1 <= centre.dot(-f) - front <= 1.0:
                parts.append(o)
        for o in scene.objects:
            o.hide_render = not (o is carrier or o in parts)
        data = bpy.data.cameras.new(stem)
        data.type = 'ORTHO'
        data.ortho_scale = max(r1 - r0, u1 - u0)
        camera = bpy.data.objects.new(stem + ' camera', data)
        scene.collection.objects.link(camera)
        back = -f
        middle = r * ((r0 + r1) / 2) + u * ((u0 + u1) / 2) + back * (front + 20.0)
        camera.matrix_world = Matrix(((r.x, u.x, back.x, middle.x), (r.y, u.y, back.y, middle.y),
                                      (r.z, u.z, back.z, middle.z), (0, 0, 0, 1)))
        scene.camera = camera
        wide = (r1 - r0) >= (u1 - u0)
        scene.render.resolution_x = pixels if wide else max(8, round(pixels * (r1 - r0) / (u1 - u0)))
        scene.render.resolution_y = max(8, round(pixels * (u1 - u0) / (r1 - r0))) if wide else pixels
        scene.render.filepath = os.path.join(OUT, stem + '.png')
        bpy.ops.render.render(write_still=True)
        for o in scene.objects:
            o.hide_render = False
        # the carrier wears the picture (laid on as the camera saw it); its small parts go
        for o in parts:
            bpy.data.objects.remove(o, do_unlink=True)
        mesh = carrier.data
        while mesh.uv_layers:
            mesh.uv_layers.remove(mesh.uv_layers[0])
        layer = mesh.uv_layers.new(name='Game UV')
        for loop in mesh.loops:
            p = carrier.matrix_world @ mesh.vertices[loop.vertex_index].co
            layer.data[loop.index].uv = ((p.dot(r) - r0) / (r1 - r0), (p.dot(u) - u0) / (u1 - u0))
        material = bpy.data.materials.new('Print ' + stem)
        material.diffuse_color = (1, 1, 1, 1)
        mesh.materials.clear()
        mesh.materials.append(material)
        PRINT_LOOK[material.name] = ('Game/recursion/astra/' + stem + '.png', roughness, extra)
        print('Print %s: %d parts drawn into %s.png' % (name, len(parts), stem))


# ---- standing on the floor (Blender z = 0): in the Blender file these stop short of it, the chair and
# the desk on legs 1 unit up (they looked as if they floated). Legs reach down to it; wheels and the
# computer under the desk are set down on it
FLOOR = 0.0
REACH_DOWN = ('Chair leg', 'Desk leg')
SET_DOWN = ('Caster', 'IV caster', 'Computer tower')


def ground():
    import re
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue
        base = re.sub(r'\.\d+$', '', obj.name)
        world = obj.matrix_world
        lowest = min((world @ v.co).z for v in obj.data.vertices)
        if lowest <= FLOOR + 1e-3:
            continue
        if base in REACH_DOWN:
            inverse = world.inverted()
            for v in obj.data.vertices:
                p = world @ v.co
                if p.z < lowest + 0.3:          # its foot: brought down to the floor
                    p.z = FLOOR
                    v.co = inverse @ p
        elif base in SET_DOWN:
            obj.location.z -= lowest - FLOOR


def export_meshes():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(OUT, 'hospital_room.blend'))
    ground()
    bake_prints()
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
        if name in PRINT_LOOK:
            material.update(Texture=PRINT_LOOK[name][0], roughness=PRINT_LOOK[name][1])
            material.update(PRINT_LOOK[name][2])
        if name == 'Ward records print':
            material.update(Texture='Game/recursion/astra/ward_records.png', roughness=.88)
        if name in MATERIAL_LOOK:
            material = MATERIAL_LOOK[name](material)
        entity = {'Transform': {'position': [0, 0, 0], 'rotation': [0, 0, 0], 'scale': [1, 1, 1]},
                  'Components': {'RenderComponent': {'Mesh': 'Game/recursion/astra/' + filename, 'Material': material}}}
        if group == 'fittings':
            entity['Components']['RenderComponent']['castShadow'] = False
            if name == 'R7_bone':
                material['emissive'] = 0.12     # the housing's reflector, lit by its own tubes
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
    # light bouncing between the walls, ceiling and floor (Rendering/BounceLight): a corner fills in as a real one does
    scene['Environment'].update(filmGrain=0.0, chromaticAberration=.18, bounceLight=1.0, bounceRadius=80.0,
                                bloomStrength=1.5, bloomThreshold=0.7)
    # what faces down (the ceiling, under the furniture) gets the light the lit floor sends up: the bounce
    # light cannot see the floor when the ceiling is looked at (it is off the screen)
    scene['Environment']['ambientGroundTint'] = [3.0, 3.1, 2.9]
    ward_interaction.add(scene)
    # the ceiling lights' shadows: 1024 pixels a cube face (512 drew stair steps on the walls)
    for light in scene.get('Lights', []):
        if light.get('Attributes', {}).get('cast_shadow'):
            light['Attributes']['shadowMapSize'] = 1024
        # their reach: the one by the clock (z -57) lights the room's far end, the other two only round them
        size = 260 if abs(light['Transform']['position'][2] + 57.04) < 0.1 else 100
        light['Transform']['scale'] = [size, size, size]
        # where the light comes from: the tubes, in the bottom of each fitting (y 69.6-69.9), and how the
        # fitting sends it out: down and to the sides, none up (its housing is above the tubes). As a point
        # light below the housing (the old level's 68.2), the housing threw a long sharp shadow across the
        # ceiling
        light['Transform']['position'][1] = TUBES_Y
        # (bare tubes hang below the housing: some light leaves them sideways and a little up, past its edge)
        light['Attributes']['emission'] = {'axis': [0, -1, 0], 'full': 75, 'none': 120}
        # its tubes (29 long, along z): lit from along them, their highlights streaks on the glossy floor
        light['Attributes']['tube'] = {'axis': [0, 0, 1], 'length': 29}
    with open(LEVEL, 'w') as f:
        json.dump(scene, f, indent=2)
        f.write('\n')
    print('Exported %d meshes into %s' % (len(entities), os.path.relpath(LEVEL, ROOT)))


main()
