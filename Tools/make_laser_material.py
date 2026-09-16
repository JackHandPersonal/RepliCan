# M_LaserBeam: the beam a laser weapon draws while its trigger is held (see TickLaser). Unlit,
# additive, emissive: a hot red core that reads through anything behind it. Run in the editor
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
