"""A SHOULDER point for every weapon: where the stock meets the shoulder for recoil control.
    python Tools/ue_remote.py --file Tools/derive_shoulders.py

HAC1 space: +X downrange, origin at the grip. A stock is whatever the mesh has behind the grip.
  - With a stock (the mesh reaches at least STOCK_MIN behind the grip): the shoulder is the
    centre of the rear-most 2 cm slab of vertices -- the butt plate -- nudged 1 cm forward
    into it so the point sits in the pad rather than on its skin.
  - Without one (pistols, SMGs with no stock, tools, blades): a point in space where a stock
    would end, NO_STOCK behind the grip and a little above it, so the pose still has somewhere
    to pull the weapon back to.
Written to UI/Weapons.json as "shoulder" on every weapon; an entry that already has one is
left alone unless FORCE.
"""
import unreal, json, io, traceback
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'
ONLY = []
FORCE = False
STOCK_MIN = 12.0      # cm behind the grip before it counts as a stock
NO_STOCK = (-28.0, 0.0, 7.0)   # where a stock would end on a weapon that has none

try:
    doc = json.load(io.open(CAT, encoding='utf-8'))
    tally = {'stock': 0, 'none': 0, 'kept': 0, 'missing': 0}
    for key, e in doc['weapons'].items():
        if ONLY and key not in ONLY: continue
        if (e.get('stance') or '').startswith('Pistol'): e.pop('shoulder', None); continue   # no stock: a pistol indexes off the hand (its stance's carry offsets)
        if e.get('shoulder') and not FORCE: tally['kept'] += 1; continue
        path = (e.get('body_mesh') or e.get('mesh') or '').split('.')[0]
        m = unreal.load_asset(path) if path else None
        if not m: tally['missing'] += 1; print('MISSING', key, path); continue
        dyn = unreal.DynamicMesh()
        r = AU.copy_mesh_from_static_mesh(m, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
        pts = []
        for i in range(dyn.get_vertex_count()):
            p, ok = MQ.get_vertex_position(dyn, i)
            if ok: pts.append((p.x, p.y, p.z))
        if not pts: tally['missing'] += 1; continue
        xmin = min(p[0] for p in pts)
        if xmin <= -STOCK_MIN:
            slab = [p for p in pts if p[0] <= xmin + 2.0]
            cy = sum(p[1] for p in slab) / len(slab); cz = sum(p[2] for p in slab) / len(slab)
            e['shoulder'] = [round(xmin + 1.0, 2), round(cy, 2), round(cz, 2)]; tally['stock'] += 1
            print('%-36s stock  butt at x %.1f -> shoulder %s' % (key, xmin, e['shoulder']))
        else:
            e['shoulder'] = list(NO_STOCK); tally['none'] += 1
            print('%-36s no stock (rear at x %.1f) -> shoulder %s' % (key, xmin, e['shoulder']))
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('SHOULDERS', tally)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
