# M_LaserBeam: the beam a laser weapon draws while its trigger is held (see TickLaser). Unlit,
# additive, emissive: a hot red core that reads through anything behind it. And M_TracerStreak
# (2026-09-17): the round's streak, a dark grey unlit TRANSLUCENT bar -- additive cannot draw dark --
# with the same Colour parameter and an Opacity one, faded at the cylinder's edges the same way. Run in the editor
# through the remote-exec tool, from the project root. (No line of this header may end in the
# script extension: the editor would take it for a file path.)
import unreal, os
NAME, PKG = 'M_LaserBeam', '/Game/RepliCan/Materials'
full = PKG + '/' + NAME
MEL = unreal.MaterialEditingLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else tools.create_asset(NAME, PKG, unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
col = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
col.set_editor_property('parameter_name', 'Colour'); col.set_editor_property('default_value', unreal.LinearColor(1.0, 0.16, 0.08, 1.0))
heat = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 180)
heat.set_editor_property('parameter_name', 'Heat'); heat.set_editor_property('default_value', 14.0)
mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 60)
MEL.connect_material_expressions(col, '', mul, 'A'); MEL.connect_material_expressions(heat, '', mul, 'B')
MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
# the edges of the cylinder fade, so the beam reads as a glowing line rather than a tube
fres = MEL.create_material_expression(mat, unreal.MaterialExpressionFresnel, -500, 340)
fres.set_editor_property('exponent', 1.5)
om = MEL.create_material_expression(mat, unreal.MaterialExpressionOneMinus, -300, 340)
MEL.connect_material_expressions(fres, '', om, '')
MEL.connect_material_property(om, '', unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(mat)
unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
p = os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', NAME + '.uasset')
print('M_LaserBeam saved:', os.path.exists(p), p)

# ---- the tracer's streak: dark, so translucent rather than additive
NAME2 = 'M_TracerStreak'; full2 = PKG + '/' + NAME2
m2 = unreal.load_asset(full2) if unreal.EditorAssetLibrary.does_asset_exist(full2) else tools.create_asset(NAME2, PKG, unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(m2)
m2.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m2.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m2.set_editor_property('two_sided', True)
c2 = MEL.create_material_expression(m2, unreal.MaterialExpressionVectorParameter, -500, 0)
c2.set_editor_property('parameter_name', 'Colour'); c2.set_editor_property('default_value', unreal.LinearColor(0.06, 0.06, 0.06, 1.0))
MEL.connect_material_property(c2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
op = MEL.create_material_expression(m2, unreal.MaterialExpressionScalarParameter, -500, 180)
op.set_editor_property('parameter_name', 'Opacity'); op.set_editor_property('default_value', 0.8)
f2 = MEL.create_material_expression(m2, unreal.MaterialExpressionFresnel, -500, 340); f2.set_editor_property('exponent', 1.5)
o2 = MEL.create_material_expression(m2, unreal.MaterialExpressionOneMinus, -300, 340)
MEL.connect_material_expressions(f2, '', o2, '')
mo = MEL.create_material_expression(m2, unreal.MaterialExpressionMultiply, -150, 260)
MEL.connect_material_expressions(op, '', mo, 'A'); MEL.connect_material_expressions(o2, '', mo, 'B')
MEL.connect_material_property(mo, '', unreal.MaterialProperty.MP_OPACITY)
MEL.recompile_material(m2)
unreal.EditorLoadingAndSavingUtils.save_packages([m2.get_outermost()], False)
print('M_TracerStreak saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', NAME2 + '.uasset')))
