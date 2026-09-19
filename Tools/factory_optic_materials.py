"""Put each factory optic back on the material of the weapon it was cut from.

A factory scope is geometry carved off its weapon, so it should wear the weapon's paint and offer the
weapon's colourways. It did not: the parts were baked with MI_PolygonScifiWorlds_01_A while the
weapons wear M_PolygonSciFiWorlds_01_A. Those differ by a prefix and one capital letter, so they are
unrelated assets in unrelated families -- and a paint list is gathered per family. The Spear 350
offered thirteen finishes; the scope cut off the Spear 350 offered two, both yellow.

Two traps, both hit on the first attempt at this:

  * MATCH BY ASSET, NOT BY KEY. The optic key is "Factory_" plus the tail of the weapon key, and
    several packs have a Wep_Pistol_01, so four different weapons mapped onto one optic and each
    overwrote the last. The weapon that owns an optic is the one whose carved scope IS that optic's
    mesh, so that is what is matched here.
  * UNREAL ASSET LOOKUP IS CASE-INSENSITIVE. Naming the new two-sided child after its parent gave
    MI_PolygonSciFiWorlds_01_A_TwoSided, which load_asset happily resolved to the EXISTING
    MI_PolygonScifiWorlds_01_A_TwoSided -- the wrong family, silently, which is the very bug being
    fixed. The name carries a checksum of the parent's full path so two materials that differ only
    in case cannot share a child.

  Tools/ue_remote --file Tools/factory_optic_materials
"""
import unreal, json, io, os, zlib

CAT = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
OUT_DIR = '/Game/RepliCan/Optics'
_twins = {}


def two_sided_twin(mat):
    path = mat.get_path_name()
    if path in _twins:
        return _twins[path]
    tag = format(zlib.crc32(path.encode('utf-8')) & 0xFFFF, '04x')
    name = 'MI_TS_%s_%s' % (mat.get_name().replace('MI_', '').replace('M_', ''), tag)
    full = OUT_DIR + '/' + name
    mi = unreal.load_asset(full)
    if not mi:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, OUT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mi, mat)
    ov = mi.get_editor_property('base_property_overrides')
    ov.set_editor_property('override_two_sided', True)
    ov.set_editor_property('two_sided', True)
    mi.set_editor_property('base_property_overrides', ov)
    unreal.EditorLoadingAndSavingUtils.save_packages([mi.get_outer()], False)
    _twins[path] = mi
    return mi


def first_material(path):
    a = unreal.load_asset((path or '').split('.')[0])
    if not a:
        return None
    for s in a.get_editor_property('static_materials'):
        if s.material_interface:
            return s.material_interface
    return None


def base_name(p):
    return (p or '').split('.')[0].split('/')[-1]


cat = json.load(io.open(CAT, encoding='utf-8'))
weapons, optics = cat.get('weapons', {}), cat.get('optics', {})

# The weapon that owns each factory optic: the one whose carved scope became that optic's mesh.
owner = {}
for wkey, w in weapons.items():
    scope = (w.get('parts') or {}).get('scope')
    if not scope:
        continue
    stem = base_name(scope).replace('_Scope_01', '')
    for okey, o in optics.items():
        if not okey.startswith('Factory_'):
            continue
        if base_name(o.get('mesh')).replace('_Optic_01', '') == stem:
            owner[okey] = wkey

done, skipped = [], []
for okey, wkey in sorted(owner.items()):
    o, w = optics[okey], weapons[wkey]
    src = first_material(w.get('body_mesh') or w.get('mesh'))
    asset = unreal.load_asset((o.get('mesh') or '').split('.')[0])
    if not src or not asset:
        skipped.append((okey, 'no material or no mesh'))
        continue
    twin = two_sided_twin(src)
    slots = [s for s in asset.get_editor_property('static_materials')]
    if not slots:
        skipped.append((okey, 'no slots'))
        continue
    was = slots[0].material_interface.get_path_name() if slots[0].material_interface else None
    if was == twin.get_path_name():
        skipped.append((okey, 'already on the weapon material'))
        continue
    rebuilt = []
    for i, s in enumerate(slots):
        if i == 0:
            s.material_interface = twin      # slot 0 is the optic body; Reticle is left alone
        rebuilt.append(s)
    asset.modify()
    asset.set_editor_property('static_materials', rebuilt)
    unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outer()], False)
    done.append((okey, wkey, base_name(was), twin.get_name(), src.get_name()))

print('factory optics put back on their weapon material: %d' % len(done))
for okey, wkey, was, now, src in done:
    print('   %-24s from %-24s  %s -> %s  (parent %s)' % (okey, wkey.split('/')[-1], was, now, src))
for k, why in skipped:
    print('   skipped %-22s %s' % (k, why))
