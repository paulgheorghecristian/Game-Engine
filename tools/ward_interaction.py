"""Level 1's way out, as data: the keypad by the ward door takes the code, the door opens, and the
corridor beyond it (Game/recursion/02_corridor_subject4.json) streams in where it really is, its west
cell door at the ward's door; out in it, the door's closer swings it shut behind the player, and the
ward, out of reach now, is taken out of the world (the corridor's own door standing where the ward's
did). The engine knows none of this; it is what the map's JSON says (Core/Interaction,
Core/LevelStreaming.cpp, Components/PressableComponent, Components/DrivenComponent, the door's
timelines Game/sequences/01_ward_door.json and 01_ward_door_close.json).

    python3 tools/ward_interaction.py     puts it into Game/recursion/01_beginning_astra.json as it is

tools/export_astra_room.py puts it in on every export (add(scene)).
"""
import json, os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LEVEL = os.path.join(ROOT, 'Game/recursion/01_beginning_astra.json')

CODE = '0314'
LOCK = 'ward_keypad'
DOOR_TIMELINES = ['Game/sequences/01_ward_door.json', 'Game/sequences/01_ward_door_close.json']
CORRIDOR = 'Game/recursion/02_corridor_subject4.json'
# the corridor put where it is: its west cell door (x -31.74, z 198.4, in its own coordinates) on the ward
# door (z 89.28); turned so the corridor's +x (away from that cell) is the ward's +z (out of the room)
CORRIDOR_ATTACH = {'here': [0, 0, 89.28], 'there': [-31.74, 0, 198.4], 'turn': -90}
# the corridor's own door there (leaf, window, handle; not the plate above it, nor the wall over the
# doorway): built only once the ward is gone, where the ward's door stood
CORRIDOR_STAND_INS = [[[-33.6, -1, 186], [-29.9, 56, 211]]]
# the door leaf: 23.56 wide (x -11.78..11.78), its room side at z 88.54, the handle on the +x side, so
# hung on the -x side; it swings into the room (a turn of +90 round y takes its free edge towards -z)
HINGE = [-11.78, 0.0, 88.54]
# what swings with it: the old level's leaf and the dark backing behind its window (by where they are),
# and astra's fittings on the leaf (by mesh)
DOOR_AT = [(0, 26.04, 89.28), (0, 39.68, 89.28)]
DOOR_MESHES = re.compile(r'Game/recursion/astra/room_door_.*\.obj$')
KEY_MESH = re.compile(r'Game/recursion/astra/room_keypad_print_keypad_key_(\w+)\.obj$')
KEYPAD_MIDDLE = [18.6, 32.6, 84.7]

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
                                {'event': 'ward_door_unlocked'},
                                {'streamLevel': CORRIDOR, 'attach': CORRIDOR_ATTACH,
                                 'standIns': CORRIDOR_STAND_INS}],
                   'onWrong': [{'sound': 'keypad_deny', 'volume': 0.6, 'at': KEYPAD_MIDDLE, 'radius': 250}]}],
        # out in the corridor (past the doorway, beyond the corridor wall's face at z 91.3): the door's
        # closer swings it shut behind
        'triggers': [{'box': [[-30, 0, 96], [30, 70, 150]], 'after': 'ward_door_unlocked', 'once': True,
                      'on': [{'event': 'ward_door_closing'}]}],
        # shut: the ward cannot be reached any more
        'reactions': [{'event': 'ward_door_shut', 'on': [{'unloadLevel': 'self'}]}],
    }
    sequences = scene.setdefault('Sequences', [])
    for timeline in DOOR_TIMELINES:
        if {'file': timeline} not in sequences:
            sequences.append({'file': timeline})
    return scene


if __name__ == '__main__':
    with open(LEVEL) as f:
        scene = json.load(f)
    add(scene)
    with open(LEVEL, 'w') as f:
        json.dump(scene, f, indent=2)
        f.write('\n')
    print('The ward door and its keypad in %s' % os.path.relpath(LEVEL, ROOT))
