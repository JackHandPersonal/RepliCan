"""How Synty assembles a lift, taken from the pack's own demo rather than guessed.

    python Tools/ue_remote.py --file Tools/measure_lift_assembly.py

The SciFi Space kit ships seven lift meshes and no instructions. The demo map is the
instructions: it has one built, so the relative transforms of every piece can be read straight
off it. Loads Demonstration_Interior, finds the lift cluster, prints each piece relative to the
car, and puts the project's level back.

What comes out is the recipe: which piece sits where, which way round, and -- because the demo
stacks two levels -- what the pack considers a storey to be.
"""
import unreal, io, traceback, collections

MAP = '/Game/PolygonSciFiSpace/Maps/Demonstration_Interior'
HOME = '/Game/RepliCan/Maps/Lvl_AsteroidFacility'
KEY = 'Lift'

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    les.load_level(MAP)
    pieces = []
    for a in eas.get_all_level_actors():
        try:
            c = a.static_mesh_component
            m = c.get_editor_property('static_mesh')
        except Exception:
            continue
        if not m or KEY not in m.get_name():
            continue
        pieces.append((m.get_name(), a.get_actor_location(), a.get_actor_rotation(), a.get_actor_scale3d()))

    if not pieces:
        raise RuntimeError('no lift pieces found in ' + MAP)

    # The car is the anchor everything else is described against.
    cars = [p for p in pieces if p[0] == 'SM_Bld_Lift_01']
    anchor = cars[0] if cars else pieces[0]
    ax, ay, az = anchor[1].x, anchor[1].y, anchor[1].z
    ayaw = anchor[2].yaw
    print('anchor: %s at (%.0f, %.0f, %.0f) yaw %.0f' % (anchor[0], ax, ay, az, ayaw))
    print('')
    print('%-32s %28s %8s %s' % ('piece', 'offset from the car', 'yaw', 'scale'))
    for name, loc, rot, scale in sorted(pieces, key=lambda p: (p[1].z, p[0])):
        print('%-32s (%7.1f, %7.1f, %7.1f) %8.1f  (%.2f, %.2f, %.2f)'
              % (name, loc.x - ax, loc.y - ay, loc.z - az, rot.yaw - ayaw, scale.x, scale.y, scale.z))

    levels = sorted(set(round(p[1].z) for p in pieces))
    print('')
    print('distinct heights used: %s' % levels)
    if len(levels) > 1:
        gaps = [levels[i + 1] - levels[i] for i in range(len(levels) - 1)]
        print('spacing between them: %s   <- the storey height the pack builds to' % gaps)

    counts = collections.Counter(p[0] for p in pieces)
    print('piece counts: %s' % dict(counts))

    # The car's own size decides how much room a shaft needs.
    car_mesh = unreal.load_asset('/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Lift_01')
    if car_mesh:
        b = car_mesh.get_bounds()
        print('car bounds: %.1f x %.1f x %.1f cm, origin offset (%.1f, %.1f, %.1f)'
              % (b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2, b.origin.x, b.origin.y, b.origin.z))
    for n in ('SM_Bld_Lift_Wall_01', 'SM_Bld_Lift_Door_01', 'SM_Bld_Lift_Wall_Door_01'):
        m = unreal.load_asset('/Game/PolygonSciFiSpace/Meshes/Buildings/' + n)
        if m:
            b = m.get_bounds()
            print('%-26s %6.1f x %6.1f x %6.1f cm, origin (%.1f, %.1f, %.1f)'
                  % (n, b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2, b.origin.x, b.origin.y, b.origin.z))

    les.load_level(HOME)
    print('')
    print('level restored')
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
