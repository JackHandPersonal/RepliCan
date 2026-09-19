"""Fit an optic to a weapon, on the rail, from the sight line the weapon already has.

    python Tools/ue_remote.py --file Tools/fit_optics.py

An optic does two things to a weapon, and both are data:

  1. It sits on the rail -- the flat on top of the receiver, which is the line between the
     weapon's own rear and front sights. Those are already measured
     (Tools/derive_weapon_points.py), so the mount is not a new question: put the optic a short
     way forward of the rear sight, at the height the receiver actually is there.
  2. It BECOMES the rear sight. The eye lines up through the glass instead of over the notch,
     so the aim point moves to the lens centre and everything downstream -- the ADS solve, the
     carry offsets, the sight pitch -- keeps working with no changes at all. That is the return
     on having expressed aiming as geometry rather than as an animation.

The optic's own eye point is a property of the optic, not of the weapon, so it lives in the
"optics" block and is shared by everything that mounts one.
"""
import unreal, json, io, traceback

CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'

# The optics themselves. "eye" is the lens centre in the optic's own space, which for the
# generated red dot is the middle of the glass disc -- see Tools/make_optics.py.
OPTICS = {
    'RedDot_01': {
        'mesh': '/Game/RepliCan/Optics/SM_Optic_RedDot_01',
        'eye': [-0.6, 0.0, 2.2],
        'sit': 2.2,
        'name': 'RDS-1 reflex sight',
    },
}

# Which weapons get one, and which optic.
FIT = {
    'Worlds/Wep_Assault_01': 'RedDot_01',
    'Horror/Wep_Rifle_01': 'RedDot_01',
}

# How far along the sight line to sit, as a fraction of rear-to-front. A red dot goes toward the
# back of the rail, over the receiver rather than out on the handguard.
ALONG = 0.36
CENTRE_BAND = 3.0
CLEARANCE = 1.2         # how far the line of sight must clear the tallest thing behind the glass
MIN_OVER_BORE = 4.0     # ... and never lower than this above the bore, however flat the weapon
# The derived sights sit SIGHT_RISE above the metal; the optic clamps to the metal itself.
SIGHT_RISE = 0.8

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before editing the catalogue')

    doc = json.load(io.open(CAT, encoding='utf-8'))
    doc['_optics'] = ("Optics that can be fitted to a weapon. A weapon naming one in its 'optic' field mounts "
                      "it at 'optic_mount' (its own space) and AIMS THROUGH IT: the lens centre replaces the "
                      "weapon's rear sight, so the ADS solve needs no special case. Fitted by Tools/fit_optics.py.")
    doc['optics'] = OPTICS

    def verts(mesh):
        dyn = unreal.DynamicMesh()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        out = []
        for i in range(dyn.get_vertex_count()):
            p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
            if ok:
                out.append((p.x, p.y, p.z))
        return out

    for key, optic in FIT.items():
        e = doc['weapons'].get(key)
        if not e:
            print('MISSING WEAPON', key); continue
        mesh = unreal.load_asset(e['mesh'])
        if not mesh:
            print('MISSING MESH', key); continue
        pts = verts(mesh)
        xs = [p[0] for p in pts]
        lo, hi = min(xs), max(xs)

        # The bore: mid-height of the forward half, where there is nothing but barrel.
        fwd = [p for p in pts if p[0] > hi * 0.55 and abs(p[1]) <= CENTRE_BAND]
        bore = (min(p[2] for p in fwd) + max(p[2] for p in fwd)) * 0.5 if fwd else 0.0

        mx = hi * ALONG
        # THE MEASUREMENT THAT MATTERS, and it is FORWARD of the glass, not behind it. The eye
        # sits behind the lens, so the stock and the comb are behind the eye and block nothing;
        # what fills the view through an optic is the weapon's own structure DOWNRANGE of it --
        # the front sight, a carry handle, the top of the receiver. Take the tallest thing on
        # the centreline in front of the mount and put the glass above that.
        #
        # (Measuring behind the mount instead puts the glass above the STOCK COMB, which on this
        # rifle is the tallest thing on it: a 17 cm riser, solving a problem that was not there.)
        ahead = [p for p in pts if mx <= p[0] <= hi and abs(p[1]) <= CENTRE_BAND]
        obstruction = max((p[2] for p in ahead), default=bore)

        glass_z = max(obstruction + CLEARANCE, bore + MIN_OVER_BORE)
        mz = glass_z - OPTICS[optic]['sit']

        e['optic'] = optic
        e['optic_mount'] = [round(mx, 2), 0.0, round(mz, 2)]
        eye = OPTICS[optic]['eye']
        e['sight'] = [round(mx + eye[0], 2), 0.0, round(mz + eye[2], 2)]
        # An optic is level with the bore by construction, so the weapon's own sight slope no
        # longer applies. Left explicit rather than inherited: inheriting it would tilt a
        # perfectly good optic by the error in the irons underneath it.
        e['sight_pitch'] = 0.0
        print('%-28s bore %5.2f  tallest ahead %5.2f  ->  mount (%6.2f, %5.2f)  glass %5.2f'
              % (key, bore, obstruction, mx, mz, glass_z))

    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('OPTICS FITTED', len(FIT))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
