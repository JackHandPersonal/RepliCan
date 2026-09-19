"""Puts every weapon's muzzle on its BARREL, not on the middle of its bounding box.

    python Tools/ue_remote.py --file Tools/derive_muzzles.py

Tools/derive_weapon_data.py placed the muzzle at the far face of the bounds, centred across the
bounds -- which on a rifle whose stock and grip hang well below the barrel is a point under the
barrel, and the flash and the shot came from there. A barrel is the thing that reaches
furthest forward: the vertices in the front-most SLAB of the mesh (the last FRONT_CM along
HAC1 +X) are the muzzle face, and their centroid is on the bore. That is what is written, for
every ranged weapon, and grip_reseated-style hand edits are not needed: it is measured.
"""
import unreal, io, json, os, traceback

CAT = os.path.join(unreal.Paths.project_dir(), 'Content', 'GameData', 'UI', 'Weapons.json')
FRONT_CM = 2.0
ONLY = []

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play')
    Q = unreal.GeometryScript_MeshQueries
    doc = json.load(io.open(CAT, encoding='utf-8'))
    changed = []
    for key, e in doc['weapons'].items():
        if ONLY and key not in ONLY: continue
        if not e.get('ranged') or e.get('space') != 'hac1': continue
        m = unreal.load_asset(e['mesh'])
        if not m: print('MISSING', key); continue
        dyn = unreal.DynamicMesh()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(m, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        pts = []
        for i in range(dyn.get_vertex_count()):
            p, ok = Q.get_vertex_position(dyn, i)
            if ok: pts.append((p.x, p.y, p.z))
        if not pts: continue
        xmax = max(p[0] for p in pts)
        slab = [p for p in pts if p[0] >= xmax - FRONT_CM]
        cy = sum(p[1] for p in slab) / len(slab); cz = sum(p[2] for p in slab) / len(slab)
        new = [round(xmax, 2), round(cy, 2), round(cz, 2)]
        old = e.get('muzzle')
        if old != new:
            changed.append((key, old, new)); e['muzzle'] = new
    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('MUZZLES moved: %d' % len(changed))
    for key, old, new in changed[:40]:
        print('  %-30s %s -> %s' % (key, old, new))
except Exception:
    print('ERROR', traceback.format_exc())
