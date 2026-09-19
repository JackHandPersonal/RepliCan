"""Print the side profile of a weapon so the pistol grip can be picked out properly.

    python Tools/ue_remote.py --file Tools/profile_weapon.py

The bake placed each gun's origin by a RULE -- a fraction of the rear extent back, a fraction
of the drop down -- and on the assault rifle that rule missed the grip by enough to see. A
fraction of a bounding box was never going to find a grip; the grip is a SHAPE, and the way to
find a shape is to look at the profile.

This draws the underside of the weapon as text: for every 2 cm along the barrel, how far the
mesh hangs below the bore line. A rifle reads unmistakably in that column -- the stock is
shallow, the grip is a narrow deep spike, the magazine is a wider deep block just in front of
it, and the handguard is shallow again. Which is which stops being a guess.
"""
import unreal, io, json, traceback, collections

WEAPONS = ['Worlds/Wep_Assault_01']
CAT = r'C:\Dev\Games\RepliCan\Content\GameData\UI\Weapons.json'
BUCKET = 2.0
BAND = 3.0

try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')
    cat = json.load(io.open(CAT, encoding='utf-8'))['weapons']

    for key in WEAPONS:
        mesh = unreal.load_asset(cat[key]['mesh'])
        dyn = unreal.DynamicMesh()
        dyn, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
            mesh, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
        pts = []
        for i in range(dyn.get_vertex_count()):
            p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(dyn, i)
            if ok:
                pts.append((p.x, p.y, p.z))
        xs = [p[0] for p in pts]
        lo, hi = min(xs), max(xs)
        fwd = [p for p in pts if p[0] > hi * 0.55 and abs(p[1]) <= BAND]
        bore = (min(p[2] for p in fwd) + max(p[2] for p in fwd)) * 0.5 if fwd else 0.0

        print('')
        print('=== %s   x %.1f..%.1f   bore z %.2f   origin at x=0 ===' % (key, lo, hi, bore))
        print('   x      depth below bore   width   profile')
        band = collections.defaultdict(list)
        for p in pts:
            if abs(p[1]) <= BAND:
                band[int(p[0] // BUCKET)].append(p)
        for b in sorted(band):
            col = band[b]
            below = [p for p in col if p[2] < bore]
            if not below:
                continue
            depth = bore - min(p[2] for p in below)
            width = max(abs(p[1]) for p in below) * 2.0
            x = b * BUCKET
            bar = '#' * min(40, int(depth))
            mark = '  <-- ORIGIN' if -BUCKET <= x < BUCKET else ''
            print('%7.1f %8.1f            %5.1f   %s%s' % (x, depth, width, bar, mark))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
