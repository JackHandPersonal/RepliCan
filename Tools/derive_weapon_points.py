"""The four points that let a gun be aimed and held without an animation.

    python Tools/ue_remote.py --file Tools/derive_weapon_points.py

Writes into each ranged entry of UI/Weapons.json:

    rear_sight   where the eye lines up, eye relief behind it
    front_sight  the far end of the sight line
    fore_grip    where the support hand wraps the handguard
    (rear_grip)  the origin, by definition -- HAC1 put it there, so it is never written

THE IDEA. Aiming down sights is one geometric statement: the eye, the rear sight, the front
sight and the target are collinear. Given the eye and the direction it is looking, that single
constraint FIXES the weapon -- put the rear sight at eye relief along the view line, and rotate
the weapon so the front sight lands on that line too. No animation is involved and none can do
it better, because the correct answer is different for every weapon and an animation is one
pose.

Then the hands follow the weapon instead of the weapon following the hands: two-bone IK puts
the right hand at the origin and the left at the fore grip. That inversion is the whole point.
One ADS animation cannot fit 43 guns of different lengths; IK to tagged points fits all of them
exactly.

WHY THESE ARE MEASURED, NOT CLICKED. In HAC1 space every gun means the same thing by its own
axes (+X down the barrel, +Z up, origin at the grip -- see Docs/HeldAssetStandard.md), so all
three remaining points are the answer to a geometric question rather than an opinion:

    rear/front sight   the highest thing on the centreline, in the back / the front
    fore grip          the underside of the handguard, past where the magazine hangs

Where that comes out wrong -- and on a few of the stranger alien pieces it will -- the entry is
flagged below and can be corrected by hand or with the bench tool. Clicking 129 points to begin
with would be the wrong way round.

THE PITCH CORRECTION matters and is easy to miss. If the front sight sits lower than the rear,
looking along the sight line is not looking along +X: the weapon has to be pitched up by the
difference over the span, or the player looks down the barrel at the floor. That angle is
written as sight_pitch and the runtime applies it.
"""
import unreal, json, io, math, traceback, collections

CAT = r'C:\Dev\Games\RepliCan\UI\Weapons.json'

CENTRE_BAND = 2.2        # how near the centreline a vertex has to be to count
TOP_PCT = 0.92           # ignore a carry handle or an aerial: not the sight
REAR_FROM, REAR_TO = 0.05, 0.45      # fractions of total length, from the back
FRONT_FROM, FRONT_TO = 0.62, 0.98
SIGHT_RISE = 0.8         # the eye looks over the notch, not through the metal
# The support hand goes here along the barrel, as a fraction of the distance from the grip to
# the muzzle. Far enough forward to be past the magazine, near enough to be a natural reach.
FORE_AT = 0.50          # fallback only, for weapons with nothing hanging underneath
MAG_CLEARANCE = 5.0     # fallback when there is no handguard run to measure
FORE_MAX = 0.80         # ... and never further out than this share of the barrel
FORE_DROP = 1.2          # the hand wraps under the handguard, not flush with it
# Anything outside these is reported for review rather than trusted.
SANE_SIGHT_Z = (4.0, 45.0)
SANE_SPAN = 12.0         # rear to front sight, minimum, for the pitch to mean anything
MAX_SIGHT_PITCH = 5.0    # past this the 'sights' are really just the barrel's slope


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


def top_of(pts, x0, x1, band):
    """Highest point on the centreline in a slice, by percentile rather than maximum."""
    slab = [p for p in pts if x0 <= p[0] <= x1 and abs(p[1]) <= band]
    if len(slab) < 4:
        slab = [p for p in pts if x0 <= p[0] <= x1 and abs(p[1]) <= band * 2.5]
    if len(slab) < 4:
        return None
    zs = sorted(p[2] for p in slab)
    top = zs[min(len(zs) - 1, int(len(zs) * TOP_PCT))]
    at = [p for p in slab if abs(p[2] - top) <= 1.0]
    return (sum(p[0] for p in at) / len(at), top)


def magazine_clear(pts, hi):
    """Where the handguard starts: just forward of whatever hangs deepest under the weapon.

    On a rifle that deepest thing is the magazine, and a support hand belongs in front of it.
    Weapons with nothing hanging down -- a laser, a tube -- fall back to a share of the barrel,
    which is the right answer when there is no magazine to clear."""
    fwd = [p for p in pts if p[0] > hi * 0.55 and abs(p[1]) <= CENTRE_BAND]
    if not fwd:
        return (hi * FORE_AT, None, 0.0)
    bore = (min(p[2] for p in fwd) + max(p[2] for p in fwd)) * 0.5

    import collections as _c
    band = _c.defaultdict(list)
    for p in pts:
        if abs(p[1]) <= CENTRE_BAND:
            band[int(p[0] // 2.0)].append(p)
    cols = []
    for b in sorted(band):
        below = [p for p in band[b] if p[2] < bore]
        if below:
            cols.append((b * 2.0, bore - min(p[2] for p in below)))
    if not cols:
        return (hi * FORE_AT, None, bore)

    deepest = max(cols, key=lambda cc: cc[1])
    # Walk forward from the deepest column until the weapon stops hanging down: that is the
    # front face of the magazine well.
    front = deepest[0]
    for x, depth in cols:
        if x <= deepest[0]:
            continue
        if depth < deepest[1] * 0.45:
            break
        front = x
    # The handguard is the RUN of weapon still hanging below the bore in front of the magazine,
    # ending where the barrel comes out clean. Take its middle. A fixed clearance in front of the
    # magazine overshoots on a short handguard and lands the hand under open barrel, which is
    # how it ended up floating: on the assault rifle the magazine ends at 12 and the handguard
    # runs 14 to 20, so "magazine plus nine" was past the end of the thing being held.
    guard = [(x, depth) for x, depth in cols if x > front and depth > 2.0]
    if guard:
        run = [guard[0]]
        for entry in guard[1:]:
            if entry[0] - run[-1][0] <= 4.0:
                run.append(entry)
            else:
                break
        gx = min((run[0][0] + run[-1][0]) * 0.5, hi * FORE_MAX)
        # The depth comes from the RUN, not from re-measuring the mesh around gx: a window wide
        # enough to be stable also reaches back into the magazine well, and then the hand is
        # placed under the magazine's floor rather than under the handguard -- fifteen
        # centimetres lower, and obviously wrong.
        depths = sorted(d for _, d in run)
        # The handguard's SLOPE, for the support hand's pitch. The underside of the run is
        # z = bore - depth at each column; a least-squares line through those gives dz/dx, and
        # the hand wraps a drooping guard drooping with it rather than at one global angle.
        # HAC1 pitch: nose up positive, so a guard that drops toward the muzzle is negative.
        # ROBUSTLY. A least-squares line through the run's underside gave the assault rifle 15
        # degrees and a dozen weapons the clamp, because the underside of a handguard is rails
        # and greebles and a fitted line chases them. The median of each half of the run is the
        # handguard's actual line; the greebles are outvoted.
        n = len(run)
        if n >= 4 and run[-1][0] > run[0][0]:
            half = n // 2
            a, b = run[:half], run[half:]
            za = sorted(bore - d for _, d in a)[len(a) // 2]
            zb = sorted(bore - d for _, d in b)[len(b) // 2]
            xa = sum(x for x, _ in a) / len(a)
            xb = sum(x for x, _ in b) / len(b)
            pitch_deg = math.degrees(math.atan2(zb - za, max(xb - xa, 1e-3)))
        else:
            pitch_deg = 0.0
        return (gx, depths[len(depths) // 2], bore, pitch_deg)
    return (min(front + MAG_CLEARANCE, hi * FORE_MAX), None, bore, 0.0)


def under_of(pts, x, band, width):
    """The underside of the weapon at a station along the barrel: where a hand wraps."""
    slab = [p for p in pts if abs(p[0] - x) <= width and abs(p[1]) <= band]
    if len(slab) < 4:
        return None
    zs = sorted(p[2] for p in slab)
    return zs[max(0, int(len(zs) * 0.08))]


try:
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ues.get_game_world() is not None:
        raise RuntimeError('the editor is in Play; stop the session before measuring')

    doc = json.load(io.open(CAT, encoding='utf-8'))
    tally = collections.Counter()
    review = []

    for key in sorted(doc['weapons']):
        e = doc['weapons'][key]
        if not e.get('ranged'):
            for f in ('rear_sight', 'front_sight', 'fore_grip', 'sight_pitch'):
                e.pop(f, None)
            tally['melee, skipped'] += 1
            continue
        if e.get('space') != 'hac1':
            tally['NOT NORMALISED'] += 1
            continue
        mesh = unreal.load_asset(e['mesh'])
        if not mesh:
            tally['mesh missing'] += 1
            continue
        pts = verts(mesh)
        if not pts:
            tally['no geometry'] += 1
            continue

        xs = [p[0] for p in pts]
        lo, hi = min(xs), max(xs)
        length = hi - lo
        rear = top_of(pts, lo + length * REAR_FROM, lo + length * REAR_TO, CENTRE_BAND)
        front = top_of(pts, lo + length * FRONT_FROM, lo + length * FRONT_TO, CENTRE_BAND)
        if not rear or not front:
            tally['no sight line'] += 1
            review.append((key, 'no sight line'))
            continue

        rx, rz = rear
        fx, fz = front
        span = fx - rx
        # The angle the weapon has to be pitched UP by so the two sights are level along the
        # line of sight. Positive means the muzzle rises.
        pitch = math.degrees(math.atan2(rz - fz, max(span, 1e-3))) if span >= SANE_SPAN else 0.0
        # A real pair of sights is level to within a couple of degrees. A large angle does not
        # mean this weapon needs a large correction -- it means it HAS NO SIGHTS, and what was
        # measured is the slope of the barrel casing. Tilting the weapon by that would be
        # obeying a measurement that is not answering the question, so past the threshold the
        # correction is dropped and the weapon is aimed down its bore instead, which is what a
        # shooter does with a gun that has no sights anyway.
        if abs(pitch) > MAX_SIGHT_PITCH:
            pitch = 0.0

        # THE SUPPORT HAND GOES ON THE HANDGUARD, AND THE HANDGUARD IS FORWARD OF THE MAGAZINE.
        # A fraction of the barrel length is a guess, and on a weapon with a long magazine well
        # it is a guess that lands the hand ON the magazine. The magazine is findable the same
        # way the pistol grip was: it is the deepest thing hanging under the weapon. So find its
        # front edge, and put the hand a comfortable way in front of that.
        fore_x, guard_depth, bore_z, fore_pitch = magazine_clear(pts, hi) if hi > 0 else (length * 0.5, None, 0.0, 0.0)
        if guard_depth is not None:
            fore_z = bore_z - guard_depth - FORE_DROP
        else:
            under = under_of(pts, fore_x, CENTRE_BAND, max(2.0, length * 0.03))
            fore_z = (under - FORE_DROP) if under is not None else 0.0

        e['rear_sight'] = [round(rx, 2), 0.0, round(rz + SIGHT_RISE, 2)]
        e['front_sight'] = [round(fx, 2), 0.0, round(fz + SIGHT_RISE, 2)]
        e['fore_grip'] = [round(fore_x, 2), 0.0, round(fore_z, 2)]
        # Clamped: past this the "guard" is a barrel taper, and a hand pitched to a barrel taper
        # is a hand pointing at the floor.
        e['fore_grip_pitch'] = round(max(-8.0, min(8.0, fore_pitch)), 2)
        e['sight_pitch'] = round(pitch, 2)
        # The old single "sight" field is what this replaces.
        e['sight'] = e['rear_sight']

        flags = []
        if not (SANE_SIGHT_Z[0] <= rz <= SANE_SIGHT_Z[1]): flags.append('rear sight height %.1f' % rz)
        if span < SANE_SPAN: flags.append('sights only %.1f cm apart' % span)
        if e['sight_pitch'] == 0.0 and span >= SANE_SPAN and abs(math.degrees(math.atan2(rz - fz, span))) > MAX_SIGHT_PITCH:
            flags.append('no usable sight line, aiming down the bore')
        if guard_depth is None: flags.append('no handguard run; fore grip is a fraction of the barrel')
        if flags:
            review.append((key, '; '.join(flags)))
            tally['REVIEW'] += 1
        else:
            tally['clean'] += 1
        print('%-32s len%6.1f  rear(%6.1f,%5.1f) front(%6.1f,%5.1f) span%6.1f pitch%5.1f  fore(%5.1f,%5.1f)%s'
              % (key, length, rx, rz, fx, fz, span, pitch, fore_x, fore_z, '   <-- review' if flags else ''))

    io.open(CAT, 'w', encoding='utf-8', newline='\n').write(json.dumps(doc, indent=1, ensure_ascii=False))
    print('')
    print('POINTS', dict(tally))
    if review:
        print('NEEDS A LOOK (%d):' % len(review))
        for k, why in review:
            print('   %-32s %s' % (k, why))
except Exception:
    io.open(r'C:/Dev/Games/RepliCan/RawArt/render_error.txt', 'w').write(traceback.format_exc())
    print('ERROR', traceback.format_exc().strip().splitlines()[-1])
