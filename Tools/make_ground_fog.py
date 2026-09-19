# M_GroundFog: the cheap ground fog. ONE translucent plane over the service deck's floor (the
# layout tool places S10_FogPlane) wearing two copies of a tileable noise, panned against each
# other and multiplied, faded where it meets a prop or a leg (depth fade), unlit, a cold grey-blue
# barely there (2026-09-17 evening: thinner a third time, 0.10, with a 420 fade, on the user's
# word that the basement is still too thick -- the two planes it feeds also sit higher now, so it reads as haze
# in the air rather than as a lid on the floor. Was 0.16 with a 300 fade, and 0.2 before that. Softer against
# what stands in it; the plane itself sits higher in the layout). One quad of overdraw where the particle fog was six sheets of
# overlapping sprites. Run in the editor through the remote-exec tool from the project root, after
# the fog texture script (make_fog_texture, PowerShell). No line here may hold a script name with
# its extension: the remote tool would take it for a path.
import unreal, os
RAW = 'C:/Dev/Games/RepliCan/RawArt'; TEXDIR = '/Game/RepliCan/Textures'; MATDIR = '/Game/RepliCan/Materials'
tools = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
t = unreal.AssetImportTask(); t.filename = RAW + '/T_FogNoise.png'; t.destination_path = TEXDIR; t.destination_name = 'T_FogNoise'
t.replace_existing = True; t.automated = True; t.save = True
tools.import_asset_tasks([t])
tex = unreal.load_asset(TEXDIR + '/T_FogNoise')
name = 'M_GroundFog'; full = MATDIR + '/' + name
m = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset(name, MATDIR, unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)
def node(cls, x, y): return MEL.create_material_expression(m, cls, x, y)
def const(v, x, y): k = node(unreal.MaterialExpressionConstant, x, y); k.set_editor_property('r', v); return k
def layer(tu, tv, sx, sy, y):
    uv = node(unreal.MaterialExpressionTextureCoordinate, -1100, y); uv.set_editor_property('u_tiling', tu); uv.set_editor_property('v_tiling', tv)
    pan = node(unreal.MaterialExpressionPanner, -900, y); pan.set_editor_property('speed_x', sx); pan.set_editor_property('speed_y', sy)
    MEL.connect_material_expressions(uv, '', pan, 'Coordinate')
    ts = node(unreal.MaterialExpressionTextureSample, -700, y); ts.set_editor_property('texture', tex)
    MEL.connect_material_expressions(pan, '', ts, 'UVs')
    return ts
a = layer(6.0, 12.0, 0.011, 0.006, 0)
b = layer(9.0, 18.0, -0.007, 0.012, 260)
mul = node(unreal.MaterialExpressionMultiply, -480, 120)
MEL.connect_material_expressions(a, 'R', mul, 'A'); MEL.connect_material_expressions(b, 'R', mul, 'B')
gain = node(unreal.MaterialExpressionMultiply, -320, 120)
MEL.connect_material_expressions(mul, '', gain, 'A'); MEL.connect_material_expressions(const(0.10, -480, 300), '', gain, 'B')
fade = node(unreal.MaterialExpressionDepthFade, -150, 120); fade.set_editor_property('fade_distance_default', 420.0)
MEL.connect_material_expressions(gain, '', fade, 'InOpacity')
MEL.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)
col = node(unreal.MaterialExpressionConstant3Vector, -480, 480); col.set_editor_property('constant', unreal.LinearColor(0.16, 0.2, 0.25, 1.0))
MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m); unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)
print(name, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', name + '.uasset')))

# ---- the second sheet (2026-09-17): the same material, thinner and slower, for a plane at waist height.
name2 = 'M_GroundFog_High'; full2 = MATDIR + '/' + name2
m2 = unreal.load_asset(full2) if eal.does_asset_exist(full2) else tools.create_asset(name2, MATDIR, unreal.Material, unreal.MaterialFactoryNew())
MEL.delete_all_material_expressions(m2)
m2.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
m2.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m2.set_editor_property('two_sided', True)
m = m2
a = layer(4.0, 8.0, 0.006, 0.003, 0)
b = layer(7.0, 14.0, -0.004, 0.007, 260)
mul = node(unreal.MaterialExpressionMultiply, -480, 120)
MEL.connect_material_expressions(a, 'R', mul, 'A'); MEL.connect_material_expressions(b, 'R', mul, 'B')
gain = node(unreal.MaterialExpressionMultiply, -320, 120)
MEL.connect_material_expressions(mul, '', gain, 'A'); MEL.connect_material_expressions(const(0.030, -480, 300), '', gain, 'B')
fade = node(unreal.MaterialExpressionDepthFade, -150, 120); fade.set_editor_property('fade_distance_default', 460.0)
MEL.connect_material_expressions(gain, '', fade, 'InOpacity')
MEL.connect_material_property(fade, '', unreal.MaterialProperty.MP_OPACITY)
col = node(unreal.MaterialExpressionConstant3Vector, -480, 480); col.set_editor_property('constant', unreal.LinearColor(0.18, 0.22, 0.28, 1.0))
MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m2); unreal.EditorLoadingAndSavingUtils.save_packages([m2.get_outermost()], False)
print(name2, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', name2 + '.uasset')))
