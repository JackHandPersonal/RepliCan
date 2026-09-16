"""Which Synty alternate palette is the red one.

    python Tools/ue_remote.py --file Tools/soldier_palette_swatches.py
    powershell Tools/palette_pick.ps1

Synty ships the SciFi Space atlas as M_PolygonSciFiSpace_<group>_<A..F>. Reading the textures
does not answer which one makes a SOLDIER red, because the colour depends on where that mesh's
UVs land in the atlas. So the soldier is simply rendered once per alternate and the swatches are
measured; palette_pick.ps1 reports the hue of each and names the red one.
"""
import unreal, io, os, traceback
OUT = r'C:\Dev\Games\RepliCan\RawArt\Palette'
MESH = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Male_01'
GROUPS = ('01', '02', '03')
LETTERS = ('A', 'B', 'C', 'D', 'E', 'F')
LIGHT_SCALE = 0.85
NOMINAL = 180.0
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session first')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    w = ues.get_editor_world()
    Y0 = -60000.0
    for act in eas.get_all_level_actors():
        try:
            if abs(act.get_actor_location().y - Y0) < 4000.0: eas.destroy_actor(act)
        except Exception:
            pass
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        try: os.remove(os.path.join(OUT, f))
        except Exception: pass

    mesh = unreal.load_asset(MESH)
    black = unreal.load_asset('/Game/RepliCan/Materials/M_IconBackBlack')
    a = eas.spawn_actor_from_object(mesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
    o, ext = a.get_actor_bounds(False)
    a.add_actor_world_offset(unreal.Vector(0.0, Y0, 0.0) - o, False, False)
    o, ext = a.get_actor_bounds(False)
    # Chest only: that is where the armour colour lives, and it keeps skin out of the average.
    total_h = max(1.0, 2.0 * ext.z)
    span = max(total_h * 0.22, 2.0 * ext.y * 0.55)
    frame_z = ext.z - total_h * 0.30
    L = NOMINAL / 100.0
    back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(120.0 * L, Y0, frame_z), unreal.Rotator(roll=0, pitch=90, yaw=0))
    back.set_actor_scale3d(unreal.Vector(span / 20.0, span / 20.0, 1.0))
    back.static_mesh_component.set_material(0, black)
    lights = []
    for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
        l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * L, Y0 + y * L, frame_z + z * L)); lc = l.light_component
        lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i * LIGHT_SCALE); lc.set_attenuation_radius(600 * L); lights.append(l)
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    rt = unreal.RenderingLibrary.create_render_target2d(w, 384, 384, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-260.0 * L, Y0, frame_z), unreal.Rotator(roll=0, pitch=0, yaw=0))
    c = cap.get_editor_property('capture_component2d'); c.texture_target = rt
    c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    c.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC; c.ortho_width = span
    c.set_editor_property('capture_every_frame', False)

    done = 0
    for grp in GROUPS:
        for letter in LETTERS:
            name = 'M_PolygonSciFiSpace_%s_%s' % (grp, letter)
            path = '/Game/PolygonSciFiSpace/Materials/%s/%s' % ('Alternates' if letter != 'A' or grp != '01' else '', name)
            mat = unreal.load_asset('/Game/PolygonSciFiSpace/Materials/Alternates/' + name) or unreal.load_asset('/Game/PolygonSciFiSpace/Materials/' + name)
            if not mat: continue
            for i in range(a.skeletal_mesh_component.get_num_materials()):
                a.skeletal_mesh_component.set_material(i, mat)
            for _ in range(3): c.capture_scene()
            unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s.png' % name)
            done += 1
    for x in [cap, back, a] + lights: eas.destroy_actor(x)
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('SWATCHES', done)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written')
