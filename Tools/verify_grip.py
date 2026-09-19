"""Does the rifle now sit in the hand? Checked with numbers, not by eye.

    python Tools/ue_remote.py --file Tools/verify_grip.py

Poses a soldier with Lyra's rifle idle, attaches the rifle to WeaponGrip_R exactly the way
ABaseCharacter::ApplyWeapon does (identity relative transform), and then asks the three
questions that decide whether it is held correctly:

  1. Is the weapon's ORIGIN at the trigger hand?      -- it is the grip, by the HAC1 convention
  2. Does the weapon's +X run down the barrel line?   -- hand_r to hand_l, taken from the pose
  3. Does the support hand fall on the handguard?     -- compare hand_l to the fore grip point

Three numbers, and each one has an obvious right answer. Eyeballing a screenshot cannot tell
the difference between five degrees and twenty-five, and five degrees at the muzzle of a 78 cm
rifle is seven centimetres.
"""
import unreal, io, json, traceback

BODY = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Male_01'
POSE = '/Game/Characters/Animations/Lyra/Rifle/MM_Rifle_Idle_Hipfire'
WEAPON_KEY = 'Worlds/Wep_Assault_01'
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'

V = unreal.Vector
ML = unreal.MathLibrary


def length(v):
    return (v.x * v.x + v.y * v.y + v.z * v.z) ** 0.5


def sub(a, b):
    return V(a.x - b.x, a.y - b.y, a.z - b.z)


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    Y0 = -70000.0
    for a in eas.get_all_level_actors():
        try:
            if abs(a.get_actor_location().y - Y0) < 3000.0: eas.destroy_actor(a)
        except Exception: pass

    body = eas.spawn_actor_from_object(unreal.load_asset(BODY), unreal.Vector(0, Y0, 0))
    sc = body.skeletal_mesh_component
    sc.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    d = sc.get_editor_property('animation_data')
    d.set_editor_property('anim_to_play', unreal.load_asset(POSE))
    d.set_editor_property('saved_looping', True)
    d.set_editor_property('saved_playing', True)
    d.set_editor_property('saved_position', 0.0)
    sc.set_editor_property('animation_data', d)
    try: sc.set_editor_property('update_animation_in_editor', True)
    except Exception: pass
    sc.set_position(0.0, False)

    e = json.load(io.open(CAT, encoding='utf-8'))['weapons'][WEAPON_KEY]
    gun = eas.spawn_actor_from_object(unreal.load_asset(e['mesh']), sc.get_socket_location('hand_r'))
    gun.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    gun.attach_to_actor(body, 'WeaponGrip_R', unreal.AttachmentRule.SNAP_TO_TARGET,
                        unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, False)
    gun.set_actor_relative_location(V(0, 0, 0), False, False)
    gun.set_actor_relative_rotation(unreal.Rotator(roll=0, pitch=0, yaw=0), False, False)

    hr = sc.get_socket_location('hand_r')
    hl = sc.get_socket_location('hand_l')
    gun_loc = gun.get_actor_location()
    gun_fwd = gun.get_actor_forward_vector()
    barrel = ML.normal(sub(hl, hr))

    off = sub(gun_loc, hr)
    dot = max(-1.0, min(1.0, gun_fwd.x * barrel.x + gun_fwd.y * barrel.y + gun_fwd.z * barrel.z))
    import math
    angle = math.degrees(math.acos(dot))

    print('1. weapon origin vs trigger hand:  %.2f cm apart  (%.2f, %.2f, %.2f)'
          % (length(off), off.x, off.y, off.z))
    print('2. weapon +X vs the barrel line:   %.2f degrees' % angle)

    fore = e.get('fore_grip')
    if fore:
        fore_world = gun.get_actor_transform().transform_location(V(*fore))
        print('3. support hand vs fore grip:      %.2f cm apart' % length(sub(fore_world, hl)))
    else:
        span = length(sub(hl, hr))
        print('3. no fore_grip recorded yet; the hands are %.1f cm apart in this pose' % span)

    print('')
    ok = length(off) < 8.0 and angle < 12.0
    print('VERDICT:', 'held correctly' if ok else 'STILL WRONG -- see which number is out')
    eas.destroy_actor(gun)
    eas.destroy_actor(body)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
