"""What the SciFi Horror kit is made of, and how its demo uses the bits we want.

    python Tools/ue_remote.py --file Tools/horror_kit_survey.py

The transplanted cryo deck answered "what does a room from this pack look like" and the answer
was "twisty". To BUILD a simple one instead we need the vocabulary, not a chunk: how big each
piece is, what the pack's storey height is, and -- the part the first capture missed entirely --
where the demo puts its FX. Tools/horror_capture.py only ever read static meshes and lights, so
the steam, sparks and ground fog that make the pack's own screenshots look the way they do were
never coming across.

Prints:
    sizes     the pieces a room is built from, measured
    fx        every Niagara actor in the demo: system, where, how big
    fx use    which systems appear most, and at what heights
"""
import unreal, io, collections, traceback

MAP = '/Game/Synty/PolygonSciFiHorror/Maps/Demo'
HOME = '/Game/RepliCan/Maps/Lvl_AsteroidFacility'

G = '/Game/Synty/PolygonGeneric/Meshes/Base/'
H = '/Game/Synty/PolygonSciFiHorror/Meshes/Buildings/'
P = '/Game/Synty/PolygonSciFiHorror/Meshes/Props/'

SIZES = [
    # the shell
    (G, 'SM_Bld_Base_Floor_01'), (G, 'SM_Bld_Base_Floor_Combined_01'),
    (G, 'SM_Bld_Base_Floor_Half_01'), (G, 'SM_Bld_Base_Floor_Hole_01'),
    (G, 'SM_Bld_Base_Ceiling_01'), (G, 'SM_Bld_Base_Wall_01'),
    (G, 'SM_Bld_Base_Wall_Door_01'), (G, 'SM_Bld_Base_Wall_Window_01'),
    (G, 'SM_Bld_Base_Pillar_01'), (G, 'SM_Bld_Base_Stairs_01'), (G, 'SM_Bld_Base_Stairs_02'),
    (G, 'SM_Bld_Base_Wall_Trim_01'),
    # the horror dressing
    (H, 'SM_Bld_Stairs_01'), (H, 'SM_Bld_Railing_01'), (H, 'SM_Bld_Railing_Half_01'),
    (H, 'SM_Bld_Railing_Stairs_01'), (H, 'SM_Bld_Wall_Trim_02'), (H, 'SM_Bld_Pillar_01'),
    (P, 'SM_Prop_Pipe_Straight_Full_01'), (P, 'SM_Prop_Pipe_Straight_Full_02'),
    (P, 'SM_Prop_Pipe_Straight_Full_03'), (P, 'SM_Prop_Pipe_Bend_04'),
    (P, 'SM_Prop_Pipe_Large_Bundle_01'), (P, 'SM_Prop_Pipe_Strut_03'),
    (P, 'SM_Prop_Light_Grid_01'), (P, 'SM_Prop_Light_Grid_04'), (P, 'SM_Prop_Light_04'),
    (P, 'SM_Prop_Extractor_Fan_01'), (P, 'SM_Prop_Extractor_Fan_01_Blades_01'),
    (P, 'SM_Prop_Cable_04'), (P, 'SM_Prop_Vent_02'),
]

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before surveying')
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    print('=== sizes (local bounds) ===')
    for folder, name in SIZES:
        m = unreal.load_asset(folder + name)
        if not m:
            print('%-42s MISSING' % name)
            continue
        b = m.get_bounds()
        o, e = b.origin, b.box_extent
        print('%-42s %7.1f x %7.1f x %7.1f   x %7.1f..%-7.1f y %7.1f..%-7.1f z %7.1f..%-7.1f'
              % (name, e.x * 2, e.y * 2, e.z * 2,
                 o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z))

    les.load_level(MAP)
    print('')
    print('=== FX in the demo ===')
    rows = []
    for a in eas.get_all_level_actors():
        comp = None
        for attr in ('niagara_component',):
            try:
                comp = a.get_editor_property(attr)
            except Exception:
                comp = None
            if comp:
                break
        if not comp:
            continue
        try:
            sysasset = comp.get_editor_property('asset')
        except Exception:
            sysasset = None
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        sc = a.get_actor_scale3d()
        rows.append((sysasset.get_name() if sysasset else '?', loc, rot, sc))

    print('%d Niagara actors' % len(rows))
    by_sys = collections.Counter(r[0] for r in rows)
    print('')
    print('%-36s %5s  %s' % ('system', 'count', 'heights it is used at'))
    for name, n in by_sys.most_common():
        hs = sorted({round(r[1].z / 25) * 25 for r in rows if r[0] == name})
        scales = sorted({round(r[3].x, 2) for r in rows if r[0] == name})
        print('%-36s %5d  z %s   scale %s' % (name, n, hs[:10], scales[:6]))

    print('')
    print('=== a sample of each, with pitch (0 = blowing along +X, 90 = straight up) ===')
    seen = set()
    for name, loc, rot, sc in rows:
        if name in seen:
            continue
        seen.add(name)
        print('%-36s at (%8.0f, %8.0f, %7.0f)  pitch %6.1f yaw %6.1f  scale %.2f'
              % (name, loc.x, loc.y, loc.z, rot.pitch, rot.yaw, sc.x))

    les.load_level(HOME)
    print('')
    print('level restored')
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
