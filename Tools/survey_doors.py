"""Every door-shaped mesh in the packs we hold, measured, so a room gets a door of its own
scale. Run in the editor: python Tools/ue_remote.py --file Tools/survey_doors.py

The cabins were built with the LIFT door (SM_Bld_Lift_Door_*), which is a two-metre-wide car
door; a crew cabin wants something a person fits through. For each StaticMesh whose name says
door, doorframe, hatch or gate:

  size        the mesh's bounding box, cm (x, y, z) -- a leaf's size, or a wall piece's
  opening     for a WALL piece with a hole in it, the hole: its width along the wall's long
              axis at waist height and the lintel height above the floor, found from the
              triangles themselves (a flat wall covers a run of x at a height; the run it
              does not cover is the doorway)
  pivot       where the origin sits within the box, so a placing script knows what it holds

Printed sorted by opening / leaf width, and written to Saved/ClaudeAssist/doors_report.txt.
"""
import unreal, os, re
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils
ROOTS = ['/Game/PolygonSciFiSpace', '/Game/PolygonSciFiWorlds', '/Game/PolygonCyberCity', '/Game/Synty', '/Game/PolygonMech']
WORDS = re.compile(r'door|hatch|gate|airlock', re.I)
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


def opening(dyn, box):
    """The hole in a wall piece. The wall's long axis is whichever of x/y is longer; the thin
    axis is the other. Triangles are projected onto (long, z); coverage at waist height
    (z 100) along the long axis gives the doorway's width as the longest uncovered run that is
    inside the piece; the lintel is the lowest covered z above 60 at the doorway's centre."""
    size = box.max - box.min
    long_axis = 0 if size.x >= size.y else 1
    lo = (box.min.x if long_axis == 0 else box.min.y); hi = (box.max.x if long_axis == 0 else box.max.y)
    nb = max(4, int((hi - lo) / 5.0))
    covered = [False] * nb
    tris = []
    for t in range(dyn.get_triangle_count()):
        r = MQ.get_triangle_positions(dyn, t)
        pts = [x for x in r if isinstance(x, unreal.Vector)] if isinstance(r, tuple) else []
        if len(pts) < 3: continue
        us = [(p.x if long_axis == 0 else p.y) for p in pts]; zs = [p.z for p in pts]
        tris.append((min(us), max(us), min(zs), max(zs)))
        if min(zs) <= 100.0 <= max(zs):
            a = int((min(us) - lo) / (hi - lo) * nb); b = int((max(us) - lo) / (hi - lo) * nb)
            for k in range(max(0, a), min(nb, b + 1)): covered[k] = True
    # the longest uncovered run strictly inside the piece
    best = (0, 0, 0); run = 0; start = 0
    for k in range(nb):
        if not covered[k]:
            if run == 0: start = k
            run += 1
            if run > best[0]: best = (run, start, k)
        else: run = 0
    if best[0] * 5.0 < 40.0: return None
    w = best[0] * (hi - lo) / nb
    cu = lo + (best[1] + best[2] + 1) * 0.5 * (hi - lo) / nb
    lintel = None
    for zc in range(60, int(box.max.z) + 1, 2):
        if any(u0 <= cu <= u1 and z0 <= zc <= z1 for u0, u1, z0, z1 in tris):
            lintel = zc; break
    return (round(w, 0), lintel, round(cu - lo, 0))


rows = []
for root in ROOTS:
    for a in reg.get_assets_by_path(root, recursive=True):
        if str(a.asset_class_path.asset_name) != 'StaticMesh': continue
        name = str(a.asset_name)
        if not WORDS.search(name): continue
        if re.search(r'Fridge|Server|Electrical|Cupboard|Locker|Cabinet|Oven|Microwave|Safe|Chest|Vending|Car_|Vehicle|Truck|Ship|Mech_', name): continue
        sm = unreal.load_asset(str(a.package_name))
        if not sm: continue
        box = sm.get_bounding_box(); size = box.max - box.min
        dyn = unreal.DynamicMesh()
        r = AU.copy_mesh_from_static_mesh(sm, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
        op = None
        if max(size.x, size.y) >= 150.0 and size.z >= 150.0 and min(size.x, size.y) <= 80.0:
            try: op = opening(dyn, box)
            except Exception as e: op = None
        pack = str(a.package_name).split('/')[2] if '/Synty/' not in str(a.package_name) else str(a.package_name).split('/')[3]
        rows.append((pack, name, (round(size.x), round(size.y), round(size.z)), (round(-box.min.x), round(-box.min.y), round(-box.min.z)), op, dyn.get_triangle_count()))

lines = ['DOOR SURVEY: %d meshes' % len(rows), '',
         'WALL PIECES WITH A DOORWAY (opening width x lintel height; where along the piece the hole is)']
walls = [r for r in rows if r[4]]
for pack, name, size, piv, op, nt in sorted(walls, key=lambda r: r[4][0]):
    lines.append('  %-16s %-44s size %4d x %3d x %3d   opening %4.0f wide, lintel %s   at %s from the end' % (pack, name, size[0], size[1], size[2], op[0], op[1], op[2]))
lines += ['', 'LEAVES, FRAMES AND THE REST (by width)']
for pack, name, size, piv, op, nt in sorted([r for r in rows if not r[4]], key=lambda r: max(r[2][0], r[2][1])):
    lines.append('  %-16s %-44s size %4d x %3d x %3d   pivot in from min %s   tris %d' % (pack, name, size[0], size[1], size[2], piv, nt))
text = '\n'.join(lines)
print(text)
d = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeAssist'); os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'doors_report.txt'), 'w', encoding='utf-8').write(text)
