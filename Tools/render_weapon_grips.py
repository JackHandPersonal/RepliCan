"""Every weapon, held, rendered so the grips can be reviewed as a checklist.

    python Tools/ue_remote.py --file Tools/render_weapon_grips.py
    powershell Tools/grip_sheet.ps1

Whether a weapon sits convincingly in a hand is one visual question per weapon, and there are
ninety-six of them. Playing the game to find the broken ones does not scale; a wall of tiles
does. Each weapon is attached to a soldier's hand_r bone with exactly the transform
ABaseCharacter uses at runtime, so what these show is what the game shows.

Two framings per weapon:
  third  over-the-shoulder distance, the silhouette as another character sees it
  first  close on the hands, roughly where the first-person eye sits

Set ONLY to a few catalogue keys while iterating.
"""
import unreal, json, io, os, math, traceback

OUT = r'C:\Dev\Games\RepliCan\RawArt\Grips'
BODY = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Male_01'
HAND_BONE = 'WeaponGrip_R'
# A gripping pose, so the hand is closed round the weapon rather than flat in the bind pose.
IDLE_POSE = '/Game/Characters/Animations/Lyra/Rifle/MM_Rifle_Idle_Hipfire'
ONLY = []
SIZE = 460
LIGHT_SCALE = 0.55
# Nothing. Under the Held Asset Standard the weapon mesh is baked into grip space and the socket
# carries the rig correction, so the relative transform IS identity -- exactly what
# ABaseCharacter::ApplyWeapon does. If this sheet ever needs an offset to look right, the asset
# is wrong, not the sheet. See Docs/HeldAssetStandard.md.
GRIP_OFFSET = unreal.Vector(0.0, 0.0, 0.0)
GRIP_ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0)

cat = json.load(io.open(r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json', encoding='utf-8'))['weapons']

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

    body_mesh = unreal.load_asset(BODY)
    black = unreal.load_asset('/Game/RepliCan/Materials/M_IconBackBlack')
    aa_before = unreal.SystemLibrary.get_console_variable_int_value('r.AntiAliasingMethod')
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod 1')
    rt = unreal.RenderingLibrary.create_render_target2d(w, SIZE, SIZE, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)

    # The soldier stands once; only the weapon changes between shots.
    body = eas.spawn_actor_from_object(body_mesh, unreal.Vector(0, Y0, 0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
    o, ext = body.get_actor_bounds(False)
    body.add_actor_world_offset(unreal.Vector(0.0, Y0, 0.0) - o, False, False)
    # POSE the body before measuring anything. In the bind pose the hand is splayed out sideways
    # and every weapon looks wrong no matter where it is put, so the sheet cannot answer the
    # question it exists to answer. A rifle idle from Lyra plays directly: same skeleton.
    idle = unreal.load_asset(IDLE_POSE)
    if idle:
        sc = body.skeletal_mesh_component
        sc.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        d = sc.get_editor_property('animation_data')
        d.set_editor_property('anim_to_play', idle)
        d.set_editor_property('saved_looping', True)
        d.set_editor_property('saved_playing', True)
        d.set_editor_property('saved_position', 0.0)
        sc.set_editor_property('animation_data', d)
        try: sc.set_editor_property('update_animation_in_editor', True)
        except Exception: pass
        sc.set_position(0.0, False)   # evaluates the pose; there is no refresh_bone_transforms in Python
    else:
        print('IDLE POSE MISSING', IDLE_POSE)

    hand = body.skeletal_mesh_component.get_socket_location(HAND_BONE)

    L = 1.8
    back = eas.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/Plane'), unreal.Vector(300.0, Y0, hand.z), unreal.Rotator(roll=0, pitch=90, yaw=0))
    back.set_actor_scale3d(unreal.Vector(12.0, 12.0, 1.0))
    back.static_mesh_component.set_material(0, black)
    lights = []
    for (x, y, z, i) in ((-160, -120, 120, 120), (-140, 140, 60, 60), (-60, 0, -120, 30)):
        l = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x * L, Y0 + y * L, hand.z + z * L)); lc = l.light_component
        lc.set_intensity_units(unreal.LightUnits.CANDELAS); lc.set_intensity(i * LIGHT_SCALE); lc.set_attenuation_radius(700 * L); lights.append(l)

    done = 0
    for key, e in sorted(cat.items()):
        if ONLY and key not in ONLY: continue
        mesh = unreal.load_asset(e['mesh'])
        if not mesh: print('MISSING', e['mesh']); continue
        gun = eas.spawn_actor_from_object(mesh, hand)
        if not gun: print('NOSPAWN', e['mesh']); continue
        gun.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        gun.attach_to_actor(body, HAND_BONE, unreal.AttachmentRule.SNAP_TO_TARGET,
                            unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, False)
        gun.set_actor_relative_location(GRIP_OFFSET, False, False)
        gun.set_actor_relative_rotation(GRIP_ROTATION, False, False)

        hand_now = body.skeletal_mesh_component.get_socket_location(HAND_BONE)
        for tag, dist, height, fov in (('third', 260.0, 40.0, 42.0), ('first', 95.0, 12.0, 52.0)):
            look = unreal.Vector(hand_now.x, hand_now.y, hand_now.z + height * 0.3)
            cam = unreal.Vector(look.x - dist * 0.72, look.y - dist * 0.69, look.z + height)
            rot = unreal.MathLibrary.find_look_at_rotation(cam, look)
            cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, cam, rot)
            c = cap.get_editor_property('capture_component2d')
            c.texture_target = rt
            c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
            c.fov_angle = fov
            c.set_editor_property('capture_every_frame', False)
            for _ in range(3): c.capture_scene()
            unreal.RenderingLibrary.export_render_target(w, rt, OUT, '%s__%s.png' % (e['icon'], tag))
            eas.destroy_actor(cap)
        eas.destroy_actor(gun)
        done += 1

    for x in [back, body] + lights: eas.destroy_actor(x)
    unreal.SystemLibrary.execute_console_command(w, 'r.AntiAliasingMethod %d' % aa_before)
    print('GRIPS RENDERED', done)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc()); print('ERROR written')
