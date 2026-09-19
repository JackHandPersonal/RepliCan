# The keep-out art and the laser's scorch, imported and wrapped: T_Sign_OutOfOrder -> M_Sign_OutOfOrder
# (a masked plane material like the door numbers), T_HazardTape -> M_HazardTape (opaque, on the
# tape strips), T_Scorch_Soft -> M_LaserScorch (a deferred decal: near-black, with an emissive that
# a "Heat" parameter drives from white-hot to nothing; BasePlayerController::TickScorches cools it).
# Run in the editor through the remote-exec tool from the project root, after Tools/make_keepout.ps1.
# Re-runnable: textures replaced, materials rebuilt.
import unreal, os
RAW = 'C:/Dev/Games/RepliCan/RawArt'; TEXDIR = '/Game/RepliCan/Textures'; MATDIR = '/Game/RepliCan/Materials'
tools = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
tasks = []
for nm in ('T_Sign_OutOfOrder', 'T_HazardTape', 'T_Scorch_Soft'):
    t = unreal.AssetImportTask(); t.filename = RAW + '/' + nm + '.png'; t.destination_path = TEXDIR; t.destination_name = nm
    t.replace_existing = True; t.automated = True; t.save = True; tasks.append(t)
tools.import_asset_tasks(tasks)

def material(name):
    full = MATDIR + '/' + name
    m = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset(name, MATDIR, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m)
    return m
def finish(m, name):
    MEL.recompile_material(m); unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)
    print(name, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', name + '.uasset')))
def const(m, v, x, y):
    k = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, x, y); k.set_editor_property('r', v); return k

# the sign: colour from the texture, a touch of emissive so it reads in a dim hall, masked by alpha
tex = unreal.load_asset(TEXDIR + '/T_Sign_OutOfOrder')
m = material('M_Sign_OutOfOrder')
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0); ts.set_editor_property('texture', tex)
MEL.connect_material_property(ts, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(ts, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
MEL.connect_material_property(const(m, 0.85, -500, 300), '', unreal.MaterialProperty.MP_ROUGHNESS)
mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, 180)
MEL.connect_material_expressions(ts, 'RGB', mul, 'A'); MEL.connect_material_expressions(const(m, 0.12, -500, 220), '', mul, 'B')
MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(m, 'M_Sign_OutOfOrder')

# the tape: plain and a little glossy
tex = unreal.load_asset(TEXDIR + '/T_HazardTape')
m = material('M_HazardTape')
ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0); ts.set_editor_property('texture', tex)
MEL.connect_material_property(ts, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(const(m, 0.55, -500, 300), '', unreal.MaterialProperty.MP_ROUGHNESS)
finish(m, 'M_HazardTape')

# the scorch: a decal. Base colour near black inside the mask; emissive = the hot colour x Heat x mask.
tex = unreal.load_asset(TEXDIR + '/T_Scorch_Soft')
m = material('M_LaserScorch')
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -700, 0); ts.set_editor_property('texture', tex)
dark = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -700, -200); dark.set_editor_property('constant', unreal.LinearColor(0.02, 0.016, 0.012, 1.0))
MEL.connect_material_property(dark, '', unreal.MaterialProperty.MP_BASE_COLOR)
op = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 120)
MEL.connect_material_expressions(ts, 'A', op, 'A'); MEL.connect_material_expressions(const(m, 0.92, -700, 200), '', op, 'B')
MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
MEL.connect_material_property(const(m, 0.9, -700, 320), '', unreal.MaterialProperty.MP_ROUGHNESS)
heat = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 420); heat.set_editor_property('parameter_name', 'Heat'); heat.set_editor_property('default_value', 1.0)
hot = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -700, 540); hot.set_editor_property('constant', unreal.LinearColor(9.0, 2.6, 0.5, 1.0))   # white-orange, well over one: it glows
m1 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -400, 460)
MEL.connect_material_expressions(hot, '', m1, 'A'); MEL.connect_material_expressions(heat, '', m1, 'B')
m2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 460)
MEL.connect_material_expressions(m1, '', m2, 'A'); MEL.connect_material_expressions(ts, 'A', m2, 'B')
MEL.connect_material_property(m2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(m, 'M_LaserScorch')
print('keep-out art and the scorch ready')
