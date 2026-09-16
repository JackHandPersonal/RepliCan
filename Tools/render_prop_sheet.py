"""Contact sheet for a list of static meshes, so props can be chosen by eye instead of by name.

    python Tools/ue_remote.py --file Tools/render_prop_sheet.py
    powershell Tools/prop_sheet.ps1

Edit PROPS below. Renders each to RawArt/Props/<AssetName>.png on black in the icon booth, framed
to the piece so a mug and a fridge both fill their tile. Tools/prop_sheet.ps1 tiles them with
names underneath.
"""
import unreal, io, os, traceback
OUT = r'C:\Dev\Games\RepliCan\RawArt\Props'
CP = '/Game/PolygonCyberCity/Meshes/Props/'
SIZE = 400
LIGHT_SCALE = 0.25          # the value measured for the weapon icons; same rig
PROPS = [CP + n for n in (
    'SM_Prop_Food_Shelf_01', 'SM_Prop_Food_Shelf_02', 'SM_Prop_Food_Shelf_03',
    'SM_Prop_Food_Sticks_01', 'SM_Prop_Food_Sticks_03', 'SM_Prop_Food_Sticks_Holder_01',
    'SM_Prop_Food_Burger_01', 'SM_Prop_Food_Plates_02', 'SM_Prop_Food_Plates_03',
    'SM_Prop_Plates_01', 'SM_Prop_Plates_02', 'SM_Prop_Plates_03',
    'SM_Prop_Food_Tray_02', 'SM_Prop_Food_Tray_03',
    'SM_Prop_Noodle_Box_02',
    'SM_Prop_Snack_01', 'SM_Prop_Snack_02', 'SM_Prop_Snack_03',
    'SM_Prop_Snack_04', 'SM_Prop_Snack_05', 'SM_Prop_Snack_06',
    'SM_Prop_Drink_01', 'SM_Prop_Drink_03', 'SM_Prop_Drink_05', 'SM_Prop_Drink_07',
    'SM_Prop_Drink_Boba_01', 'SM_Prop_Drink_Syncola_01', 'SM_Prop_Drink_Syncola_03',
    'SM_Prop_Sink_Wall_01', 'SM_Prop_Small_Table_01',
    'SM_Prop_Bench_Seat_01', 'SM_Prop_Bench_Seat_02',
    'SM_Prop_Stool_02', 'SM_Prop_Vending_Machine_02',
    'SM_Prop_Hologram_Table_01', 'SM_Prop_Fridge_01',
)]
try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before rendering')
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
    black = unreal.load_asset('/Game/RepliCan/Materials/M_IconBackBlack')
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    rt = unreal.RenderingLibrary.create_render_target2d(w, SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    done = 0
    for path in PROPS:
        mesh = unreal.load_asset(path)
        if not mesh: print('MISSING', path); continue
        a = eas.spawn_actor_from_object(mesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=-30.0))
        if not a: print('NOSPAWN', path); continue
        o, ext = a.get_actor_bounds(False)
        a.add_actor_world_offset(unreal.Vector(0.0, Y0, 0.0) - o, False, False)
        o, ext = a.get_actor_bounds(False)
        span = max(2.0 * ext.x, max(2.0 * ext.y, 2.0 * ext.z)) * 1.12
        L = max(0.4, span / 100.0)
        back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(140.0 * L, Y0, 0), unreal.Rotator(roll=0, pitch=90, yaw=0))
        back.set_actor_scale3d(unreal.Vector(span / 20.0, span / 20.0, 1.0))
        back.static_mesh_component.set_material(0, black)
        lights = []
        for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
            l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * L, Y0 + y * L, z * L)); lc = l.light_component
            lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i * LIGHT_SCALE); lc.set_attenuation_radius(600 * L); lights.append(l)
        cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-260.0 * L, Y0, 0), unreal.Rotator(roll=0, pitch=0, yaw=0))
        c = cap.get_editor_property('capture_component2d'); c.texture_target = rt
        c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        c.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC; c.ortho_width = span
        c.set_editor_property('capture_every_frame', False)
        for _ in range(3): c.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s.png' % path.rsplit('/', 1)[-1])
        for x in [cap, back, a] + lights: eas.destroy_actor(x)
        done += 1
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('PROPS RENDERED', done)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written')
