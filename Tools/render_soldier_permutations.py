"""Every body-and-head combination the SpaceSoldier set allows, rendered so they can be chosen
by eye rather than guessed at from asset names.

    python Tools/ue_remote.py --file Tools/render_soldier_permutations.py
    powershell Tools/permutation_sheet.ps1

The pack ships three soldier BODIES, two of which are headless, plus two bare heads, two helmets
and a shoulder-armour piece. Everything is on UE4_Mannequin_Skeleton, so any head will physically
sit on any body through SetLeaderPoseComponent -- what the renders answer is whether the neck
seam and the proportions actually agree, which the names cannot tell you.

Writes RawArt/Perms/<body>__<head>.png in the red trooper palette.
"""
import unreal, io, os, traceback
OUT = r'C:\Dev\Games\RepliCan\RawArt\Perms'
SM = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/'
MAT = '/Game/PolygonSciFiSpace/Materials/Alternates/M_PolygonSciFiSpace_02_F'
LIGHT_SCALE = 0.85
NOMINAL = 180.0
HEAD_FRACTION = 0.42        # head and chest: the part that differs between combinations

BODIES = [
    ('Male_01', SM + 'SK_Chr_SpaceSoldier_Male_01'),
    ('Female_01', SM + 'SK_Chr_SpaceSoldier_Female_01'),
    ('BR_Male_01', SM + 'SK_Chr_BR_SpaceSoldier_Male_01'),
]
HEADS = [
    ('bare', None),
    ('HeadMale', SM + 'SK_Chr_SpaceSoldier_Head_Male_01'),
    ('HeadFemale', SM + 'SK_Chr_SpaceSoldier_Head_Female_01'),
    ('HelmetMale', SM + 'SK_Chr_Attach_SpaceSoldier_Male_Helmet_01'),
    ('HelmetFemale', SM + 'SK_Chr_Attach_SpaceSoldier_Female_Helmet_01'),
    ('Armour', SM + 'SK_Chr_Attach_SpaceSoldier_Armour_01'),
]

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

    mat = unreal.load_asset(MAT)
    black = unreal.load_asset('/Game/RepliCan/Materials/M_IconBackBlack')
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    rt = unreal.RenderingLibrary.create_render_target2d(w, 420, 420, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    L = NOMINAL / 100.0

    done = 0
    for bname, bpath in BODIES:
        bmesh = unreal.load_asset(bpath)
        if not bmesh: print('MISSING BODY', bpath); continue
        for hname, hpath in HEADS:
            body = eas.spawn_actor_from_object(bmesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
            if not body: print('NOSPAWN', bpath); continue
            o, ext = body.get_actor_bounds(False)
            body.add_actor_world_offset(unreal.Vector(0.0, Y0, 0.0) - o, False, False)
            bc = body.skeletal_mesh_component
            if mat:
                for i in range(bc.get_num_materials()): bc.set_material(i, mat)

            head = None
            if hpath:
                hmesh = unreal.load_asset(hpath)
                if hmesh:
                    head = eas.spawn_actor_from_object(hmesh, body.get_actor_location(), body.get_actor_rotation())
                    if head:
                        hc = head.skeletal_mesh_component
                        if mat:
                            for i in range(hc.get_num_materials()): hc.set_material(i, mat)
                        hc.set_leader_pose_component(bc)

            o, ext = body.get_actor_bounds(False)
            total_h = max(1.0, 2.0 * ext.z)
            span = max(total_h * HEAD_FRACTION, 2.0 * ext.y * 0.85)
            frame_z = ext.z - total_h * HEAD_FRACTION * 0.5
            back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(120.0 * L, Y0, frame_z), unreal.Rotator(roll=0, pitch=90, yaw=0))
            back.set_actor_scale3d(unreal.Vector(span / 20.0, span / 20.0, 1.0))
            back.static_mesh_component.set_material(0, black)
            lights = []
            for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
                l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * L, Y0 + y * L, frame_z + z * L)); lc = l.light_component
                lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i * LIGHT_SCALE); lc.set_attenuation_radius(600 * L); lights.append(l)
            cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(-260.0 * L, Y0, frame_z), unreal.Rotator(roll=0, pitch=0, yaw=0))
            c = cap.get_editor_property('capture_component2d'); c.texture_target = rt
            c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
            c.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC; c.ortho_width = span
            c.set_editor_property('capture_every_frame', False)
            for _ in range(3): c.capture_scene()
            unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s__%s.png' % (bname, hname))
            for x in ([cap, back, body] + lights + ([head] if head else [])): eas.destroy_actor(x)
            done += 1
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('PERMUTATIONS', done)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written')
