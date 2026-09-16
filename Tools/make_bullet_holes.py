# Bullet-hole decals: the three RawArt textures imported and one deferred-decal material each,
# the texture's colour over the wall and its alpha as the shape. Run in the editor through the
# remote-exec tool from the project root, after Tools/make_bullet_hole_textures.ps1.
import unreal, os
RAW = 'C:/Dev/Games/RepliCan/RawArt'; TEXDIR = '/Game/RepliCan/Textures'; MATDIR = '/Game/RepliCan/Materials'
tools = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
tasks = []
for i in (1, 2, 3):
    t = unreal.AssetImportTask(); t.filename = RAW + '/T_BulletHole_%02d.png' % i; t.destination_path = TEXDIR; t.destination_name = 'T_BulletHole_%02d' % i
    t.replace_existing = True; t.automated = True; t.save = True; tasks.append(t)
tools.import_asset_tasks(tasks)
for i in (1, 2, 3):
    tex = unreal.load_asset(TEXDIR + '/T_BulletHole_%02d' % i)
    if not tex: print('missing texture', i); continue
    name = 'M_BulletHole_%02d' % i; full = MATDIR + '/' + name
    m = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset(name, MATDIR, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(m)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    ts = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0); ts.set_editor_property('texture', tex)
    MEL.connect_material_property(ts, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(ts, 'A', unreal.MaterialProperty.MP_OPACITY)
    r = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -500, 250); r.set_editor_property('r', 0.8)
    MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m); unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)
    print(name, 'saved:', os.path.exists(os.path.join(unreal.Paths.project_content_dir(), 'RepliCan', 'Materials', name + '.uasset')))
