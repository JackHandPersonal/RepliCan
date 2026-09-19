"""Find a rail point on every projectile weapon, so an optic has somewhere to bolt on.

There are optics in the catalogue and almost nothing to put them on: an optic needs an
"optic_mount", the point in the weapon's own space where its underside meets the receiver, and
without one it sits at the origin, inside the grip. That point is not worth authoring by hand for
seventy weapons, and it does not have to be: it is on the mesh.

THE RAIL IS THE TOP OF THE RECEIVER, BEHIND THE MUZZLE. For each weapon, in HAC1 space (weapon
points down +X, grip at the origin):

  - take the vertices over the receiver, which is the stretch of bore between the grip and the
    half-way point to the muzzle -- far enough forward to clear the hand, far enough back that a
    barrel or a foregrip is not mistaken for a rail;
  - keep the ones near the CENTRELINE (a rail is on the bore, not out on a side rail);
  - the mount is the highest of those, at the median x of that top band, on y = 0.

A weapon with no flat top -- a pistol slide, a launcher tube -- still gives a sensible answer: the
highest point over the receiver is where a sight would sit anyway.

The optic each weapon gets is chosen by its stance and kind, not by taste: a rifle or an SMG takes
a red dot, a sniper takes a scope, a launcher and anything already carrying an optic are left
alone.

  Tools/ue_remote.py --file Tools/find_optic_mounts.py

Reports every weapon it could not measure rather than guessing one.
"""
import unreal, io, json, collections, os

WJ = os.path.join('C:/Dev/Games/RepliCan', 'UI', 'Weapons.json')
DRY_RUN = False

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if ues.get_game_world() is not None:
    raise RuntimeError('the editor is in Play; stop the session first')

Q = unreal.GeometryScript_MeshQueries
AU = unreal.GeometryScript_AssetUtils

def rail_point(mesh_path):
    """(x, z) of the rail over the receiver, or None if the mesh cannot be read."""
    a = unreal.load_asset(mesh_path)
    if not a:
        return None
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(a, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    dyn = r[0] if isinstance(r, tuple) else dyn
    pts = []
    for i in range(dyn.get_vertex_count()):
        p, ok = Q.get_vertex_position(dyn, i)
        if ok:
            pts.append((p.x, p.y, p.z))
    if len(pts) < 8:
        return None
    x_hi = max(p[0] for p in pts)
    if x_hi <= 0.5:
        return None
    # Over the receiver: clear of the hand, short of the barrel.
    lo, hi = x_hi * 0.10, x_hi * 0.55
    # On the bore, not out on a side rail: within a fifth of the weapon's half-width of centre.
    y_half = max(0.4, max(abs(p[1]) for p in pts))
    band = [p for p in pts if lo <= p[0] <= hi and abs(p[1]) <= y_half * 0.35]
    if len(band) < 4:
        band = [p for p in pts if lo <= p[0] <= hi]
    if len(band) < 4:
        return None
    z_top = max(p[2] for p in band)
    top = [p for p in band if p[2] > z_top - 0.6]
    xs = sorted(p[0] for p in top)
    return (round(xs[len(xs) // 2], 2), round(z_top, 2))

def wants(w):
    """The optic this weapon should carry, or None to leave it alone."""
    if w.get('optic'):
        return None                       # already has one, and a tuned mount with it
    if not w.get('ranged', True):
        return None
    stance = str(w.get('stance', '')).lower()
    kind = str(w.get('kind', '')).lower()
    name = str(w.get('name', '')).lower()
    if 'launcher' in name or 'grenade' in name or 'rocket' in name:
        return None                       # iron sights on a tube; an optic looks wrong
    if kind == 'shock' or 'shock stick' in name:
        # THE DATA SAYS THESE ARE PROJECTILE WEAPONS -- ammo kind, magazine, fire rate, all set --
        # and by that test they passed. A red dot bolted to a stun baton still looks absurd, so the
        # KIND decides here rather than the ammunition.
        return None
    if 'sniper' in name:
        return 'SM_Wep_Scope_Sniper_01'
    if stance == 'rifle':
        return 'RedDot_02'
    if stance in ('pistol', 'pistol1h'):
        return 'RedDot_01'
    return None

d = json.load(io.open(WJ, encoding='utf-8'), object_pairs_hook=collections.OrderedDict)
ws = d['weapons']
fitted, skipped, failed = [], [], []
for key, w in ws.items():
    optic = wants(w)
    if not optic:
        skipped.append(key)
        continue
    mesh = w.get('mesh')
    pt = rail_point(mesh) if mesh else None
    if not pt:
        failed.append(key)
        continue
    w['optic'] = optic
    w['optic_mount'] = [pt[0], 0, pt[1]]
    fitted.append((key, optic, pt))

print('fitted %d, left alone %d, could not measure %d' % (len(fitted), len(skipped), len(failed)))
for key, optic, pt in fitted:
    print('   %-34s %-26s mount x %.2f z %.2f' % (key, optic, pt[0], pt[1]))
if failed:
    print('could not measure (left untouched):')
    for k in failed:
        print('   ' + k)
if not DRY_RUN:
    io.open(WJ, 'w', encoding='utf-8', newline='\r\n').write(json.dumps(d, indent='\t', ensure_ascii=False))
    print('written')
else:
    print('DRY RUN: nothing written')
