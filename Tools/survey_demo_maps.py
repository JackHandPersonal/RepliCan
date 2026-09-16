"""Read every Synty demo map and write down how the pack expects to be built with.

    python Tools/ue_remote.py --file Tools/survey_demo_maps.py

Each pack ships demo levels built by the people who made the kit. They are the only place the
pack's own conventions are written down: which wall goes with which floor, how a doorway is
actually assembled, what a ceiling is made of, where the lights go and how far off the fixture
they sit. Guessing at those from the mesh browser is how a room ends up looking almost right.

Writes one JSON per map to Tools/DemoSurvey/ and prints a digest. For every map:

    what is in it      counted by mesh family, so the kit's actual vocabulary is visible
    materials          which material each family is used with -- packs ship several palettes
    doorways           every door-ish mesh with what is standing within two metres of it,
                       which is what reveals the frame/trim/threshold recipe
    lighting           every light with its class, intensity, units, colour, radius, mobility
    light-to-mesh      the nearest mesh to each light and the OFFSET IN THAT MESH'S OWN FRAME,
                       because "the lamp sits 7 cm below the panel's underside, on its centre"
                       is reusable and "the lamp is at Z=412" is not

Loading a map replaces what is open in the editor, so this saves nothing and puts the project's
own level back at the end. Do not run it with unsaved work open.
"""
import unreal, io, json, os, math, collections, traceback

OUT = os.path.join(unreal.Paths.project_dir(), 'Tools', 'DemoSurvey')
HOME = '/Game/RepliCan/Maps/Lvl_AsteroidFacility'
MAPS = [
    '/Game/PolygonSciFiSpace/Maps/Demonstration_Interior',
    '/Game/PolygonSciFiSpace/Maps/Demonstration_Exterior',
    '/Game/PolygonSciFiSpace/Maps/Overview',
    '/Game/PolygonSciFiWorlds/Maps/Demo_Corporation',
    '/Game/PolygonSciFiWorlds/Maps/Demo_Explorer',
    '/Game/PolygonSciFiWorlds/Maps/Demo_Scavenger',
    '/Game/PolygonSciFiWorlds/Maps/Demo_BlackMarket',
    '/Game/PolygonSciFiWorlds/Maps/Overview',
    '/Game/PolygonCyberCity/Maps/Demo',
    '/Game/PolygonCyberCity/Maps/Demo_Interior',
    '/Game/PolygonCyberCity/Maps/Overview',
    '/Game/Synty/PolygonSciFiHorror/Maps/Demo',
    '/Game/Synty/PolygonSciFiHorror/Maps/Demo_Exterior',
]
DOORISH = ('Door', 'Lift', 'Gate', 'Hatch', 'Airlock')
NEAR_DOOR = 260.0      # cm: what counts as part of a doorway assembly
NEAR_LIGHT = 400.0     # cm: how far to look for the fixture a light belongs to


def family(name):
    """SM_Bld_Wall_01_Alt -> Bld_Wall. The kit's vocabulary, not its part numbers."""
    n = name
    for p in ('SM_', 'SKM_', 'SK_'):
        if n.startswith(p):
            n = n[len(p):]
    bits = n.split('_')
    keep = []
    for b in bits:
        if b.isdigit() or (len(b) == 3 and b[:2].isdigit()):
            break
        keep.append(b)
        if len(keep) >= 2:
            break
    return '_'.join(keep) if keep else n


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before surveying')
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    os.makedirs(OUT, exist_ok=True)

    for map_path in MAPS:
        if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
            print('MISSING MAP', map_path)
            continue
        les.load_level(map_path)
        actors = eas.get_all_level_actors()

        meshes = []          # (family, mesh name, material names, actor)
        lights = []
        fams = collections.Counter()
        mats = collections.defaultdict(collections.Counter)

        for a in actors:
            cn = a.get_class().get_name()
            if 'Light' in cn:
                lights.append(a)
                continue
            comp = None
            try:
                comp = a.static_mesh_component
            except Exception:
                try:
                    comp = a.skeletal_mesh_component
                except Exception:
                    comp = None
            if not comp:
                continue
            asset = None
            try:
                asset = comp.get_editor_property('static_mesh')
            except Exception:
                try:
                    asset = comp.get_editor_property('skeletal_mesh_asset')
                except Exception:
                    pass
            if not asset:
                continue
            name = asset.get_name()
            f = family(name)
            fams[f] += 1
            mlist = []
            for i in range(comp.get_num_materials()):
                m = comp.get_material(i)
                if m:
                    mlist.append(m.get_name())
            for m in mlist:
                mats[f][m] += 1
            meshes.append((f, name, mlist, a))

        # ---- doorways: what stands beside a door ----
        doors = []
        for f, name, mlist, a in meshes:
            if not any(k.lower() in name.lower() for k in DOORISH):
                continue
            loc = a.get_actor_location()
            near = collections.Counter()
            for f2, n2, _, a2 in meshes:
                if a2 == a:
                    continue
                d = a2.get_actor_location()
                if abs(d.x - loc.x) < NEAR_DOOR and abs(d.y - loc.y) < NEAR_DOOR and abs(d.z - loc.z) < NEAR_DOOR * 2:
                    near[n2] += 1
            doors.append({'mesh': name, 'at': vec(loc), 'yaw': round(a.get_actor_rotation().yaw, 1),
                          'materials': mlist, 'beside': near.most_common(8)})

        # ---- lights, and what fixture each belongs to ----
        light_rows = []
        for l in lights:
            lc = None
            for prop in ('light_component', 'point_light_component', 'spot_light_component',
                         'rect_light_component', 'directional_light_component'):
                try:
                    lc = l.get_editor_property(prop)
                    if lc:
                        break
                except Exception:
                    continue
            if not lc:
                continue
            loc = l.get_actor_location()
            row = {'class': l.get_class().get_name(), 'at': vec(loc),
                   'mobility': str(l.root_component.mobility) if l.root_component else '?'}
            for prop, key in (('intensity', 'intensity'), ('attenuation_radius', 'radius'),
                              ('light_color', 'colour'), ('intensity_units', 'units'),
                              ('source_radius', 'source_radius'), ('outer_cone_angle', 'cone'),
                              ('temperature', 'temperature'), ('use_temperature', 'use_temperature'),
                              ('cast_shadows', 'shadows'), ('volumetric_scattering_intensity', 'volumetric')):
                try:
                    v = lc.get_editor_property(prop)
                    row[key] = str(v) if not isinstance(v, (int, float, bool)) else v
                except Exception:
                    pass
            # Which mesh is it attached to, in that mesh's own frame?
            best, bestd = None, 1e9
            for f2, n2, _, a2 in meshes:
                d = a2.get_actor_location()
                dist = math.dist((loc.x, loc.y, loc.z), (d.x, d.y, d.z))
                if dist < bestd:
                    best, bestd = a2, dist
                    row['nearest'] = n2
            if best and bestd < NEAR_LIGHT:
                rel = best.get_actor_transform().inverse_transform_location(loc)
                row['nearest_dist'] = round(bestd, 1)
                row['offset_in_mesh_frame'] = vec(rel)
            else:
                row['nearest'] = row.get('nearest', None)
                row['nearest_dist'] = round(bestd, 1) if best else None
            light_rows.append(row)

        report = {
            'map': map_path,
            'actors': len(actors),
            'mesh_actors': len(meshes),
            'families': fams.most_common(),
            'materials_by_family': {k: v.most_common(6) for k, v in sorted(mats.items())},
            'doorways': doors,
            'lights': light_rows,
        }
        name = map_path.split('/')[-1]
        pack = map_path.split('/')[2] if map_path.startswith('/Game/Synty/') else map_path.split('/')[2]
        io.open(os.path.join(OUT, '%s__%s.json' % (pack, name)), 'w', encoding='utf-8', newline='\n').write(
            json.dumps(report, indent=1))

        lc = collections.Counter(r['class'] for r in light_rows)
        print('%-30s %5d actors  %4d meshes  %3d lights %s' % (name, len(actors), len(meshes), len(light_rows), dict(lc)))
        print('    top families: %s' % ', '.join('%s x%d' % (k, v) for k, v in fams.most_common(8)))
        if doors:
            print('    doorish: %s' % ', '.join(sorted(set(d['mesh'] for d in doors))[:6]))

    les.load_level(HOME)
    print('')
    print('SURVEY written to', OUT)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
