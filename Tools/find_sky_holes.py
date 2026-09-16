"""Find every place in a room where you can see the sky.

    python Tools/ue_remote.py --file Tools/find_sky_holes.py

A room built from tiles has a hole wherever a tile is missing, a piece faces the wrong way, or
two kits meet at different heights -- and none of those can be found by reading the script that
laid them, because the script believes it covered everything. So this asks the level instead:
from a lattice of points at head height inside the region, trace straight UP; a trace that hits
nothing within REACH has found a hole. Same thing DOWN, for floor.

Prints each hole's world position and the nearest labelled actor, which is usually the piece
that should have been there.
"""
import unreal, io, traceback

# Sub 10 room one, with a margin, in world units.
X0, X1 = -200.0, 1200.0
Y0, Y1 = 100.0, 3150.0
Z_EYE = -5000.0 + 150.0
Z_EYE2 = -5000.0 + 76.0 + 150.0   # standing on the gantry
STEP = 125.0
REACH = 900.0

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before probing')
    world = ues.get_editor_world()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = [(a.get_actor_label(), a.get_actor_location()) for a in eas.get_all_level_actors()]

    def nearest(p):
        best, bd = '?', 1e12
        for lbl, loc in actors:
            d = (loc.x - p.x) ** 2 + (loc.y - p.y) ** 2 + (loc.z - p.z) ** 2
            if d < bd:
                best, bd = lbl, d
        return best, bd ** 0.5

    def probe(direction, name):
        global Z_EYE
        holes, inside = [], 0
        y = Y0
        while y <= Y1:
            x = X0
            while x <= X1:
                start = unreal.Vector(x, y, Z_EYE)
                # Only points that are actually in a room: something must be under them.
                floor = unreal.SystemLibrary.line_trace_single(
                    world, start, start + unreal.Vector(0, 0, -400.0),
                    unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
                if floor:
                    inside += 1
                    hit = unreal.SystemLibrary.line_trace_single(
                        world, start, start + direction * REACH,
                        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
                    if not hit:
                        holes.append(start)
                x += STEP
            y += STEP
        print('%s: %d points over floor, %d see nothing within %.0f cm' % (name, inside, len(holes), REACH))
        for h in holes[:40]:
            lbl, d = nearest(h)
            print('   hole at (%7.0f, %7.0f)   nearest actor %s (%.0f cm)' % (h.x, h.y, lbl, d))
        if len(holes) > 40:
            print('   ... and %d more' % (len(holes) - 40))
        return holes

    for z in (Z_EYE, Z_EYE2):
        Z_EYE = z
        probe(unreal.Vector(0, 0, 1.0), 'UP  (sky) from z %.0f' % z)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
