"""Turn each weapon's carved-off factory scope into a real entry in the optic system.

Several weapons ship with a sight moulded into the mesh. Tools/strip_scopes.py cut those off into
Parts/<weapon>_Scope_01 so an optic could be mounted in their place, and the catalogue kept the whole
mesh as `mesh` and the cut-down one as `body_mesh`. The game then shows `mesh` when no optic is
fitted -- so "IRON SIGHTS" was never iron sights at all, it was the factory scope coming back with
the un-cut mesh, and the part was not selectable as an optic in its own right.

The cut part is still in the WEAPON's coordinate frame -- measured: SM_Wep_Assault_01_Scope_01's
bounds match parts_bounds.scope to four decimal places -- so it can be put back exactly, with no
eyeballing:

    mount = the weapon's own optic_mount   -> relative position (optic_mount - mount) is zero,
                                              i.e. precisely where it was carved from
    eye   = the weapon's rear_sight        -> `eye` is measured from the OPTIC'S OWN ORIGIN, and this
                                              part's origin IS the weapon's origin, so the rear sight
                                              goes in unchanged. (Subtracting optic_mount here looks
                                              right and is not: that would double-count a rail offset
                                              which the zero relative position has already cancelled.
                                              Measured in game, it put the sight point 3.78 cm behind
                                              and 17.85 cm below where it belongs.)

On a DIFFERENT weapon the same scope lands on that weapon's rail instead, which is what you want
from an interchangeable optic.

Each weapon whose optic is unset also gets its own factory scope as the default, so nothing looks
different today -- what changes is that the scope is now a named, swappable choice, and IRON SIGHTS
(no optic) honestly means no optic.

  python Tools/promote_factory_scopes.py
"""
import io, json, collections

PATH = 'UI/Weapons.json'


def vec3(v):
    """A catalogue vector, tolerating absent, empty and short forms."""
    if not isinstance(v, (list, tuple)):
        return [0.0, 0.0, 0.0]
    out = list(v) + [0.0] * (3 - len(v))
    return [float(x) for x in out[:3]]


d = json.load(io.open(PATH, encoding='utf-8'), object_pairs_hook=collections.OrderedDict)
weapons, optics = d.get('weapons', {}), d.setdefault('optics', collections.OrderedDict())

added, defaulted, skipped, corrected = [], [], [], []
for wkey, w in weapons.items():
    scope = (w.get('parts') or {}).get('scope')
    if not scope or not w.get('body_mesh'):
        continue                      # nothing was cut off this one, or there is no cut-down mesh
    okey = 'Factory_' + wkey.split('/')[-1]
    label = w.get('model') or w.get('name') or wkey
    mount = vec3(w.get('optic_mount'))
    rear = vec3(w.get('rear_sight') or w.get('sight'))
    existing = optics.get(okey)
    if existing is None:
        optics[okey] = collections.OrderedDict([
            ('name', '%s factory scope' % label),
            ('mesh', scope),
            # Its origin IS the weapon's origin, so the mount point is the weapon's own rail: the
            # two cancel and the scope returns to the exact place it was cut from.
            ('mount', mount),
            ('eye', rear),
            ('rot', [0, 0, 0]),
            ('offset', [0, 0, 0]),
        ])
        added.append((okey, label, optics[okey]['eye']))
    else:
        # These keys are generated, so mount and eye are recomputed on every run -- that is what
        # lets a fix to the arithmetic above reach entries that are already written. `rot` and
        # `offset` are NOT touched: those are the two a person tunes by eye.
        before = (existing.get('mount'), existing.get('eye'))
        existing['mesh'], existing['mount'], existing['eye'] = scope, mount, rear
        if before != (mount, rear):
            corrected.append((okey, label, before[1], rear))
        else:
            skipped.append(okey)
    # Wear it by default where nothing else was chosen, so today's look is unchanged.
    if not w.get('optic'):
        w['optic'] = okey
        defaulted.append(label)

io.open(PATH, 'w', encoding='utf-8', newline='\n').write(json.dumps(d, indent=1) + '\n')
print('factory scopes promoted to optics: %d' % len(added))
for okey, label, eye in added:
    print('   %-28s %-34s eye %s' % (okey, label, eye))
if defaulted:
    print('fitted by default (look unchanged): %s' % ', '.join(defaulted))
if corrected:
    print('recomputed (mount/eye), rot and offset left alone:')
    for okey, label, was, now in corrected:
        print('   %-28s %-30s eye %s -> %s' % (okey, label, was, now))
if skipped:
    print('already correct, left alone: %s' % ', '.join(skipped))
