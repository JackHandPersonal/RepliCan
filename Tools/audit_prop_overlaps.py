"""Which placed props pass through each other or through the building, from the level itself.

    python Tools/ue_remote.py --file Tools/audit_prop_overlaps.py

Walls, floors and trim overlap on purpose. A barrel through a crate is untidy; a crate through
a wall, a trim moulding or a flight of stairs, or a prop sunk into the floor, is a fault. The
only way to be sure there are none is to ask the level: every actor whose label marks it as a
placed item has its world box read back, and:
  * every pair of props that intersects by more than SLACK on all three axes is listed;
  * every prop that intersects a piece of ARCHITECTURE (any S10_ actor that is not a prop, a
    light or an effect; planes such as floors and walkway decks are compared by height instead)
    is listed as a FAULT -- this is the check that clutter against the building must pass;
  * anything whose bottom is below the floor it stands on is listed as SUNK;
  * anything standing under a walkway that is taller than the walkway is high is listed.
"""
import unreal, io, traceback, itertools

PREFIXES = ('S10_Clutter_', 'S10_Stair_', 'Bay', 'Foyer_Crate', 'Foyer_Barrel', 'Foyer_Cart', 'Foyer_Tank', 'Caf_')
SKIP_WORDS = ('Wall', 'Floor', 'Ceil', 'Trim', 'Pillar', 'Fixture', 'Light', 'Sign', 'Door', 'Board', 'Window', 'Glass', 'Fill', 'Rail', 'Lamp', 'Red', 'Fog', 'Steam', 'Dust', 'Mist', 'Walk', 'Grate', 'Trench', 'Sill', 'Pilaster', 'Lineup', 'Trooper')
# Architecture of the basement, for the prop-against-building check: anything S10_ that is not
# a prop, a light, an effect or the sign. Planes (floors, decks, grates) have no thickness and
# are checked by height, not by box.
ARCH_NOT = ('S10_Clutter_', 'S10_Fog', 'S10_Steam', 'S10_Dust', 'S10_Sign', 'S10_Lamp', 'S10_Red', 'S10_UnderWalk')
FLOOR_Z = -5000.0
SLACK = 2.0
WALK_Z, WALK_TOP = -5000.0 + 76.0, -5000.0 + 76.0
WALK_COLS = ((-125.0, 125.0), (875.0, 1125.0))
ROOM_Y = (178.0, 2678.0)

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before auditing')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    items = []
    for a in eas.get_all_level_actors():
        l = a.get_actor_label()
        if not l.startswith(PREFIXES) or any(w in l for w in SKIP_WORDS):
            continue
        try:
            o, e = a.get_actor_bounds(False)
        except Exception:
            continue
        items.append((l, (o.x - e.x, o.y - e.y, o.z - e.z, o.x + e.x, o.y + e.y, o.z + e.z)))
    print('%d placed items checked' % len(items))
    arch = []
    for a in eas.get_all_level_actors():
        l = a.get_actor_label()
        if not l.startswith('S10_') or l.startswith(ARCH_NOT) or '_Light' in l:
            continue
        try:
            o, e = a.get_actor_bounds(False)
        except Exception:
            continue
        if e.z < 0.5:
            continue   # a plane: the floor, a deck, a grate
        arch.append((l, (o.x - e.x, o.y - e.y, o.z - e.z, o.x + e.x, o.y + e.y, o.z + e.z)))
    faults = []
    for l, b in items:
        if not l.startswith('S10_Clutter_'):
            continue
        for la, a in arch:
            ox = min(a[3], b[3]) - max(a[0], b[0]); oy = min(a[4], b[4]) - max(a[1], b[1]); oz = min(a[5], b[5]) - max(a[2], b[2])
            if ox > 1.0 and oy > 1.0 and oz > 1.0:
                faults.append((ox * oy * oz, l, la, ox, oy, oz))
    faults.sort(reverse=True)
    print('%d FAULTS: props through the architecture (%d architecture pieces checked)' % (len(faults), len(arch)))
    for _v, l, la, ox, oy, oz in faults[:40]:
        print('   %-26s through %-26s  by %4.0f x %4.0f x %4.0f cm' % (l, la, ox, oy, oz))
    sunk = [(l, FLOOR_Z - b[2]) for l, b in items if l.startswith('S10_Clutter_') and b[2] < FLOOR_Z - 1.0]
    print('%d SUNK into the floor' % len(sunk))
    for l, d in sunk:
        print('   %-26s %.0f cm below the floor' % (l, d))
    bad = []
    for (la, a), (lb, b) in itertools.combinations(items, 2):
        ox = min(a[3], b[3]) - max(a[0], b[0]); oy = min(a[4], b[4]) - max(a[1], b[1]); oz = min(a[5], b[5]) - max(a[2], b[2])
        if ox > SLACK and oy > SLACK and oz > SLACK:
            bad.append((ox * oy * oz, la, lb, ox, oy, oz))
    bad.sort(reverse=True)
    print('%d overlapping pairs' % len(bad))
    for _v, la, lb, ox, oy, oz in bad[:30]:
        print('   %-26s x %-26s  overlap %4.0f x %4.0f x %4.0f cm' % (la, lb, ox, oy, oz))
    tall = []
    for l, b in items:
        if not l.startswith('S10_Clutter_'):
            continue
        cx = (b[0] + b[3]) * 0.5
        if any(x0 <= cx <= x1 for x0, x1 in WALK_COLS) and ROOM_Y[0] <= (b[1] + b[4]) * 0.5 <= ROOM_Y[1] and b[5] > WALK_TOP - 1.0:
            tall.append((l, b[5] - (-5000.0)))
    print('%d items under a walkway stand higher than it (76 cm)' % len(tall))
    for l, h in tall:
        print('   %-26s %.0f cm' % (l, h))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
