"""Make optic housings solid from behind.

A Synty optic body uses the pack's shared atlas material, which is SINGLE-SIDED. That is fine for a
prop seen from outside, and wrong for the one prop the player deliberately puts their eye behind: at
ADS you are inside the shroud, the faces around you point away, they are culled, and you look
straight through the body of the sight. Measured on SM_Optic_RedDot_02: slot 0
(MI_PolygonScifiWorlds_01_A -> M_PolygonScifiWorlds_Base_01) two_sided false, while the glass on
slot 1 (M_RedDot) was already true.

The shared atlas instance is NOT edited -- half the level uses it, and making the whole pack
two-sided to fix one prop is a bad trade. Instead each parent gets one child instance that overrides
only bTwoSided, and the optic meshes point their non-glass slots at it.

  Tools/ue_remote --file Tools/optic_two_sided
"""
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
REG = unreal.AssetRegistryHelpers.get_asset_registry()
HOME = '/Game/RepliCan/Optics'
FOLDERS = ['/Game/RepliCan/Optics', '/Game/RepliCan/Weapons/Optics']

_cache = {}


def two_sided_twin(mat):
    """A child of `mat` that is two-sided, made once and reused."""
    key = mat.get_name()
    if key in _cache:
        return _cache[key]
    name = 'MI_%s_TwoSided' % key.replace('MI_', '').replace('M_', '')
    path = '%s/%s' % (HOME, name)
    mic = unreal.load_asset(path)
    if not mic:
        mic = TOOLS.create_asset(name, HOME, unreal.MaterialInstanceConstant,
                                 unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mic, mat)
    ov = mic.get_editor_property('base_property_overrides')
    ov.set_editor_property('override_two_sided', True)
    ov.set_editor_property('two_sided', True)
    mic.set_editor_property('base_property_overrides', ov)
    _cache[key] = mic
    return mic


def base_of(mi):
    """Walk an instance chain to the material that actually carries bTwoSided."""
    seen = 0
    while isinstance(mi, unreal.MaterialInstance) and seen < 8:
        ov = mi.get_editor_property('base_property_overrides')
        if ov and ov.get_editor_property('override_two_sided'):
            return mi, bool(ov.get_editor_property('two_sided'))
        mi = mi.get_editor_property('parent')
        seen += 1
    return mi, (bool(mi.get_editor_property('two_sided')) if mi else True)


def optic_meshes():
    """Every mesh the CATALOGUE calls an optic, plus whatever sits in the optic folders.

    Following the catalogue matters: the factory scopes promoted out of the weapons live in
    Weapons/Worlds/Parts among the magazines and triggers, so a folder sweep would either miss them
    or make every unrelated weapon part two-sided. A scope is an optic because the catalogue says
    so, not because of where it is filed."""
    seen, out = set(), []
    def take(a):
        if isinstance(a, unreal.StaticMesh) and a.get_path_name() not in seen:
            seen.add(a.get_path_name())
            out.append(a)
    try:
        import json, io as _io, os
        cat = os.path.join(unreal.Paths.project_dir(), 'UI', 'Weapons.json')
        for o in json.load(_io.open(cat, encoding='utf-8')).get('optics', {}).values():
            path = (o.get('mesh') or '').split('.')[0]
            if path:
                take(unreal.load_asset(path))
    except Exception as ex:
        print('   could not read the catalogue (%s); falling back to the folders' % ex)
    for folder in FOLDERS:
        for data in REG.get_assets_by_path(folder, recursive=True):
            take(data.get_asset())
    return out


changed = []
for mesh in optic_meshes():
    slots = mesh.get_editor_property('static_materials')
    # INDEXING AN ARRAY OF STRUCTS HANDS BACK A COPY. Mutating the loop variable edits a temporary
    # and the untouched array goes straight back in -- the asset then re-serialises, the file's
    # timestamp moves, and the change is nowhere in it. That is what happened the first time this
    # ran: every line printed, the package saved, and the editor came back single-sided. Collect the
    # copies and hand the whole rebuilt list over instead.
    rebuilt, touched = [], False
    for i, s in enumerate(slots):
        mi = s.material_interface
        if mi and 'TwoSided' not in mi.get_name():
            _, is_two = base_of(mi)
            if not is_two:      # the glass is already two-sided; leave it alone
                s.material_interface = two_sided_twin(mi)
                touched = True
                print('   %-30s slot %d  %s -> two-sided' % (mesh.get_name(), i, mi.get_name()))
        rebuilt.append(s)
    if touched:
        mesh.modify()
        mesh.set_editor_property('static_materials', rebuilt)
        changed.append(mesh)

pkgs = [m.get_outer() for m in changed] + [m.get_outer() for m in _cache.values()]
if pkgs:
    unreal.EditorLoadingAndSavingUtils.save_packages(pkgs, False)

# AND CHECK IT TOOK. Re-read from the object after saving; the restart is the real proof, but a
# slot that still names the single-sided material here has certainly not been written.
bad = 0
for mesh in changed:
    for i, s in enumerate(mesh.get_editor_property('static_materials')):
        mi = s.material_interface
        if mi and 'TwoSided' not in mi.get_name() and not base_of(mi)[1]:
            print('   STILL SINGLE-SIDED: %s slot %d = %s' % (mesh.get_name(), i, mi.get_name()))
            bad += 1
print('optics made solid from behind: %d mesh(es), %d new material(s), %d slot(s) still wrong'
      % (len(changed), len(_cache), bad))
