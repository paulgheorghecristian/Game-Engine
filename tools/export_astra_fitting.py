"""Exports astra's ceiling fitting (Game/recursion/astra/hospital_room.blend: the middle one of the
ward's three) as a model of its own, for any level to hang from its ceilings:

    blender -b --python tools/export_astra_fitting.py

- Game/recursion/astra/fitting_housing.obj   the housing (open below), along z, 34 units long
- Game/recursion/astra/fitting_tubes.obj     its two frosted tubes (glowing)

In engine coordinates, centred on x = z = 0, at the height they hang at in the ward (top 71.1, under
a ceiling at 71.92): an entity puts one at (x, 0, z), turned 90 degrees round y to run along x.
"""
import bpy, os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'Game/recursion/astra')


def centre(obj):
    corners = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return [(min(c[i] for c in corners) + max(c[i] for c in corners)) / 2 for i in range(3)]


def export(prefix, filename):
    bpy.ops.object.select_all(action='DESELECT')
    chosen = [o for o in bpy.context.scene.objects if o.type == 'MESH' and o.name.startswith(prefix)
              and abs(centre(o)[0]) < 4 and abs(centre(o)[1]) < 4]       # (Blender y: the middle one)
    assert chosen, 'no %s in the middle of the ward' % prefix
    for o in chosen:
        o.select_set(True)
    bpy.context.view_layer.objects.active = chosen[0]
    bpy.ops.wm.obj_export(filepath=os.path.join(OUT, filename), export_selected_objects=True,
                          forward_axis='NEGATIVE_Z', up_axis='Y', export_materials=False,
                          apply_modifiers=True, export_triangulated_mesh=True)
    print('%s: %s' % (filename, ', '.join(o.name for o in chosen)))


bpy.ops.wm.open_mainfile(filepath=os.path.join(OUT, 'hospital_room.blend'))
export('Fluorescent fixture', 'fitting_housing.obj')
export('Frosted fluorescent tube', 'fitting_tubes.obj')
