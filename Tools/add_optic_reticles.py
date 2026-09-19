"""Give every optic a reticle to look at.

Three optics in the catalogue carry M_RedDot glass (the two red dots and the built-in lens); the
other sixteen -- Synty's scope parts and the factory scopes cut off the weapons -- are solid atlas
meshes with nothing to aim with. Aiming down one of them shows a lump of painted plastic.

This appends a LENS to each of those: a thin disc at the eyepiece end, with its own material slot
carrying an instance of M_RedDot centred and sized on that disc. M_RedDot draws the dot and ring
from LensCentre/LensSize, so a lens of the wrong size or in the wrong place draws its dot off the
glass entirely -- which is exactly what a shared default did to the RDS-2 once before. Each optic
therefore gets its OWN instance with its own numbers.

WHICH END IS THE EYEPIECE is worked out rather than assumed. The disc goes on the end of the long
axis that points BACKWARDS once the optic's catalogue `rot` has been applied -- for the Synty scopes
that yaw is 90 degrees, so the end that faces the shooter is not the end you would pick by looking
at the mesh in isolation.

The optic's `eye` (where the player's eye lines up) is set to the lens for any optic that has none,
so aiming actually looks through the new glass instead of at the rail.

  Tools/ue_remote --file Tools/add_optic_reticles
"""
import unreal, json, io, os

AU = unreal.GeometryScript_AssetUtils
PRIM = unreal.GeometryScript_Primitives
MATS = unreal.GeometryScript_Materials
CAT = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
RED_DOT = '/Game/RepliCan/Materials/M_RedDot'
OUT_DIR = '/Game/RepliCan/Optics'


def dyn_of(asset):
    d = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(asset, d, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                      unreal.GeometryScriptMeshReadLOD())
    return r[0] if isinstance(r, tuple) else d


def reticle_instance(key, centre, radius):
    """An M_RedDot instance centred and sized on this lens, made once per optic."""
    name = 'MI_Reticle_%s' % key
    path = '%s/%s' % (OUT_DIR, name)
    mi = unreal.load_asset(path)
    if not mi:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, OUT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(RED_DOT))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        mi, 'LensCentre', unreal.LinearColor(centre.x, centre.y, centre.z, 0.0))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        mi, 'LensSize', unreal.LinearColor(radius, radius, radius, 0.0))
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    return mi


cat = json.load(io.open(CAT, encoding='utf-8'))
optics = cat.get('optics', {})
done, skipped = [], []

for key, o in optics.items():
    path = (o.get('mesh') or '').split('.')[0]
    asset = unreal.load_asset(path) if path else None
    if not asset:
        skipped.append((key, 'no mesh')); continue
    slots = [s for s in asset.get_editor_property('static_materials')]
    # A lens this script put there before is REPLACED, not skipped: the first version oriented every
    # disc with a positional unreal.Rotator and so laid them all flat, edge-on to the shooter and
    # invisible. Its triangles are deleted by material id and the slot reused.
    existing_id = None
    for i, sm in enumerate(slots):
        if sm.material_interface and 'Reticle' in sm.material_interface.get_name():
            existing_id = i
    if existing_id is None and any(sm.material_interface and 'RedDot' in sm.material_interface.get_name() for sm in slots):
        skipped.append((key, 'already has its own glass')); continue

    d = dyn_of(asset)
    MATS.enable_material_i_ds(d)
    if existing_id is not None:
        MATS.delete_triangles_by_material_id(d, existing_id)
    new_id = existing_id if existing_id is not None else len(slots)

    # MEASURED WITH ANY PREVIOUS LENS ALREADY GONE. Reading the ASSET's bounds measures the scope
    # PLUS the lens added last time, so every re-run pushed the new one further out than the last --
    # a lens that started at the eyepiece walks off the end of the barrel in three runs.
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(d)
    box = box[0] if isinstance(box, tuple) else box
    lo, hi = box.min, box.max
    o_ = unreal.Vector((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, (lo.z + hi.z) * 0.5)
    e = unreal.Vector((hi.x - lo.x) * 0.5, (hi.y - lo.y) * 0.5, (hi.z - lo.z) * 0.5)
    dims = [('X', e.x), ('Y', e.y), ('Z', e.z)]
    axis, half = max(dims, key=lambda dd: dd[1])
    cross = sorted(v for a, v in dims if a != axis)
    radius = max(0.35, cross[0] * 0.82)          # inside the tube, not spilling past its wall

    # Which end faces the shooter, once the catalogue rotation is applied.
    rot = o.get('rot') or [0, 0, 0]
    # NAMED, NOT POSITIONAL. unreal.Rotator(a, b, c) fills (roll, pitch, yaw) -- not the (pitch,
    # yaw, roll) the catalogue stores and C++ FRotator uses.
    R = unreal.Rotator(pitch=float(rot[0]), yaw=float(rot[1]), roll=float(rot[2]))
    along = {'X': unreal.Vector(1, 0, 0), 'Y': unreal.Vector(0, 1, 0), 'Z': unreal.Vector(0, 0, 1)}[axis]
    turned = unreal.MathLibrary.quat_rotate_vector(R.quaternion(), along)
    # Weapon +X is the direction of aim, so the eyepiece is whichever end goes to -X.
    sign = -1.0 if turned.x > 0 else 1.0
    centre = unreal.Vector(o_.x, o_.y, o_.z)
    if axis == 'X': centre.x += sign * (half * 0.97)
    elif axis == 'Y': centre.y += sign * (half * 0.97)
    else: centre.z += sign * (half * 0.97)
    popts = unreal.GeometryScriptPrimitiveOptions()
    popts.material_id = new_id
    # The disc's own +Z is its normal; turn that to lie along the optic's long axis -- so you look
    # THROUGH the glass rather than along its edge. Named fields again: unreal.Rotator(a, b, c) fills
    # (roll, pitch, yaw), and the first version of this passed positionally, which gave the X case a
    # roll and the Y case a yaw. A yaw does not move +Z at all, so every lens ended up lying flat,
    # face up, invisible from behind the weapon. Measured mapping: pitch -90 sends +Z to +X, roll +90
    # sends +Z to +Y.
    face = {'X': unreal.Rotator(pitch=-90.0, yaw=0.0, roll=0.0),
            'Y': unreal.Rotator(pitch=0.0, yaw=0.0, roll=90.0),
            'Z': unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0)}[axis]
    PRIM.append_disc(d, popts, unreal.Transform(centre, face, unreal.Vector(1, 1, 1)),
                     radius, 24, 1, 0.0, 360.0)

    to = unreal.GeometryScriptCopyMeshToAssetOptions()
    to.enable_recompute_normals = False
    to.enable_recompute_tangents = True
    AU.copy_mesh_to_static_mesh(d, asset, to, unreal.GeometryScriptMeshWriteLOD())

    mi = reticle_instance(key, centre, radius)
    rebuilt = [s for s in asset.get_editor_property('static_materials')]
    while len(rebuilt) <= new_id:
        rebuilt.append(unreal.StaticMaterial())
    rebuilt[new_id].material_interface = mi
    rebuilt[new_id].material_slot_name = 'Reticle'
    asset.modify()
    asset.set_editor_property('static_materials', rebuilt)
    unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outer(), mi.get_outer()], False)

    # And line the eye up with the new glass, in WEAPON space (eye is added after the rotation).
    if not o.get('eye') or all(abs(float(v)) < 0.001 for v in o.get('eye')):
        turned_centre = unreal.MathLibrary.quat_rotate_vector(R.quaternion(), centre)
        o['eye'] = [round(turned_centre.x, 3), round(turned_centre.y, 3), round(turned_centre.z, 3)]
    done.append((key, axis, round(radius, 2), [round(centre.x, 2), round(centre.y, 2), round(centre.z, 2)]))

io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(cat, indent=1) + '\n')
print('reticles added: %d' % len(done))
for k, ax, r, c in done:
    print('   %-26s long axis %s  lens r %.2f at %s' % (k, ax, r, c))
for k, why in skipped:
    print('   skipped %-24s %s' % (k, why))
