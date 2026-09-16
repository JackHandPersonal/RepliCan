"""Inventory icon renders for ItemCatalog items that are not weapons in UI/Weapons.json.

One shot per item in the editor world, far from the level, against an unlit pure-black backdrop
(orthographic, side-on with a little yaw for depth, three lights) -- the same rig and the same
conventions as Tools/render_weapon_icons.py, so the two sets look like one set. Black ground, no
alpha cutout, no matte pass: the panels these sit on are black anyway.
Tools/import_icons.py brings them in as /Game/RepliCan/Icons/T_Icon_<Name>. Item names map to
icon names by replacing every run of non-alphanumerics with '_' (the same rule
UInventoryGridWidget uses to look icons up).

    python Tools/ue_remote.py --file Tools/render_icons.py
    python Tools/ue_remote.py --file Tools/import_icons.py

Edit ITEMS: (item name, mesh path, yaw, pitch, roll). Output: RawArt/Icons/<Icon>.png
"""
import unreal, re
ITEMS = [
    ('Pocket pistol', '/Game/RepliCan/Weapons/Worlds/SM_Wep_Pistol_06', -20.0, 0.0, 0.0),   # the slim little Sci-Fi Worlds sidearm
]
SIZE = 1024
OUT = r'C:\Dev\Games\RepliCan\RawArt\Icons'
def icon_name(item): return 'T_Icon_' + re.sub(r'[^A-Za-z0-9]+', '_', item).strip('_')

import traceback
try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    MEL = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    Y0 = -60000.0

    # Debris from an interrupted run would be photographed alongside the item: clear it first.
    swept = 0
    for act in eas.get_all_level_actors():
        try:
            if abs(act.get_actor_location().y - Y0) < 4000.0:
                eas.destroy_actor(act); swept += 1
        except Exception:
            pass
    print('SWEPT', swept)

    # load_asset hands back None for a material that never compiled, and a null material draws
    # flat engine grey behind the item: build the backdrop whenever it does not load. On the run
    # that first creates it the plane still renders grey -- run the script again.
    def flat_unlit(name, v):
        path = '/Game/RepliCan/Materials/' + name
        found = unreal.load_asset(path)
        if found: return found
        if eal.does_asset_exist(path): eal.delete_asset(path)
        m = tools.create_asset(name, '/Game/RepliCan/Materials', unreal.Material, unreal.MaterialFactoryNew())
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        c = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
        c.set_editor_property('constant', unreal.LinearColor(v, v, v, 1.0))
        MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        MEL.recompile_material(m); eal.save_asset(path); return m
    black = flat_unlit('M_IconBackBlack', 0.0)
    import os; os.makedirs(OUT, exist_ok=True)
    # Temporal AA carries one item's history into the next shot's edges; FXAA has no history.
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    # sRGB target + the finished LDR image: the two have to agree or the icon is encoded twice
    # and comes out washed out.
    rt = unreal.RenderingLibrary.create_render_target2d(w, SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    for item, path, yaw, pitch, roll in ITEMS:
        mesh = unreal.load_asset(path)
        if not mesh: print('MISSING', path); continue
        b = mesh.get_bounding_box(); centre = (b.min + b.max) * 0.5
        extent = max(b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z) * 1.25
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
        # centre the mesh's bounds on the origin of the shot regardless of its pivot
        wc = a.get_actor_transform().transform_location(centre)   # the bounds centre in world space
        a.set_actor_location(a.get_actor_location() - (wc - a.get_actor_location()), False, True)
        back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(80.0, Y0, 0), unreal.Rotator(roll=0, pitch=90, yaw=0))   # plane normal -X, behind the item
        back.set_actor_scale3d(unreal.Vector(extent / 20.0, extent / 20.0, 1.0))
        back.static_mesh_component.set_material(0, black)
        lights = []
        for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
            l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, Y0 + y, z)); lc = l.light_component
            lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i); lc.set_attenuation_radius(600); lights.append(l)
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-200.0, Y0, 0), unreal.Rotator(roll=0, pitch=0, yaw=0))   # looking +X at the item's side
        c = cap.get_editor_property('capture_component2d'); c.texture_target = rt
        c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        c.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC; c.ortho_width = extent
        c.set_editor_property('capture_every_frame', False)
        for _ in range(3): c.capture_scene()   # warm the capture so nothing from the last shot lingers
        unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s.png' % icon_name(item))
        for x in [cap, back, a] + lights: eas.destroy_actor(x)
        print('rendered', item, '->', icon_name(item), 'extent %.0f' % extent)
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('done')
except Exception:
    import io as _io; _io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written to RawArt/render_error.txt')
