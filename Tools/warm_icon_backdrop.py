"""Warms the icon backdrop's shader before Tools/render_weapon_icons.py runs.

    python Tools/ue_remote.py --file Tools/warm_icon_backdrop.py          # park the plane
    (wait ~30 s with the editor idle)
    python Tools/ue_remote.py --file Tools/render_weapon_icons.py
    python Tools/ue_remote.py --file Tools/import_icons.py
    (set REMOVE = True below and run it again to take the plane away)

A material's shader compiles asynchronously and the results land on the game thread between
ticks; a remote script holds that thread, so a material first used inside the render batch
draws as the engine's grey checker for the whole batch. Parking one plane wearing the material
in the level and letting the editor tick finishes the compile; the batch then draws it black.
"""
import unreal
REMOVE = False
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if ues.get_game_world() is not None:
    raise RuntimeError('the editor is in Play')
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if a.get_actor_label() == 'IconBackdropWarm':
        eas.destroy_actor(a); print('warm plane removed')
if not REMOVE:
    MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary; tools = unreal.AssetToolsHelpers.get_asset_tools()
    def flat_unlit(name, v):
        path = '/Game/RepliCan/Materials/' + name
        found = unreal.load_asset(path)
        if found: return found
        m = tools.create_asset(name, '/Game/RepliCan/Materials', unreal.Material, unreal.MaterialFactoryNew())
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
        c.set_editor_property('constant', unreal.LinearColor(v, v, v, 1.0))
        MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        MEL.recompile_material(m); eal.save_asset(path)
        return m
    for k, (name, v) in enumerate((('M_IconBackBlack', 0.0), ('M_IconBackWhite', 1.0))):
        m = flat_unlit(name, v)
        p = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(300.0 * k, -90000, 30000))
        p.set_actor_label('IconBackdropWarm'); p.static_mesh_component.set_material(0, m)
    print('warm planes placed (black, white): wait half a minute, then render')
