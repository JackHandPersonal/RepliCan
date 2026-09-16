"""What rotation does WeaponGrip_R actually need? Taken from a pose that holds a rifle properly.

    python Tools/ue_remote.py --file Tools/derive_grip_socket.py

THE MISTAKE THIS FIXES. The socket was first authored by hand as "yaw -90 off hand_r", reasoning
about the bone frame from the outside. Measured, that socket comes out at pitch 76, yaw -97 in
world with the body at yaw zero -- a HAC1 weapon hung off it points nearly straight up. Guessing
a bone's local frame does not work, and the bind pose is the worst place to guess from: the arm
hangs at the side, so hand_r there is rotated most of a right angle away from where it sits when
the character is actually holding something.

THE MEASUREMENT. Lyra's MM_Rifle_Idle_Hipfire was authored by someone holding a rifle correctly.
In that pose, hand_r's rotation IS the grip orientation -- so the socket is simply whatever
turns the hand's frame into the weapon's frame:

    Socket = HandRotation(in the rifle pose)^-1 * WeaponRotation(where a rifle should point)

and "where a rifle should point" is the character's own forward and up, which are themselves
measured from the pose rather than assumed:

    up      = pelvis -> head
    right   = clavicle_l -> clavicle_r
    forward = right x up          (UE is left-handed: Y x Z = X)

Nothing here assumes the mesh faces +X or +Y, or which way round hand_r is. It reads the body.

The location is measured the same way: the palm centre, taken as a short step from the wrist
toward the middle finger, since that is where a grip sits in a closed fist.
"""
import unreal, io, traceback

BODY = '/Game/PolygonSciFiSpace/Meshes/CharactersUE4/SK_Chr_SpaceSoldier_Male_01'
POSE = '/Game/Characters/Animations/Lyra/Rifle/MM_Rifle_Idle_Hipfire'
# How far along the hand bone the grip sits. A grip is held in the palm, not at the wrist joint.
PALM_FRACTION = 0.45

V = unreal.Vector
ML = unreal.MathLibrary

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
    pose = unreal.load_asset(POSE)
    if not pose:
        raise RuntimeError('pose missing: ' + POSE)
    sc.set_editor_property('animation_mode', unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    d = sc.get_editor_property('animation_data')
    d.set_editor_property('anim_to_play', pose)
    d.set_editor_property('saved_looping', True)
    d.set_editor_property('saved_playing', True)
    d.set_editor_property('saved_position', 0.0)
    sc.set_editor_property('animation_data', d)
    try: sc.set_editor_property('update_animation_in_editor', True)
    except Exception: pass
    sc.set_position(0.0, False)      # evaluates the pose

    def cross(a, b):
        return V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)

    def at(bone):
        return sc.get_socket_location(bone)

    origin = body.get_actor_location()
    def rel(p):
        return V(p.x - origin.x, p.y - origin.y, p.z - origin.z)

    pelvis, head = rel(at('pelvis')), rel(at('head'))
    up = ML.normal(V(head.x - pelvis.x, head.y - pelvis.y, head.z - pelvis.z))

    # The barrel direction, taken from the pose itself. In a rifle idle BOTH HANDS ARE ON THE
    # GUN, so the line from the trigger hand to the support hand runs along the weapon. That is
    # a far better answer than deriving forward from the shoulders: a rifle stance is bladed,
    # the shoulder line sits thirty-odd degrees off the aim, and using it would hang the weapon
    # across the chest at exactly that angle.
    hr, hl = rel(at('hand_r')), rel(at('hand_l'))
    barrel = V(hl.x - hr.x, hl.y - hr.y, hl.z - hr.z)
    reach = (barrel.x**2 + barrel.y**2 + barrel.z**2) ** 0.5
    forward = ML.normal(barrel)
    # Orthogonalise up against the barrel so the pair is a clean frame.
    dot = forward.x * up.x + forward.y * up.y + forward.z * up.z
    up = ML.normal(V(up.x - forward.x * dot, up.y - forward.y * dot, up.z - forward.z * dot))

    print('in the rifle pose:')
    print('   hands are %.1f cm apart -- that is where the fore grip sits along the barrel' % reach)
    print('   barrel  %s' % forward)
    print('   up      %s' % up)

    hand_t = sc.get_socket_transform('hand_r', unreal.RelativeTransformSpace.RTS_WORLD)
    hand_rot = hand_t.rotation
    # Where a rifle should point in this pose: along the body's own forward, level with its up.
    want = ML.make_rot_from_xz(forward, up)
    print('   hand_r world rotation %s' % hand_rot.rotator())
    print('   a rifle should be at  %s' % want)

    # Socket = hand^-1 * want, done as transforms so the API does the quaternion work.
    hand_x = unreal.Transform(location=V(0, 0, 0), rotation=hand_rot.rotator(), scale=V(1, 1, 1))
    want_x = unreal.Transform(location=V(0, 0, 0), rotation=want, scale=V(1, 1, 1))
    socket_x = ML.compose_transforms(want_x, ML.invert_transform(hand_x))
    socket_rot = socket_x.rotation.rotator()

    # The palm: a step from the wrist toward the middle finger, which is where a grip sits.
    wrist = at('hand_r')
    tip = None
    for child in ('middle_01_r', 'index_01_r', 'ring_01_r'):
        try:
            p = at(child)
            if p and (abs(p.x) + abs(p.y) + abs(p.z)) > 0.0:
                tip = p; break
        except Exception:
            pass
    if tip:
        along = V(tip.x - wrist.x, tip.y - wrist.y, tip.z - wrist.z)
        palm_world = V(wrist.x + along.x * PALM_FRACTION, wrist.y + along.y * PALM_FRACTION, wrist.z + along.z * PALM_FRACTION)
        offset_world = V(palm_world.x - wrist.x, palm_world.y - wrist.y, palm_world.z - wrist.z)
        socket_loc = ML.inverse_transform_direction(hand_x, offset_world)
        print('   palm is %.2f cm from the wrist' % ((offset_world.x**2 + offset_world.y**2 + offset_world.z**2) ** 0.5))
    else:
        socket_loc = V(0, 0, 0)
        print('   no finger bone found; socket sits on the wrist')

    print('')
    print('SOCKET  location (%.2f, %.2f, %.2f)' % (socket_loc.x, socket_loc.y, socket_loc.z))
    print('SOCKET  rotation pitch %.2f  yaw %.2f  roll %.2f' % (socket_rot.pitch, socket_rot.yaw, socket_rot.roll))
    io.open(r'C:/Dev/Games/RepliCan/Tools/grip_socket.json', 'w', encoding='utf-8', newline='\n').write(
        '{\n "_": "Measured by Tools/derive_grip_socket.py from the Lyra rifle idle. Feed to Tools/add_grip_sockets.py.",\n'
        ' "location": [%.2f, %.2f, %.2f],\n "rotation": [%.2f, %.2f, %.2f]\n}\n'
        % (socket_loc.x, socket_loc.y, socket_loc.z, socket_rot.pitch, socket_rot.yaw, socket_rot.roll))
    eas.destroy_actor(body)
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
