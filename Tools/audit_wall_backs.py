"""Every wall whose black back is showing, found by asking the level rather than the script.

    python Tools/ue_remote.py --file Tools/audit_wall_backs.py

Synty architecture is one-sided: a wall piece has a textured FRONT and a matte black BACK, and
the kit expects walls to sit back to back so that both sides of every partition are dressed.
The Space kit's convention is measured and written at the top of Tools/facility_layout.py -- a
piece spans local Y 0..89 with the ROOM on its +Y side -- so its back is the -Y face.

A back is a problem when it is visible from somewhere the player can stand. That is a question
about the built level, not about the script that built it (the script believes it covered
everything), so it is asked of the level: for every wall piece, points are sampled a little
way BEHIND its back face along its whole length, and each one is tested for

    over a floor      a trace straight down hits something within a storey
    uncovered         a short trace from the back face outward, along -Y, hits nothing --
                      no wall standing back to back with it, no solid it is buried in

A sample that is over a floor and uncovered is a black face in a room. The report groups them
by actor, and because it runs along the piece's whole length it also catches the other classic
fault -- a wall that OVERSHOOTS its corner and pokes, uncapped, into the next room -- as a run
of exposed samples at one end.

ASSUMPTION, stated: the horror / generic Base kit is taken to use the same +Y-front convention.
Its walls are 22.5 thick and centred, so if that is wrong the report will say so loudly by
flagging every one of them; check the S10_ rows before trusting the rest.
"""
import unreal, io, math, traceback, collections

BEHIND_CM = 18.0        # how far behind the back face to sample
COVER_CM = 70.0         # how far a back-to-back partner may be and still count as cover
EYE_CM = 120.0          # sample height above the piece's own base
STEP_CM = 60.0          # spacing along the length
FLOOR_CM = 320.0        # a floor must be within this far below to count as a room
MAX_ROWS = 60

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before auditing')
    world = ues.get_editor_world()
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    Q = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1

    def trace(a, b, ignore):
        return unreal.SystemLibrary.line_trace_single(world, a, b, Q, False, ignore,
                                                      unreal.DrawDebugTrace.NONE, True)

    walls = []
    for a in eas.get_all_level_actors():
        try:
            m = a.static_mesh_component.get_editor_property('static_mesh')
        except Exception:
            continue
        if not m or 'Wall' not in m.get_name() or 'Trim' in m.get_name() or 'Glass' in m.get_name():
            continue
        walls.append((a, m))

    findings = []
    per_kit = collections.Counter()
    for a, m in walls:
        xf = a.get_actor_transform()
        b = m.get_bounds()
        sc = a.get_actor_scale3d()
        lx0 = (b.origin.x - b.box_extent.x) * sc.x
        lx1 = (b.origin.x + b.box_extent.x) * sc.x
        # The back face: the piece's minimum local Y (0 on the Space kit, -11.2 on Base).
        ly_back = (b.origin.y - b.box_extent.y) * sc.y
        exposed = []
        n = 0
        x = lx0 + 10.0
        while x <= lx1 - 10.0:
            n += 1
            p = xf.transform_location(unreal.Vector(x, ly_back - BEHIND_CM, EYE_CM))
            back_dir = xf.transform_direction(unreal.Vector(0.0, -1.0, 0.0))
            over_floor = trace(p, p + unreal.Vector(0, 0, -FLOOR_CM), [a])
            covered = trace(p, p + back_dir * COVER_CM, [a])
            if over_floor and not covered:
                exposed.append((x, p))
            x += STEP_CM
        if exposed:
            kit = 'Base' if 'Base_' in m.get_name() else 'Space'
            per_kit[kit] += 1
            # Where along the piece: whole length, or one end (an overshoot).
            xs = [e[0] for e in exposed]
            where = 'ALL' if len(exposed) >= max(1, n - 1) else (
                'start %.0f..%.0f' % (min(xs), max(xs)) if max(xs) < lx0 + (lx1 - lx0) * 0.5 else
                'end %.0f..%.0f' % (min(xs), max(xs)) if min(xs) > lx0 + (lx1 - lx0) * 0.5 else
                'mid %.0f..%.0f' % (min(xs), max(xs)))
            p0 = exposed[0][1]
            findings.append((len(exposed), n, a.get_actor_label(), m.get_name(), where, p0))

    findings.sort(key=lambda f: (-f[0] / max(1, f[1]), f[2]))
    print('%d wall pieces checked, %d with an exposed back (%s)'
          % (len(walls), len(findings), ', '.join('%s %d' % kv for kv in per_kit.items()) or 'none'))
    print('')
    print('%-5s %-26s %-32s %-18s %s' % ('bad', 'actor', 'mesh', 'where along it', 'first exposed point'))
    for k, n, lbl, mesh, where, p in findings[:MAX_ROWS]:
        print('%2d/%-2d %-26s %-32s %-18s (%7.0f, %7.0f, %6.0f)' % (k, n, lbl, mesh, where, p.x, p.y, p.z))
    if len(findings) > MAX_ROWS:
        print('... and %d more' % (len(findings) - MAX_ROWS))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
