"""Put a crosshair on every magnified optic.

M_RedDot draws a DOT and a ring, sized as fractions of the lens. That is what a reflex sight has and
it is right for one -- but on a magnified scope it is a speck in the middle of a large pane, which
reads as no reticle at all. A scope wants crosshairs.

Rather than teach the material to draw them (a crosshair in a material graph is a pile of nodes that
have to be got exactly right blind), the arms are GEOMETRY: four thin bars lying in the plane of the
lens, with a gap at the centre so the target is not hidden by its own reticle, on an additive unlit
material so they glow through whatever is behind the glass.

Which plane the lens lies in is already known: the one perpendicular to the optic long axis, the
same one the lens disc was placed in.

Idempotent: bars from a previous run are deleted by material id before new ones go on.

  Tools/ue_remote --file Tools/add_scope_crosshairs
"""
import unreal, json, io, os

AU = unreal.GeometryScript_AssetUtils
PRIM = unreal.GeometryScript_Primitives
MATS = unreal.GeometryScript_Materials
CAT = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
BEAM = '/Game/RepliCan/Materials/M_LaserBeam'
OUT_DIR = '/Game/RepliCan/Optics'
SLOT = 'Crosshair'


def crosshair_material():
    name = 'MI_ScopeCrosshair'
    path = OUT_DIR + '/' + name
    mi = unreal.load_asset(path)
    if not mi:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, OUT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(BEAM))
    # Additive and unlit already; it only wants a colour and a heat. Cold and bright, so it reads
    # against the warm interior of this station rather than disappearing into it.
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        mi, 'Colour', unreal.LinearColor(0.55, 1.0, 0.85, 1.0))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, 'Heat', 6.0)
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    return mi


cat = json.load(io.open(CAT, encoding='utf-8'))
mat = crosshair_material()
done, skipped = [], []

for key, o in cat.get('optics', {}).items():
    zoom = float(o.get('zoom') or 1.0)
    if zoom <= 1.01:
        skipped.append((key, 'no magnification: a dot is right for it'))
        continue
    path = (o.get('mesh') or '').split('.')[0]
    asset = unreal.load_asset(path) if path else None
    if not asset:
        skipped.append((key, 'no mesh'))
        continue

    slots = [s for s in asset.get_editor_property('static_materials')]
    existing = None
    for i, sm in enumerate(slots):
        if str(sm.material_slot_name) == SLOT:
            existing = i
        elif sm.material_interface and 'Crosshair' in sm.material_interface.get_name():
            existing = i

    d = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(asset, d, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                      unreal.GeometryScriptMeshReadLOD())
    d = r[0] if isinstance(r, tuple) else d
    MATS.enable_material_i_ds(d)
    if existing is not None:
        MATS.delete_triangles_by_material_id(d, existing)
    new_id = existing if existing is not None else len(slots)

    # The bare optic, with any previous crosshair gone: measured, never assumed.
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(d)
    box = box[0] if isinstance(box, tuple) else box
    lo, hi = box.min, box.max
    mid_all = unreal.Vector((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, (lo.z + hi.z) * 0.5)
    e = unreal.Vector((hi.x - lo.x) * 0.5, (hi.y - lo.y) * 0.5, (hi.z - lo.z) * 0.5)
    dims = [('X', e.x), ('Y', e.y), ('Z', e.z)]
    axis, half = max(dims, key=lambda dd: dd[1])
    cross = sorted(v for a, v in dims if a != axis)
    radius = max(0.35, cross[0] * 0.78)

    rot = o.get('rot') or [0, 0, 0]
    R = unreal.Rotator(pitch=float(rot[0]), yaw=float(rot[1]), roll=float(rot[2]))
    along = {'X': unreal.Vector(1, 0, 0), 'Y': unreal.Vector(0, 1, 0), 'Z': unreal.Vector(0, 0, 1)}[axis]
    turned = unreal.MathLibrary.quat_rotate_vector(R.quaternion(), along)
    sign = -1.0 if turned.x > 0 else 1.0
    c = unreal.Vector(mid_all.x, mid_all.y, mid_all.z)
    # A hair in FRONT of the lens disc (which sits at 0.97) so the bars are never buried inside it.
    if axis == 'X':
        c.x += sign * (half * 0.94)
    elif axis == 'Y':
        c.y += sign * (half * 0.94)
    else:
        c.z += sign * (half * 0.94)

    if axis == 'X':
        u, v = unreal.Vector(0, 1, 0), unreal.Vector(0, 0, 1)
    elif axis == 'Y':
        u, v = unreal.Vector(1, 0, 0), unreal.Vector(0, 0, 1)
    else:
        u, v = unreal.Vector(1, 0, 0), unreal.Vector(0, 1, 0)

    gap = radius * 0.16
    arm = radius * 0.80
    thick = max(0.02, radius * 0.045)
    depth = max(0.01, radius * 0.02)
    popts = unreal.GeometryScriptPrimitiveOptions()
    popts.material_id = new_id

    def bar(dirv, otherv, length, offset):
        mid = length * 0.5 + offset
        at = unreal.Vector(c.x + dirv.x * mid, c.y + dirv.y * mid, c.z + dirv.z * mid)
        # A box is axis aligned, so extents are given per world axis: long along dirv, thin across
        # otherv, thinnest through the glass.
        dx = abs(dirv.x) * length + abs(otherv.x) * thick + (1 - abs(dirv.x) - abs(otherv.x)) * depth
        dy = abs(dirv.y) * length + abs(otherv.y) * thick + (1 - abs(dirv.y) - abs(otherv.y)) * depth
        dz = abs(dirv.z) * length + abs(otherv.z) * thick + (1 - abs(dirv.z) - abs(otherv.z)) * depth
        PRIM.append_box(d, popts, unreal.Transform(at, unreal.Rotator(), unreal.Vector(1, 1, 1)),
                        max(dx, 0.01), max(dy, 0.01), max(dz, 0.01), 0, 0, 0,
                        unreal.GeometryScriptPrimitiveOriginMode.CENTER)

    for dirv, otherv in ((u, v), (v, u)):
        neg = unreal.Vector(-dirv.x, -dirv.y, -dirv.z)
        bar(dirv, otherv, arm, gap)
        bar(neg, otherv, arm, gap)

    to = unreal.GeometryScriptCopyMeshToAssetOptions()
    to.enable_recompute_normals = False
    to.enable_recompute_tangents = True
    AU.copy_mesh_to_static_mesh(d, asset, to, unreal.GeometryScriptMeshWriteLOD())

    rebuilt = [s for s in asset.get_editor_property('static_materials')]
    while len(rebuilt) <= new_id:
        rebuilt.append(unreal.StaticMaterial())
    rebuilt[new_id].material_interface = mat
    rebuilt[new_id].material_slot_name = SLOT
    asset.modify()
    asset.set_editor_property('static_materials', rebuilt)
    unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outer(), mat.get_outer()], False)
    done.append((key, zoom, round(radius, 2), round(arm, 2)))

print('crosshairs fitted: %d' % len(done))
for k, z, r2, a in done:
    print('   %-26s %.0fx  lens r %.2f  arm %.2f' % (k, z, r2, a))
