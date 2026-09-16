"""Which weapons carry a scope that can be separated from the gun, found by pulling the mesh apart.

    python Tools/ue_remote.py --file Tools/survey_weapon_parts.py

Synty ships each weapon as ONE static mesh. Some of them have an optic modelled on top -- a
scope, a holosight, a carry-handle sight -- and to mix and match optics that piece has to come
off. There is no per-part data in the asset, but the modelling usually gives it away: an optic
is built as its own closed shell sitting on the rail, not welded into the receiver's triangles.
GeometryScript can split a mesh into CONNECTED COMPONENTS, and a shell that is its own component
is a shell we can lift off.

For every weapon in UI/Weapons.json this reports how many components the mesh splits into, and
for each component that sits ON TOP of the bore line in the receiver's zone -- above the bore,
between the grip and the muzzle -- its size and position. Those are the optic candidates. A
weapon whose whole body is one component has its sight welded on, and needs a cut rather than a
split; those are listed too, so the two cases are not confused.

HAC1 space throughout: +X downrange, +Z up, origin at the trigger hand.
"""
import unreal, io, json, os, traceback

CAT = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
OUT = os.path.join(unreal.Paths.project_dir(), 'Tools', 'weapon_parts.json')
MIN_TRIS = 24           # smaller than this is a screw head, not a part
ABOVE_BORE_CM = 1.0     # a part has to sit at least this far above the bore to count as on top

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before surveying')
    cat = json.load(io.open(CAT, encoding='utf-8'))['weapons']
    Q = unreal.GeometryScript_MeshQueries
    D = unreal.GeometryScript_MeshDecomposition

    report = {}
    rows = []
    for key, entry in sorted(cat.items()):
        mesh = unreal.load_asset(entry['mesh'])
        if not mesh:
            continue
        dyn = unreal.DynamicMesh()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        total_tris = dyn.get_triangle_count()
        # The bore: the mid-height of the forward third of the mesh, on the centreline.
        pts = []
        for i in range(dyn.get_vertex_count()):
            p, ok = Q.get_vertex_position(dyn, i)
            if ok:
                pts.append((p.x, p.y, p.z))
        if not pts:
            continue
        x_hi = max(p[0] for p in pts)
        fwd = [p for p in pts if p[0] > x_hi * 0.55 and abs(p[1]) <= 3.0]
        bore = (min(p[2] for p in fwd) + max(p[2] for p in fwd)) * 0.5 if fwd else 0.0

        pool = unreal.DynamicMeshPool()
        _src, parts = D.split_mesh_by_components(dyn, pool)
        comps = []
        for part in parts:
            n = part.get_triangle_count()
            if n < MIN_TRIS:
                continue
            xs, ys, zs = [], [], []
            for i in range(part.get_vertex_count()):
                p, ok = Q.get_vertex_position(part, i)
                if ok:
                    xs.append(p.x); ys.append(p.y); zs.append(p.z)
            if not xs:
                continue
            comps.append({'tris': n, 'x': [round(min(xs), 1), round(max(xs), 1)],
                          'y': [round(min(ys), 1), round(max(ys), 1)],
                          'z': [round(min(zs), 1), round(max(zs), 1)]})
        comps.sort(key=lambda c: -c['tris'])
        body = comps[0] if comps else None
        # Optic candidates: not the body, sitting wholly above the bore, roughly centred on Y.
        optics = [c for c in comps[1:]
                  if c['z'][0] >= bore + ABOVE_BORE_CM and abs((c['y'][0] + c['y'][1]) * 0.5) < 3.0
                  and c['x'][0] > -6.0]
        report[key] = {'mesh': entry['mesh'], 'tris': total_tris, 'bore_z': round(bore, 2),
                       'components': len(comps), 'optic_candidates': optics}
        tag = ('OPTIC x%d' % len(optics)) if optics else ('welded' if len(comps) <= 1 else 'multi, none on top')
        rows.append((key, total_tris, len(comps), tag,
                     ', '.join('%dt x%.0f..%.0f z%.0f..%.0f' % (o['tris'], o['x'][0], o['x'][1], o['z'][0], o['z'][1]) for o in optics[:2])))

    io.open(OUT, 'w', encoding='utf-8', newline='\n').write(json.dumps(report, indent=1))
    n_optic = sum(1 for r in rows if r[3].startswith('OPTIC'))
    n_weld = sum(1 for r in rows if r[3] == 'welded')
    print('%d weapons: %d have a separable part on top of the bore, %d are one welded shell, %d split but nothing on top'
          % (len(rows), n_optic, n_weld, len(rows) - n_optic - n_weld))
    print('')
    print('%-30s %6s %5s  %-22s %s' % ('weapon', 'tris', 'parts', 'verdict', 'candidates (tris, x span, z span)'))
    for key, tris, parts, tag, cand in rows:
        print('%-30s %6d %5d  %-22s %s' % (key, tris, parts, tag, cand))
    print('')
    print('wrote', OUT)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
