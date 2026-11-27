"""Level 1's way out, as data: the keypad by the ward door takes the code, the door opens, and walking
out through it goes on to the corridor (Game/recursion/02_corridor_subject4.json, in by its
"from_ward" entry). The engine knows none of this; it is what the map's JSON says (Core/Interaction,
Components/PressableComponent, Components/DrivenComponent, the door's timeline
Game/sequences/01_ward_door.json).

    python3 tools/ward_interaction.py     puts it into Game/recursion/01_beginning_astra.json as it is

tools/export_astra_room.py puts it in on every export (add(scene)).
"""
import json, os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LEVEL = os.path.join(ROOT, 'Game/recursion/01_beginning_astra.json')

CODE = '0314'
LOCK = 'ward_keypad'
DOOR_TIMELINE = 'Game/sequences/01_ward_door.json'
# the door leaf: 23.56 wide (x -11.78..11.78), its room side at z 88.54, the handle on the +x side, so
# hung on the -x side; it swings into the room (a turn of +90 round y takes its free edge towards -z)
HINGE = [-11.78, 0.0, 88.54]
# what swings with it: the old level's leaf and the dark backing behind its window (by where they are),
# and astra's fittings on the leaf (by mesh)
DOOR_AT = [(0, 26.04, 89.28), (0, 39.68, 89.28)]
DOOR_MESHES = re.compile(r'Game/recursion/astra/room_door_.*\.obj$')
KEY_MESH = re.compile(r'Game/recursion/astra/room_keypad_print_keypad_key_(\w+)\.obj$')
KEYPAD_MIDDLE = [18.6, 32.6, 84.7]

# outside the door: the ward's corridor (as 02_corridor_subject4's: 2.4 m wide, its walls, its lower
# band, its floor and ceiling), unlit (the night; the room's light falls into it through the door).
# Beyond the room's door wall (its outer face at z 90.52), running along x
WALL_OUT, CORRIDOR_W, CORRIDOR_HALF = 90.52, 59.52, 150.0


def box(position, scale, diffuse, solid=True, cast_shadow=True, **material):
    look = {'ambient': [0, 0, 0], 'diffuse': diffuse, 'specular': [0.2, 0.2, 0.2], 'shininess': 20.0, 'roughness': 0.85}
    look.update(material)
    render = {'Mesh': 'res/models/cube4.obj', 'Material': look}
    if not cast_shadow:
        render['castShadow'] = False
    components = {'RenderComponent': render}
    if solid:
        components['PhysicsComponent'] = {'BoundingBodyType': ['cube'], 'scale': scale, 'mass': 0}
    return {'Transform': {'position': position, 'rotation': [0, 0, 0], 'scale': scale}, 'Components': components}


def corridor_outside():
    z = WALL_OUT + CORRIDOR_W / 2
    far = WALL_OUT + CORRIDOR_W + 1.86
    length = 2 * CORRIDOR_HALF
    wall, band = [0.42, 0.5, 0.44], [0.22, 0.27, 0.24]
    return [
        box([0, -1.86, z], [length, 3.72, CORRIDOR_W], [0.3, 0.3, 0.29], shininess=80, roughness=0.22,
            specularStrength=0.7),                                                         # floor
        box([0, 73.78, z], [length, 3.72, CORRIDOR_W], [0.8, 0.8, 0.76], roughness=0.8),    # ceiling
        box([0, 35.96, far], [length, 71.92, 3.72], wall),                                 # the far wall
        box([0, 11.16, far - 2.06], [length, 22.32, 0.4], band, solid=False, cast_shadow=False, roughness=0.6),
        box([-CORRIDOR_HALF - 1.86, 35.96, z], [3.72, 71.92, CORRIDOR_W], wall),           # its ends
        box([CORRIDOR_HALF + 1.86, 35.96, z], [3.72, 71.92, CORRIDOR_W], wall),
        # the lower band on the room's side too (its door wall seen from the corridor)
        box([-43.09, 11.16, WALL_OUT + 0.2], [62.62, 22.32, 0.4], band, solid=False, cast_shadow=False, roughness=0.6),
        box([43.09, 11.16, WALL_OUT + 0.2], [62.62, 22.32, 0.4], band, solid=False, cast_shadow=False, roughness=0.6),
    ]


def at(entity, position):
    return all(abs(a - b) < 0.05 for a, b in zip(entity['Transform']['position'], position))


def add(scene):
    keys, swinging = 0, 0
    for entity in scene['Entities']:
        components = entity.get('Components', {})
        mesh = components.get('RenderComponent', {}).get('Mesh', '')
        key = KEY_MESH.match(mesh)
        if key:
            # a key: pressed in 4 mm (towards the wall, +z), its beep, its digit into the lock
            symbol = {'star': '*', 'hash': '#'}.get(key.group(1), key.group(1))
            components['Pressable'] = {'reach': 32, 'travel': [0, 0, 0.1], 'pressTime': 0.16,
                                       'on': [{'sound': 'keypad_beep', 'volume': 0.55, 'radius': 250},
                                              {'lock': LOCK, 'key': symbol}]}
            keys += 1
        if DOOR_MESHES.match(mesh) or any(at(entity, p) for p in DOOR_AT):
            components['Driven'] = {'param': 'ward_door', 'pivot': HINGE, 'rotate': [0, 90, 0],
                                    'anchor': 'ward_door'}
            swinging += 1
    assert keys == 12, 'the keypad has %d keys in the level, not 12' % keys
    assert swinging == 5, '%d parts of the door found, not 5 (leaf, window backing, 3 fittings)' % swinging

    scene['Interaction'] = {
        'locks': [{'name': LOCK, 'code': CODE, 'clear': '*', 'enter': '#',
                   'onSolved': [{'sound': 'keypad_accept', 'volume': 0.6, 'at': KEYPAD_MIDDLE, 'radius': 250},
                                {'event': 'ward_door_unlocked'}],
                   'onWrong': [{'sound': 'keypad_deny', 'volume': 0.6, 'at': KEYPAD_MIDDLE, 'radius': 250}]}],
        # through the open door: out of the room (the eye past the wall's inner face)
        'triggers': [{'box': [[-11.78, 0, 87.2], [11.78, 60, 120]], 'after': 'ward_door_unlocked', 'once': True,
                      'on': [{'loadLevel': 'Game/recursion/02_corridor_subject4.json', 'entry': 'from_ward',
                              'fade': 0.9}]}],
    }
    outside = corridor_outside()
    scene['Entities'] = [e for e in scene['Entities'] if e not in outside] + outside
    sequences = scene.setdefault('Sequences', [])
    if {'file': DOOR_TIMELINE} not in sequences:
        sequences.append({'file': DOOR_TIMELINE})
    return scene


if __name__ == '__main__':
    with open(LEVEL) as f:
        scene = json.load(f)
    add(scene)
    with open(LEVEL, 'w') as f:
        json.dump(scene, f, indent=2)
        f.write('\n')
    print('The ward door and its keypad in %s' % os.path.relpath(LEVEL, ROOT))
