"""How a Synty demo map is BUILT: module grid, corridor runs, mounting heights, interior dressing.

Tools/demo_assemblies.py answers "which meshes make up one prop" by name (SM_X plus SM_X_Part).
This answers the other half, which no naming convention records:

  * WHAT GRID the building pieces sit on -- the modulus that actually divides their positions,
    measured from the pack's own layout rather than assumed to be 500 because ours is.
  * HOW CORRIDORS RUN -- pieces repeated along a line, their spacing and their yaws.
  * HOW HIGH things mount -- the z a family is placed at, which separates floor-standing props
    from wall-mounted ones from ceiling fixtures without anyone having to label them.
  * WHAT DRESSES AN INTERIOR -- which props cluster inside, how far off the nearest wall they sit.

Written because replicating a pack's look is mostly getting these four right, and all four are
invisible in the content browser: they only exist in how the pack's own artists placed things.

    Tools/ue_remote.py --file Tools/scan_pack_layout.py

Reads the CURRENTLY OPEN level and writes Docs/<Map>_Layout.md. It does not switch levels, does
not save, and changes nothing -- open the map you want scanned first.
"""
import unreal, os, math, collections

OUT = r'C:\Dev\Games\RepliCan\Docs'
GRID_CANDIDATES = [100, 125, 150, 200, 250, 300, 400, 500, 600, 750, 800, 1000, 1200]
TOL = 6.0          # cm: how close to a grid line still counts as on it
NEAR = 1200.0      # cm: what counts as "in the same room" for dressing


def families(rows):
    """Group mesh names into families by dropping the trailing _NN variant number."""
    fam = collections.defaultdict(list)
    for n, x, y, z, yaw, sx in rows:
        base = n
        parts = base.split('_')
        while parts and parts[-1].isdigit():
            parts = parts[:-1]
        fam['_'.join(parts) or n].append((n, x, y, z, yaw, sx))
    return fam


def grid_fit(vals):
    """The largest candidate modulus that most positions land on. Returns (modulus, fraction)."""
    best = (None, 0.0)
    for g in GRID_CANDIDATES:
        if len(vals) < 3:
            break
        on = sum(1 for v in vals if min(v % g, g - (v % g)) <= TOL)
        frac = on / float(len(vals))
        if frac >= 0.75 and frac >= best[1] - 0.02:
            best = (g, frac)
    return best


def runs(pts, axis, step_tol=12.0):
    """Longest chain of pieces evenly spaced along one axis. Returns (count, spacing)."""
    if len(pts) < 3:
        return (0, 0.0)
    key = 0 if axis == 'x' else 1
    other = 1 - key
    lanes = collections.defaultdict(list)
    for p in pts:
        lanes[round(p[other] / 50.0) * 50].append(p[key])
    best = (0, 0.0)
    for lane in lanes.values():
        lane = sorted(set(round(v, 1) for v in lane))
        if len(lane) < 3:
            continue
        deltas = [round(b - a, 1) for a, b in zip(lane, lane[1:])]
        if not deltas:
            continue
        common = collections.Counter(d for d in deltas if d > 1.0).most_common(1)
        if not common:
            continue
        spacing, n = common[0]
        if n + 1 > best[0] and abs(spacing) > 1.0:
            best = (n + 1, spacing)
    return best


def scan():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world().get_name()

    rows = []
    for a in eas.get_all_level_actors():
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.static_mesh
            if not m:
                continue
            L = c.get_world_location()
            R = c.get_world_rotation() if hasattr(c, 'get_world_rotation') else a.get_actor_rotation()
            S = a.get_actor_scale3d()
            rows.append((m.get_name(), L.x, L.y, L.z, R.yaw, S.x))
    return world, rows


if __name__ == '__main__':
    world, rows = scan()
    fam = families(rows)
    # THE FLOOR IS NOT ONE PLANE. Demo_Corporation has floor pieces in bands at -250, -500, -1750,
    # -2250 and -2500, and props spread across six z bands -- it is a multi-storey site. Measuring
    # every prop against one global datum gave "median z -1962" for a bed, against a floor found at
    # -58, which is not a mounting height, it is the difference between two storeys. So each prop is
    # measured against the nearest floor piece BELOW it in plan, and says so when there isn't one.
    floors = [(x, y, z) for n, x, y, z, yw, s in rows if 'Floor' in n or 'Ground' in n]
    floor_z = collections.Counter(round(z) for _x, _y, z in floors)
    base_z = floor_z.most_common(1)[0][0] if floor_z else 0
    bands = [b for b, _c in collections.Counter(int(z // 250) * 250 for _x, _y, z in floors).most_common(8)]
    multi = len([b for b in bands if abs(b - bands[0]) > 500]) >= 2 if bands else False

    def local_floor(x, y, z, radius=1500.0):
        """z of the nearest floor piece under (x, y), or None. The datum a prop actually stands on."""
        best, bz = None, None
        for fx, fy, fz in floors:
            if fz > z + 120.0:      # above the prop: a ceiling or the storey over it
                continue
            d = math.hypot(x - fx, y - fy)
            if d < radius and (best is None or d < best):
                best, bz = d, fz
        return bz

    # the grid, measured over building pieces only -- props are dressed by eye and would blur it
    bld = [(x, y) for n, x, y, z, yw, s in rows if n.startswith(('SM_Bld', 'SM_Env_Ground', 'SM_Floor'))]
    gx = grid_fit([abs(p[0]) for p in bld])
    gy = grid_fit([abs(p[1]) for p in bld])

    lines = []
    A = lines.append
    A('# %s -- how this map is assembled' % world)
    A('')
    A('Scanned by `Tools/scan_pack_layout.py` from the map as the pack ships it. Every number here is')
    A('measured from Synty\'s own placement, not chosen by us.')
    A('')
    A('- components: **%d**, distinct meshes: **%d**, families: **%d**' % (len(rows), len({r[0] for r in rows}), len(fam)))
    A('- commonest floor z: **%s**%s' % (base_z, '  -- **MULTI-STOREY**: floor pieces also at %s, so a single '
      'datum would be meaningless and heights below are measured against the nearest floor UNDER each prop'
      % ', '.join(str(b) for b in sorted(bands)[:6]) if multi else ''))
    A('- building grid: **x %s** (%.0f%% of pieces on it), **y %s** (%.0f%%)'
      % (gx[0] or 'none found', gx[1] * 100, gy[0] or 'none found', gy[1] * 100))
    A('')
    A('## Families, mounting height and rotation')
    A('')
    A('`mount` is the median z above the floor plane: ~0 stands on the floor, a metre or two is')
    A('wall-mounted or waist-height, above that is ceiling or upper storey. `yaws` shows whether a')
    A('family is axis-aligned (only 0/90/180/270) or placed freely -- free rotation means the pack')
    A('does not expect it to tile.')
    A('')
    A('| family | n | mount z | yaws | longest run | spacing |')
    A('|---|---|---|---|---|---|')
    for name, items in sorted(fam.items(), key=lambda kv: -len(kv[1])):
        if len(items) < 3:
            continue
        zs = sorted(i[3] - base_z for i in items)
        med = zs[len(zs) // 2]
        yaws = sorted({round(i[4] / 90.0) * 90 % 360 for i in items})
        axis_aligned = all(abs(((i[4] % 90) + 90) % 90) < 2.0 or abs(((i[4] % 90) + 90) % 90 - 90) < 2.0 for i in items)
        pts = [(i[1], i[2]) for i in items]
        rx, ry = runs(pts, 'x'), runs(pts, 'y')
        run = max(rx, ry)
        A('| `%s` | %d | %.0f | %s | %s | %s |'
          % (name, len(items), med,
             ('%s' % yaws) if axis_aligned else 'free',
             run[0] or '-', ('%.0f' % run[1]) if run[0] else '-'))
    A('')
    A('## Interior dressing')
    A('')
    A('Props within %d cm of a SHELL piece -- a wall, a corridor tube or a pod body -- by how far off' % NEAR)
    A('it they sit. That gap is what decides whether a thing reads as against the wall or adrift in')
    A('the room, and it is the number a replication has to match.')
    A('')
    A('Matching only `SM_Bld*Wall*` produced an empty table on an exterior demo, which is correct and')
    A('useless: this pack builds its interiors out of corridor tubes and pod shells, not wall panels.')
    A('')
    walls = [(x, y, z) for n, x, y, z, yw, s in rows
             if n.startswith('SM_Bld') and ('Wall' in n or 'Corridor' in n or 'Pod_' in n)]
    props = [(n, x, y, z) for n, x, y, z, yw, s in rows if n.startswith('SM_Prop')]
    off = []
    for n, x, y, z in props:
        best = None
        for wx, wy, wz in walls:
            d = math.hypot(x - wx, y - wy)
            if best is None or d < best:
                best = d
        if best is not None and best < NEAR:
            lf = local_floor(x, y, z)
            off.append((best, n, (z - lf) if lf is not None else None))
    byfam = collections.defaultdict(list)
    for d, n, z in off:
        parts = n.split('_')
        while parts and parts[-1].isdigit():
            parts = parts[:-1]
        byfam['_'.join(parts)].append((d, z))
    A('| prop family | n | median gap to nearest wall | median z |')
    A('|---|---|---|---|')
    for name, vs in sorted(byfam.items(), key=lambda kv: -len(kv[1]))[:40]:
        ds = sorted(v[0] for v in vs)
        zs = sorted(v[1] for v in vs if v[1] is not None)
        A('| `%s` | %d | %.0f | %s |'
          % (name, len(vs), ds[len(ds) // 2],
             ('%.0f' % zs[len(zs) // 2]) if zs else 'no floor under it'))

    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, '%s_Layout.md' % world)
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print('wrote', path)
    print('components %d  families %d  grid x=%s y=%s  floor z=%s' % (len(rows), len(fam), gx[0], gy[0], base_z))
