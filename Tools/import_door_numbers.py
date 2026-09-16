"""The cabin door numbers: eight stencil digits (RawArt/T_Door_Num_<n>.png, drawn with the
Stencil face by scratch door_digits.ps1; the _F twin is the same digit turned half round for a
plane whose roll puts its top at the floor) imported as textures and wrapped in the masked
sign material the PCS signs use. Re-runnable: textures replaced, materials kept.
    python Tools/ue_remote.py --file Tools/import_door_numbers.py
"""
import unreal
RAW = r'C:\Dev\Games\RepliCan\RawArt'
TEXDIR = '/Game/RepliCan/Textures'; MATDIR = '/Game/RepliCan/Materials'
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
NAMES = ['T_Door_Num_%d%s' % (n, s) for n in range(1, 9) for s in ('', '_F')]
tasks = []
for nm in NAMES:
    t = unreal.AssetImportTask()
    t.filename = RAW + '/' + nm + '.png'; t.destination_path = TEXDIR; t.destination_name = nm
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
def surface_material(mname, texname, masked, emissive):
    path = MATDIR + '/' + mname
    if unreal.EditorAssetLibrary.does_asset_exist(path): return unreal.load_asset(path)
    tex = unreal.load_asset(TEXDIR + '/' + texname)
    m = tools.create_asset(mname, MATDIR, unreal.Material, unreal.MaterialFactoryNew())
    if masked: m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0)
    ts.set_editor_property('texture', tex)
    MEL.connect_material_property(ts, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    if masked: MEL.connect_material_property(ts, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    rough = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 300); rough.set_editor_property('r', 0.85)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 180)
    k = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 220); k.set_editor_property('r', emissive)
    MEL.connect_material_expressions(ts, 'RGB', mul, 'A'); MEL.connect_material_expressions(k, '', mul, 'B')
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)   # save_asset says no to an asset this young
    print('created', mname)
    return m
for nm in NAMES:
    surface_material('M_Sign_' + nm[2:], nm, True, 0.2)
print('door number textures + materials ready:', len(NAMES))
