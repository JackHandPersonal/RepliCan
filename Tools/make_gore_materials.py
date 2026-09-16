# Gore materials: M_Bone for the stump a severed limb leaves (an off-white, slightly rough), and
# M_BloodPool_01..03, floor decals cut from the Cyber City blood stain textures, dark and wet.
# Run in the editor through the remote-exec tool from the project root. (No header line may end
# in the script extension: the editor would take it for a file path.)
import unreal, os
MEL = unreal.MaterialEditingLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
PKG = '/Game/RepliCan/Materials'
def material(name):
    full = PKG + '/' + name
    m = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset(name, PKG, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m); return m
def save(m):
    MEL.recompile_material(m); unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)
    p = os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', m.get_name() + '.uasset'); print(m.get_name(), 'saved:', os.path.exists(p))
# ---- the bone (made once; left alone after)
if not eal.does_asset_exist(PKG + '/M_Bone'):
  m = material('M_Bone')
  c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0); c.set_editor_property('constant', unreal.LinearColor(0.86, 0.80, 0.66, 1.0))
  MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
  r = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 200); r.set_editor_property('r', 0.55)
  MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
  save(m)
# ---- the meat (made once): torn flesh, dark red and a little wet
if not eal.does_asset_exist(PKG + '/M_Meat'):
  m = material('M_Meat')
  c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0); c.set_editor_property('constant', unreal.LinearColor(0.40, 0.045, 0.035, 1.0))
  MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
  r = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 200); r.set_editor_property('r', 0.32)
  MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
  save(m)
# ---- the pool: the Cyber City stain's alpha is the shape (as M_BloodDecal uses it), faded to
# nothing at the decal's edge so a square never shows; dark and wet. One material; the pools
# differ by the yaw and size they are spawned at.
tex = unreal.load_asset('/Game/PolygonCyberCity/Textures/Misc/T_Stain_Blood_01')
m = material('M_BloodPool')
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
def node(cls, x, y): return MEL.create_material_expression(m, cls, x, y)
ts = node(unreal.MaterialExpressionTextureSample, -900, 0); ts.set_editor_property('texture', tex)
tint = node(unreal.MaterialExpressionConstant3Vector, -900, 250); tint.set_editor_property('constant', unreal.LinearColor(0.30, 0.008, 0.006, 1.0))
MEL.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = node(unreal.MaterialExpressionConstant, -900, 420); rough.set_editor_property('r', 0.12)
MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
uv = node(unreal.MaterialExpressionTextureCoordinate, -1100, 550)
centre = node(unreal.MaterialExpressionConstant2Vector, -1100, 670); centre.set_editor_property('r', 0.5); centre.set_editor_property('g', 0.5)
off = node(unreal.MaterialExpressionSubtract, -900, 600); MEL.connect_material_expressions(uv, '', off, 'A'); MEL.connect_material_expressions(centre, '', off, 'B')
dist = node(unreal.MaterialExpressionLength, -760, 600); MEL.connect_material_expressions(off, '', dist, '')
twice = node(unreal.MaterialExpressionMultiply, -620, 600); twice.set_editor_property('const_b', 2.2); MEL.connect_material_expressions(dist, '', twice, 'A')
inv = node(unreal.MaterialExpressionOneMinus, -480, 600); MEL.connect_material_expressions(twice, '', inv, '')
fade = node(unreal.MaterialExpressionSaturate, -360, 600); MEL.connect_material_expressions(inv, '', fade, '')
shaped = node(unreal.MaterialExpressionMultiply, -220, 400); MEL.connect_material_expressions(ts, 'A', shaped, 'A'); MEL.connect_material_expressions(fade, '', shaped, 'B')
strength = node(unreal.MaterialExpressionMultiply, -80, 400); strength.set_editor_property('const_b', 0.95); MEL.connect_material_expressions(shaped, '', strength, 'A')
MEL.connect_material_property(strength, '', unreal.MaterialProperty.MP_OPACITY)
save(m)
for i in (1, 2, 3):
    old = PKG + '/M_BloodPool_%02d' % i
    if eal.does_asset_exist(old): eal.delete_asset(old); print('removed', old)
