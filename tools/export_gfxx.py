"""Exports models from Blender files into the engine (res/models/*.obj).

Runs inside Blender:
  blender -b --disable-autoexec FILE.blend --python tools/export_gfxx.py -- OUT.obj [options]

options:
  --objects A,B,...  the meshes to export (default: every visible mesh)
  --faces N          decimate to about N faces in total (default: keep all)
  --subsurf N        cap subdivision-surface modifiers at N levels (default 1)
  --height H         scale so the model is H units tall (default: as modelled)
  --keep-origin      keep the modelled origin (default: centred in x/z, base at y = 0)
  --material NAME    only the faces with this material (e.g. a character's mask apart
                     from its body, so each gets its own engine material)
  --frame N          pose armatures at this frame first (default: the file's)

The meshes are evaluated with their modifiers (mirror, boolean, subsurf...),
joined into one, converted to Y-up (Blender is Z-up) and written without
materials: the map JSON gives each entity its material.

Used for the gfxx assets (map 3, tools/make_lot.py):
  gun.blend          -> res/models/pistol.obj
  puzzle.blend       -> res/models/tripod_light.obj
  try.blend          -> res/models/masked_man_mask.obj, masked_man_body.obj
                        (--material mask / body --keep-origin --frame 60: the two
                        parts of the rigged masked man, posed, aligned)
"""
import bpy, bmesh, sys, os
from mathutils import Vector


def arguments():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    if not argv:
        sys.exit('usage: blender -b FILE.blend --python export_gfxx.py -- OUT.obj [options]')
    opts = {'out': argv[0], 'objects': None, 'faces': 0, 'subsurf': 1, 'height': 0.0, 'keep_origin': False,
            'material': None, 'frame': None}
    i = 1
    while i < len(argv):
        a = argv[i]
        if a == '--objects':
            opts['objects'] = argv[i + 1].split(','); i += 1
        elif a == '--faces':
            opts['faces'] = int(argv[i + 1]); i += 1
        elif a == '--subsurf':
            opts['subsurf'] = int(argv[i + 1]); i += 1
        elif a == '--height':
            opts['height'] = float(argv[i + 1]); i += 1
        elif a == '--keep-origin':
            opts['keep_origin'] = True
        elif a == '--material':
            opts['material'] = argv[i + 1]; i += 1
        elif a == '--frame':
            opts['frame'] = int(argv[i + 1]); i += 1
        i += 1
    return opts


def main():
    opts = arguments()
    scene = bpy.context.scene
    if opts['objects']:
        sources = [bpy.data.objects[n] for n in opts['objects']]
    else:
        sources = [o for o in scene.objects if o.type == 'MESH' and o.visible_get()]
    if not sources:
        sys.exit('nothing to export')

    if opts['frame'] is not None:
        scene.frame_set(opts['frame'])
    for o in sources:
        for m in o.modifiers:
            if m.type == 'SUBSURF':
                m.levels = min(m.levels, opts['subsurf'])
                m.render_levels = min(m.render_levels, opts['subsurf'])

    # every source evaluated (modifiers applied) into one world-space mesh
    depsgraph = bpy.context.evaluated_depsgraph_get()
    combined = bmesh.new()
    for o in sources:
        evaluated = o.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        mesh.transform(o.matrix_world)
        if opts['material']:
            part = bmesh.new()
            part.from_mesh(mesh)
            names = [s.material.name if s.material else '' for s in o.material_slots]
            bmesh.ops.delete(part, geom=[f for f in part.faces
                                         if f.material_index >= len(names) or names[f.material_index] != opts['material']],
                             context='FACES')
            part.to_mesh(mesh)
            part.free()
        combined.from_mesh(mesh)
        evaluated.to_mesh_clear()
    data = bpy.data.meshes.new('export')
    combined.to_mesh(data)
    combined.free()
    out = bpy.data.objects.new('export', data)
    scene.collection.objects.link(out)

    faces = len(data.polygons)
    if opts['faces'] and faces > opts['faces']:
        decimate = out.modifiers.new('decimate', 'DECIMATE')
        decimate.ratio = opts['faces'] / faces
        bpy.context.view_layer.objects.active = out
        out.select_set(True)
        bpy.ops.object.modifier_apply(modifier='decimate')

    # Blender Z-up -> engine Y-up: (x, y, z) -> (x, z, -y)
    for v in data.vertices:
        x, y, z = v.co
        v.co = Vector((x, z, -y))
    if not opts['keep_origin']:
        lo = Vector([min(v.co[i] for v in data.vertices) for i in range(3)])
        hi = Vector([max(v.co[i] for v in data.vertices) for i in range(3)])
        shift = Vector(((lo.x + hi.x) / 2, lo.y, (lo.z + hi.z) / 2))
        scale = opts['height'] / (hi.y - lo.y) if opts['height'] > 0 else 1.0
        for v in data.vertices:
            v.co = (v.co - shift) * scale
    data.update()

    for o in scene.objects:
        o.select_set(o == out)
    bpy.context.view_layer.objects.active = out
    # the vertices are already Y-up: write them as they are
    bpy.ops.wm.obj_export(filepath=os.path.abspath(opts['out']), export_selected_objects=True,
                          export_materials=False, forward_axis='Y', up_axis='Z',
                          export_uv=True, export_normals=True, apply_modifiers=False)
    lo = [min(v.co[i] for v in data.vertices) for i in range(3)]
    hi = [max(v.co[i] for v in data.vertices) for i in range(3)]
    print('EXPORTED %s: %d faces, size %.2f x %.2f x %.2f' % (opts['out'], len(data.polygons),
          hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]))


main()
