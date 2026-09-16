"""Find whole rooms in the SciFi Horror demo and write them down so they can be rebuilt in ours.

    python Tools/ue_remote.py --file Tools/horror_capture.py

WHY A RECIPE AND NOT A COPY. Only one level is open at a time, so "copy those actors into our
map" is not a thing that can happen in one pass. This pass opens the horror demo, works out
which actors belong together, writes the pieces to Tools/HorrorSection/<name>.json -- mesh
path, transform, material overrides, and for lights every setting that matters -- and puts our
level back. Tools/facility_layout.py then rebuilds from that file through ensure(), so the
transplant lives under the same manifest as everything else: revisable, and deletable by hand
without a re-run putting it back.

WORKING WITH THE TILES, NOT ACROSS THEM. The first version of this flooded connected cells
hoping to find rooms, and learned something worth writing down: the horror demo is ONE building.
All 5458 mesh actors are a single connected blob, 56 m by 75 m. There are no separable rooms to
find, so the question is not "which component" but "where can this be cut without it showing".

So the cut is scored instead of guessed. A window the size of the section we want is slid over
the map on the pack's own 500 cm tile lines, and each position is judged on two things:

    what is inside     enough structure to be a place, and a mix of families rather than a
                       yard full of pipes
    the seam           how many pieces the window's edge passes THROUGH, using each actor's
                       real world bounds. A cut that runs along an existing wall slices
                       nothing; a cut through the middle of a room slices twenty pieces and
                       every one of them is a visible half-object.

The report ranks positions by seam cost so the pick is a decision made by reading it. Cheap
insurance: whatever is left open at the edges still gets capped with the pack's own wall pieces
when it is placed.

Two passes, set by MODE:

    'survey'   rank window positions: what is inside, what the cut costs
    'capture'  write the recipe for the window at CAPTURE_XY
"""
import unreal, io, json, os, math, collections, traceback

MODE = 'capture'
# South-west corner of the window to capture, in the demo's own coordinates. Set from the survey.
CAPTURE_XY = (-2500.0, -5000.0)
CAPTURE_NAME = 'horror_section'
# How big a section to take, in cm. 4000 x 4000 is eight tiles square -- bigger than our bay,
# which is the right size for somewhere you arrive by lift and then explore.
WINDOW = (3000.0, 3000.0)
# Only keep what is within this much of the floor the window sits on. The demo is stacked; a
# window that takes everything takes the storey above as well.
WINDOW_Z = 900.0
# How many of the best positions to print.
TOP = 12

MAP = '/Game/Synty/PolygonSciFiHorror/Maps/Demo'
HOME = '/Game/RepliCan/Maps/Lvl_AsteroidFacility'
OUT = os.path.join(unreal.Paths.project_dir(), 'Tools', 'HorrorSection')

# 500 cm is the pack's own tile, so a cell is one tile and two tiles that touch are connected.
CELL = 500.0
# How many actors a component needs before it is worth reporting at all.
MIN_ACTORS = 40
# Components bigger than this in either direction are the whole demo hooked together through a
# corridor, not a room. Reported, but flagged.
BIG_CM = 6000.0


def family(name):
    """SM_Bld_Wall_01_Alt -> Bld_Wall. The kit's vocabulary, not its part numbers."""
    n = name
    for p in ('SM_', 'SKM_', 'SK_'):
        if n.startswith(p):
            n = n[len(p):]
    keep = []
    for b in n.split('_'):
        if b.isdigit() or (len(b) == 3 and b[:2].isdigit()):
            break
        keep.append(b)
        if len(keep) >= 2:
            break
    return '_'.join(keep) if keep else n


def vec(v):
    return [round(v.x, 2), round(v.y, 2), round(v.z, 2)]


def read_mesh_actors(eas):
    out = []
    for a in eas.get_all_level_actors():
        try:
            c = a.static_mesh_component
            m = c.get_editor_property('static_mesh')
        except Exception:
            continue
        if not m:
            continue
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        sc = a.get_actor_scale3d()
        mats = []
        for i in range(c.get_num_materials()):
            mm = c.get_material(i)
            mats.append(mm.get_path_name() if mm else None)
        out.append({'mesh': m.get_path_name(), 'name': m.get_name(), 'family': family(m.get_name()),
                    'loc': vec(loc), 'rot': [round(rot.roll, 2), round(rot.pitch, 2), round(rot.yaw, 2)],
                    'scale': vec(sc), 'mats': mats})
    return out


def read_lights(eas):
    out = []
    for a in eas.get_all_level_actors():
        comp = None
        for attr in ('light_component', 'point_light_component', 'spot_light_component'):
            try:
                comp = a.get_editor_property(attr)
            except Exception:
                comp = None
            if comp:
                break
        if not comp:
            continue
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        e = {'class': a.get_class().get_name(), 'loc': vec(loc),
             'rot': [round(rot.roll, 2), round(rot.pitch, 2), round(rot.yaw, 2)]}
        for prop, key in (('intensity', 'intensity'), ('light_color', 'color'),
                          ('attenuation_radius', 'radius'), ('source_radius', 'source_radius'),
                          ('outer_cone_angle', 'outer_cone'), ('inner_cone_angle', 'inner_cone'),
                          ('cast_shadows', 'shadows'), ('intensity_units', 'units'),
                          ('temperature', 'temperature'), ('use_temperature', 'use_temperature')):
            try:
                v = comp.get_editor_property(prop)
            except Exception:
                continue
            if key == 'color':
                e[key] = [v.r, v.g, v.b]
            elif key == 'units':
                e[key] = str(v)
            else:
                e[key] = v if isinstance(v, (int, float, bool)) else str(v)
        out.append(e)
    return out


def world_aabb(a, mesh_bounds):
    """An actor's axis-aligned world box, near enough for a seam test.

    The kit is built on cardinal yaws, so rotating the mesh's own extent by the actor's yaw and
    taking the absolute value is exact for every piece that matters and close enough for the
    handful placed at an angle."""
    b = mesh_bounds.get(a['mesh'])
    if not b:
        return None
    (ox, oy, oz), (ex, ey, ez) = b
    sx, sy, sz = a['scale']
    yaw = math.radians(a['rot'][2])
    c, sn = math.cos(yaw), math.sin(yaw)
    # Centre of the mesh's bounds, moved into world space.
    cx = a['loc'][0] + (ox * sx) * c - (oy * sy) * sn
    cy = a['loc'][1] + (ox * sx) * sn + (oy * sy) * c
    cz = a['loc'][2] + oz * sz
    hx = abs(ex * sx * c) + abs(ey * sy * sn)
    hy = abs(ex * sx * sn) + abs(ey * sy * c)
    hz = abs(ez * sz)
    return (cx - hx, cy - hy, cz - hz, cx + hx, cy + hy, cz + hz)


def read_mesh_bounds(actors):
    """One get_bounds() per distinct mesh, not per actor."""
    out = {}
    for a in actors:
        if a['mesh'] in out:
            continue
        m = unreal.load_asset(a['mesh'])
        if not m:
            out[a['mesh']] = None
            continue
        b = m.get_bounds()
        out[a['mesh']] = ((b.origin.x, b.origin.y, b.origin.z),
                          (b.box_extent.x, b.box_extent.y, b.box_extent.z))
    return out


def score_windows(actors, boxes):
    """Every tile-aligned window position, judged on what is in it and what the cut costs."""
    xs = [a['loc'][0] for a in actors]
    ys = [a['loc'][1] for a in actors]
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(ys), max(ys)
    wx, wy = WINDOW

    # The demo is stacked, so the floor a window sits on is decided first: the most common z
    # among the FLOOR pieces, rounded to the tile, is where people walk.
    floors = [a['loc'][2] for a in actors if 'Floor' in a['name'] or 'Base' in a['name']]
    if not floors:
        floors = [a['loc'][2] for a in actors]
    deck = collections.Counter(round(z / 50.0) * 50.0 for z in floors).most_common(1)[0][0]

    rows = []
    gx = int(math.floor(x0 / CELL))
    gy = int(math.floor(y0 / CELL))
    nx = int(math.ceil((x1 - x0 - wx) / CELL)) + 1
    ny = int(math.ceil((y1 - y0 - wy) / CELL)) + 1
    for ix in range(max(1, nx)):
        for iy in range(max(1, ny)):
            ax = (gx + ix) * CELL
            ay = (gy + iy) * CELL
            bx, by = ax + wx, ay + wy
            inside, seam = 0, 0
            fams = collections.Counter()
            for i, a in enumerate(actors):
                if not (deck - WINDOW_Z <= a['loc'][2] <= deck + WINDOW_Z):
                    continue
                box = boxes[i]
                if not box:
                    continue
                lx, ly, _lz, hx, hy, _hz = box
                if hx <= ax or lx >= bx or hy <= ay or ly >= by:
                    continue            # wholly outside
                if lx >= ax and hx <= bx and ly >= ay and hy <= by:
                    inside += 1
                    fams[a['family']] += 1
                else:
                    seam += 1           # the window's edge passes through this piece
            if inside < 400:
                continue
            rows.append({'at': (ax, ay), 'inside': inside, 'seam': seam,
                         'variety': len(fams), 'fams': fams, 'deck': deck})
    # Fewest sliced pieces first; among equals, the fuller and more varied window.
    rows.sort(key=lambda r: (r['seam'] / max(1.0, r['inside']), -r['inside']))
    return rows, deck


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before capturing')
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    les.load_level(MAP)
    actors = read_mesh_actors(eas)
    lights = read_lights(eas)
    mesh_bounds = read_mesh_bounds(actors)
    boxes = [world_aabb(a, mesh_bounds) for a in actors]
    print('%s: %d mesh actors, %d distinct meshes, %d lights'
          % (MAP, len(actors), len(mesh_bounds), len(lights)))

    rows, deck = score_windows(actors, boxes)
    print('walking deck z = %.0f; window %.0f x %.0f cm' % (deck, WINDOW[0], WINDOW[1]))
    print('')
    print('%-20s %7s %6s %7s  %s' % ('south-west corner', 'inside', 'seam', 'cut', 'what is in it'))
    for r in rows[:TOP]:
        top = ', '.join('%s x%d' % (f, c) for f, c in r['fams'].most_common(6))
        print('(%8.0f, %8.0f) %7d %6d %6.1f%%  %s'
              % (r['at'][0], r['at'][1], r['inside'], r['seam'],
                 100.0 * r['seam'] / max(1, r['inside']), top))

    if MODE == 'capture':
        ax, ay = CAPTURE_XY
        bx, by = ax + WINDOW[0], ay + WINDOW[1]
        keep = []
        for i, a in enumerate(actors):
            box = boxes[i]
            if not box:
                continue
            if not (deck - WINDOW_Z <= a['loc'][2] <= deck + WINDOW_Z):
                continue
            lx, ly, _lz, hx, hy, _hz = box
            # A piece is taken if its CENTRE is in the window. Taking only wholly-inside pieces
            # leaves a fringe of missing wall along every edge; taking anything that overlaps
            # drags in half the next room. The centre rule cuts where the cut already is.
            if ax <= (lx + hx) * 0.5 < bx and ay <= (ly + hy) * 0.5 < by:
                keep.append(i)
        if not keep:
            raise RuntimeError('nothing in the window at %s; survey first' % (CAPTURE_XY,))

        zs = [actors[i]['loc'][2] for i in keep]
        # Relative to the window's own south-west corner at deck height, so placing it in our
        # map is one offset and no arithmetic per piece.
        ox, oy, oz = ax, ay, deck
        pieces = []
        for i in keep:
            a = dict(actors[i])
            a['loc'] = [round(a['loc'][0] - ox, 2), round(a['loc'][1] - oy, 2), round(a['loc'][2] - oz, 2)]
            pieces.append(a)
        keep_lights = []
        for L in lights:
            lx, ly, lz = L['loc']
            if ax <= lx < bx and ay <= ly < by and deck - WINDOW_Z <= lz <= deck + WINDOW_Z:
                L = dict(L)
                L['loc'] = [round(lx - ox, 2), round(ly - oy, 2), round(lz - oz, 2)]
                keep_lights.append(L)
        os.makedirs(OUT, exist_ok=True)
        doc = {'_': 'Captured from %s by Tools/horror_capture.py. Coordinates are relative to the '
                    'window corner at deck height; Tools/facility_layout.py adds one offset.' % MAP,
               'source': MAP, 'window': [ax, ay, WINDOW[0], WINDOW[1]], 'deck': deck,
               'z_range': [round(min(zs) - oz, 1), round(max(zs) - oz, 1)],
               'pieces': pieces, 'lights': keep_lights}
        path = os.path.join(OUT, CAPTURE_NAME + '.json')
        io.open(path, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1))
        print('')
        print('wrote %s: %d pieces, %d lights, z %.0f..%.0f'
              % (path, len(pieces), len(keep_lights), min(zs) - oz, max(zs) - oz))

    les.load_level(HOME)
    print('')
    print('level restored')
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
