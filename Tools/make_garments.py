# FOLDED GARMENTS: clothing that exists in the world. Four small meshes in the packs' own blocky
# manner (a folded body, the front folded over, a rolled collar and tucked sleeves for a jacket; a
# folded pair of trousers with its waistband on top), each wearing a flat cloth material in its
# own colour, and a catalogue entry for each that says what the body puts on when it is worn:
# the junkers' torso, arms and legs from the cut library, by sex. Run in the editor through the
# remote-exec tool from the project root. Re-runnable: meshes and instances rebuilt, entries kept.
import unreal, io, json, os
PROPS = '/Game/RepliCan/Props'; MATDIR = '/Game/RepliCan/Materials'; CUT = '/Game/RepliCan/CutLibrary/SciFiSpace'
ITEMS = 'C:/Dev/Games/RepliCan/Content/GameData/UI/Items.json'; SHEET = 'C:/Dev/Games/RepliCan/Content/GameData/UI/CharacterSheet.json'
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if ues.get_game_world() is not None: raise RuntimeError('the editor is in Play')
tools = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
V = unreal.Vector; R = unreal.Rotator
def xf(x, y, z): return unreal.Transform(location=V(x, y, z), rotation=R(roll=0.0, pitch=0.0, yaw=0.0), scale=V(1, 1, 1))

# ---- the cloth: one material, a colour parameter, an instance per garment
full = MATDIR + '/M_Garment'
mat = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset('M_Garment', MATDIR, unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(mat)
col = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
col.set_editor_property('parameter_name', 'Colour'); col.set_editor_property('default_value', unreal.LinearColor(0.3, 0.3, 0.25, 1.0))
MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 250); rough.set_editor_property('r', 0.88)
MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(mat); unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)

def instance(name, colour):
    p = MATDIR + '/' + name
    mi = unreal.load_asset(p) if eal.does_asset_exist(p) else tools.create_asset(name, MATDIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', mat)
    MEL.set_material_instance_vector_parameter_value(mi, 'Colour', unreal.LinearColor(*colour))
    MEL.update_material_instance(mi)
    unreal.EditorLoadingAndSavingUtils.save_packages([mi.get_outermost()], False)
    return mi

def garment(name, kind, mi):
    P = unreal.GeometryScript_Primitives; opts = unreal.GeometryScriptPrimitiveOptions()
    C = unreal.GeometryScriptPrimitiveOriginMode.CENTER
    dyn = unreal.DynamicMesh()
    if kind == 'jacket':
        P.append_box(dyn, opts, xf(0.0, 0.0, 2.5), 30.0, 24.0, 5.0, 0, 0, 0, C)      # the folded body
        P.append_box(dyn, opts, xf(-2.0, 0.0, 6.6), 26.0, 20.0, 3.2, 0, 0, 0, C)    # the front folded over it
        P.append_box(dyn, opts, xf(-11.0, 0.0, 8.9), 6.0, 10.0, 1.4, 0, 0, 0, C)    # the collar, rolled along the back edge
        P.append_box(dyn, opts, xf(3.0, 11.5, 4.5), 16.0, 3.5, 3.0, 0, 0, 0, C)     # a sleeve tucked along each side
        P.append_box(dyn, opts, xf(3.0, -11.5, 4.5), 16.0, 3.5, 3.0, 0, 0, 0, C)
    else:
        P.append_box(dyn, opts, xf(0.0, 0.0, 2.5), 28.0, 20.0, 5.0, 0, 0, 0, C)      # the legs folded twice
        P.append_box(dyn, opts, xf(1.0, 0.0, 6.2), 24.0, 17.0, 2.4, 0, 0, 0, C)
        P.append_box(dyn, opts, xf(-10.0, 0.0, 8.0), 5.0, 17.0, 1.2, 0, 0, 0, C)    # the waistband on top
    unreal.GeometryScript_Normals.set_per_face_normals(dyn)
    full = PROPS + '/' + name
    asset = unreal.load_asset(full) if eal.does_asset_exist(full) else None
    if asset:
        to_opts = unreal.GeometryScriptCopyMeshToAssetOptions(); to_opts.enable_recompute_normals = True; to_opts.enable_recompute_tangents = True
        to_opts.replace_materials = True; to_opts.new_materials = [mi]
        unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dyn, asset, to_opts, unreal.GeometryScriptMeshWriteLOD())
    else:
        create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions(); create.enable_recompute_normals = True; create.enable_recompute_tangents = True
        asset, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dyn, full, create)
        if asset: asset.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=mi)])
    # a box to drop and to be picked out of a pile by
    try:
        sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        sms.remove_collisions(asset); sms.add_simple_collisions(asset, unreal.ScriptCollisionShapeType.BOX)
    except Exception as ex: print('collision:', ex)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    print('garment', full, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Props', name + '.uasset')))
    return full

SETS = [
    # key suffix, kind, sex, colour, name, description
    ('Junker_Jacket_M', 'jacket',   'Male',   (0.30, 0.32, 0.20), "Junker jacket (men's)",    "A salvager's jacket, folded the way it came off. Oil in the seams, a name inked out on the collar."),
    ('Junker_Pants_M',  'trousers', 'Male',   (0.22, 0.20, 0.17), "Junker trousers (men's)",  "Salvager's work trousers, knees gone shiny. Folded once, pockets emptied."),
    ('Junker_Jacket_F', 'jacket',   'Female', (0.45, 0.22, 0.14), "Junker jacket (women's)",  "A salvager's jacket, rust red where it was not always rust red. Folded, not washed."),
    ('Junker_Pants_F',  'trousers', 'Female', (0.14, 0.22, 0.24), "Junker trousers (women's)", "Salvager's work trousers in a dark teal, the cuffs rolled once and left that way."),
]
meshes = {}
for key, kind, sex, colour, name, desc in SETS:
    mi = instance('MI_Garment_' + key, colour)
    meshes[key] = garment('SM_Garment_' + key, kind, mi)

# ---- the catalogue: what each garment is, and what the body wears with it
doc = json.load(io.open(ITEMS, encoding='utf-8'))
items = doc['items']
added = 0
# One entry per GARMENT, not per body (2026-09-17): the jacket is "Junker jacket" whoever wears
# it, and its entry carries the part for each body; the wearer's sex picks (ABasePlayerController::
# RefreshWornClothing). The two folded meshes stay as world props, either of which is the item.
for gkey, kind, name, desc in (('Junker_Jacket', 'jacket', 'Junker jacket', "A salvager's jacket, folded the way it came off. Cut to whoever puts it on."),
                               ('Junker_Pants', 'pants', 'Junker trousers', "Salvager's work trousers, folded once. Cut to whoever puts them on.")):
    k = 'Clothing/' + gkey
    if k in items: continue
    e = {
        'name': name, 'category': 'armor', 'pack': 'Clothing', 'mesh': meshes[gkey + '_M'], 'icon': 'Clothing_' + gkey, 'description': desc,
        'size': [30.0, 24.0, 10.0] if kind == 'jacket' else [28.0, 20.0, 8.6], 'keep': True, 'handmade': True,
        'kind': 'Jacket' if kind == 'jacket' else 'Pants', 'equip_slot': 'Chest' if kind == 'jacket' else 'Legs', 'use': 'equip',
        'stack_max': 1, 'bulk': 1, 'value': 45 if kind == 'jacket' else 30, 'rarity': 'common', 'droppable': True, 'sellable': True, 'quest': False, 'unique': False,
        'mass_kg': 0.9 if kind == 'jacket' else 0.7, 'material': 'cloth', 'physics_on_drop': True, 'bounce': 0.05, 'hidden': False,
    }
    for sex in ('Male', 'Female'):
        src = 'Junker_%s_01' % sex
        if kind == 'jacket': e['wear_torso_' + sex.lower()] = '%s/%s_Torso.%s_Torso' % (CUT, src, src); e['wear_arms_' + sex.lower()] = '%s/%s_Arms.%s_Arms' % (CUT, src, src)
        else: e['wear_legs_' + sex.lower()] = '%s/%s_Legs.%s_Legs' % (CUT, src, src)
    items[k] = e; added += 1
doc['items'] = dict(sorted(items.items()))
io.open(ITEMS, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
print('items: +%d garments' % added)

# ---- the sheet: a jacket goes on the chest square, trousers on the legs square
sheet = json.load(io.open(SHEET, encoding='utf-8'))
changed = False
for s in sheet['gear']['slots']:
    want = {'Chest': ['Jacket', 'Shirt'], 'Legs': ['Pants', 'Trousers']}.get(s['name'], [])
    for w in want:
        if w not in s['kinds']: s['kinds'].append(w); changed = True
if changed:
    io.open(SHEET, 'w', encoding='utf-8', newline='\n').write(json.dumps(sheet, indent=1, ensure_ascii=False) + '\n'); print('sheet: jacket and trousers kinds')
print('garments ready')
