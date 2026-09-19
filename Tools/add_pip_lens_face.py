"""Give the scope a dedicated eyepiece lens face: the cap, copied nearer the eye and scaled out.

The sight picture needs ONE surface, facing the shooter, filling the back of the scope. The mesh as
shipped offers neither half of that on its own:

  - the glass disc at x 3.47 reaches radius 4.71 but its face normals are +X, so it is a backface
    from behind the sight and is culled unless the material is made two-sided -- and made two-sided
    it draws as a big circle over the housing, because the housing is single-sided too and is itself
    culled from inside the shroud;
  - the eyepiece cap at x 2.78 faces the eye correctly (normal -X) but only reaches radius 4.04,
    against a rear shell that reaches 6.00 -- which is the thick black band around the picture.

So this COPIES the cap, moves the copy fractionally toward the eye and scales it out about the bore,
and that copy becomes the lens. The original cap goes back to the housing slot and is hidden behind
it. Nothing is deleted, and --revert removes the copy and restores the cap.

Sizes are measured, not guessed: cap outer radius 4.04, rear shell 6.00, so LENS_RADIUS 5.40 leaves
a 0.6 cm bezel of housing all the way round. UVs are carried from the glass disc's own fitted
(y,z) -> (u,v) map and computed from each vertex's PRE-SCALE position, so the 0..1 lens layout
stretches with the face instead of being cropped by it.

  Tools/ue_remote --file Tools/add_pip_lens_face
"""
import unreal, sys, math

Q = unreal.GeometryScript_MeshQueries
AU = unreal.GeometryScript_AssetUtils
MAT = unreal.GeometryScript_Materials
UV = unreal.GeometryScript_UVs
ED = unreal.GeometryScript_MeshEdits
SEL = unreal.GeometryScript_MeshSelection

OPTIC = '/Game/RepliCan/Weapons/Worlds/Parts/SM_Wep_Sniper_02_Optic_01.SM_Wep_Sniper_02_Optic_01'
# cm, the face's half-width. The artist's own eyepiece cap is 2.96 wide about its own centre, which
# reads as 4.04 about the bore only because it sits 1.20 cm low -- that number was measuring the
# offset, not the size. 5.40 was tried and is much too large; 4.30 keeps the picture a little larger
# than the cap it replaces while leaving the housing visible all the way round.
LENS_RADIUS = 4.30
# cm toward the shooter. It exists only to stop the old cap z-fighting through the new face, and
# half a millimetre does that. It was 0.20, which left the lens standing visibly proud: measured,
# nothing at all sits behind this face -- it is the rearmost geometry in the mesh -- so every bit of
# this offset is protrusion.
EYE_OFFSET = 0.05

# How far to drop the face, as a fraction of its own height. The Synty eyepiece is not centred on
# the bore -- its cap sits 1.20 cm low -- so a face centred on the bore rides high in the housing and
# leaves a thick bezel below it and almost none above. This lowers it back toward the opening.
# It does NOT move the aiming mark: since the reticle went into screen space it stays on the point of
# impact wherever the glass sits.
LENS_DROP_FRACTION = 0.10
GLASS_HINTS = ('Reticle', 'RedDot', 'ScopePiP')

# NOT sys.argv, and NOT a plain global. ue_remote runs inside the editor's PERSISTENT Python
# interpreter: whatever a previous call left in sys.argv is still there on the next one. A revert run
# was invoked through a wrapper that set sys.argv, and the very next forward run read that same
# sys.argv, took the revert path again and DELETED THE GLASS DISC -- 24 triangles, off a saved asset,
# with no prompt. So the flag is popped out of globals as it is read: true exactly once, for the call
# that set it, and any later call in the same interpreter sees a clean default.
REVERT = bool(globals().pop('REVERT_LENS_FACE', False))
if '--revert' in sys.argv:
    raise RuntimeError('set REVERT_LENS_FACE=True in the exec globals instead of passing --revert: '
                       'sys.argv persists between ue_remote calls and has already caused data loss')

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if ues.get_game_world() is not None:
    raise RuntimeError('the editor is in Play -- stop PIE before editing a mesh')

asset = unreal.load_asset(OPTIC)
dyn = unreal.DynamicMesh()
dyn = AU.copy_mesh_from_static_mesh(asset, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(),
                                    unreal.GeometryScriptMeshReadLOD())[0]

CY, CZ = 0.0, 24.678


def unpack(r, kind):
    if isinstance(r, tuple):
        return next((x for x in r if isinstance(x, kind)), None)
    return r if isinstance(r, kind) else None


def tris():
    return unreal.GeometryScript_List.convert_triangle_list_to_array(
        Q.get_all_triangle_indices(dyn, False)[1])


def points():
    return unreal.GeometryScript_List.convert_vector_list_to_array(
        Q.get_all_vertex_positions(dyn, False)[1])


def radius(v):
    return math.hypot(v.y - CY, v.z - CZ)


def normal_x(ti):
    n = unpack(Q.get_triangle_face_normal(dyn, ti), unreal.Vector)
    if n is None:
        n = Q.get_triangle_face_normal(dyn, ti)
    return n.x


# FROM static_materials, NOT from the sections. get_num_sections() counts slots that triangles
# actually USE, so a slot whose geometry is all gone reports as absent -- which is precisely the
# state this tool has to be able to start from, since the face it builds may be the only thing in
# the glass slot. The material array keeps the slot either way.
slots = []
for s in asset.get_editor_property('static_materials'):
    mi = s.get_editor_property('material_interface')
    slots.append(mi.get_name() if mi else '')
glass = next((i for i, n in enumerate(slots) if any(h in n for h in GLASS_HINTS)), None)
if glass is None:
    raise RuntimeError('no glass slot on %s' % asset.get_name())
print('%s   glass slot %d (%s)' % (asset.get_name(), glass, slots[glass]))

verts, tl = points(), tris()

if REVERT:
    capx = min(sum(verts[i].x for i in (t.x, t.y, t.z)) / 3.0
               for ti, t in enumerate(tl) if MAT.get_triangle_material_id(dyn, ti)[0] == glass)
    kill = [ti for ti, t in enumerate(tl)
            if MAT.get_triangle_material_id(dyn, ti)[0] == glass
            and sum(verts[i].x for i in (t.x, t.y, t.z)) / 3.0 < capx + 0.1]
    sel = unpack(SEL.convert_index_array_to_mesh_selection(
        dyn, kill, unreal.GeometryScriptMeshSelectionType.TRIANGLES),
        unreal.GeometryScriptMeshSelection)
    dyn = unpack(ED.delete_selected_triangles_from_mesh(dyn, sel), unreal.DynamicMesh) or dyn
    print('removed %d copied lens triangle(s)' % len(kill))
else:
    # ---- the cap: the frontmost flat plate that faces the eye
    plates = {}
    for ti, t in enumerate(tl):
        vs = [verts[t.x], verts[t.y], verts[t.z]]
        xs = [v.x for v in vs]
        if max(xs) - min(xs) > 0.25:
            continue
        if normal_x(ti) > -0.5:
            continue
        plates.setdefault(round(sum(xs) / 3.0, 1), []).append((ti, vs))
    if not plates:
        raise RuntimeError('no eye-facing plate found')
    capx = min(plates)
    cap = plates[capx]

    # ALREADY DONE. The frontmost eye-facing plate being in the glass slot means this has run
    # before; copying it again would stack a second lens on the first, each 0.20 cm nearer the eye
    # and 1.335x bigger, and the sight would silently grow a new layer per run.
    if all(MAT.get_triangle_material_id(dyn, ti)[0] == glass for ti, _ in cap):
        print('the frontmost eye-facing plate at x %.2f is already the glass slot -- nothing to do.'
              % capx)
        print('run with REVERT_LENS_FACE=True first if you want it rebuilt.')
        raise SystemExit

    # THE CAP IS NOT ON THE BORE, and scaling about the bore makes that worse rather than better.
    # Measured: the cap's own centre is z 23.475 against a bore at 24.678, so it sits 1.20 cm low;
    # scaled x1.335 about the bore it ended up 1.61 cm low, which is the "picture too low, crosshair
    # too high" -- one fault, seen twice, because the crosshair is drawn at UV (0.5,0.5) and that
    # lands exactly on the bore. So the copy is RECENTRED on the bore and scaled about its OWN
    # centre, and then the face centre, the bore and the reticle are all the same point.
    # THE PLANE'S TRUE X, not the rounded key the plates were grouped under. Grouping rounds to one
    # decimal, so offsetting from the key put the face 0.184 cm off the cap when 0.200 was asked for
    # -- 8% out, and a third of the value once the offset is half a millimetre.
    capx = sum(v.x for _, vs in cap for v in vs) / float(3 * len(cap))

    cys = [v.y for _, vs in cap for v in vs]
    czs = [v.z for _, vs in cap for v in vs]
    cap_cy = (max(cys) + min(cys)) * 0.5
    cap_cz = (max(czs) + min(czs)) * 0.5
    rmax = max(math.hypot(v.y - cap_cy, v.z - cap_cz) for _, vs in cap for v in vs)
    S = LENS_RADIUS / rmax
    print('cap at x %.2f: %d triangles, own centre (y %.3f z %.3f), %.3f cm off the bore'
          % (capx, len(cap), cap_cy, cap_cz, math.hypot(cap_cy - CY, cap_cz - CZ)))
    print('   outer radius %.2f about its own centre -> scaled x%.3f to %.2f, recentred on the bore,'
          ' %.2f cm toward the eye' % (rmax, S, LENS_RADIUS, EYE_OFFSET))

    # ---- UVs: projected straight down the bore, not inherited from anything.
    # This used to be fitted by least squares from the glass disc's own layout. It no longer is,
    # for two reasons: the disc is gone (it faced downrange, was culled from behind, and showed the
    # shooter nothing, so this face replaces it outright), and a fit is a dependency on geometry
    # that has to survive for the tool to work. A planar projection about the bore is exact by
    # construction instead: UV (0.5, 0.5) IS the bore, and 0..1 spans the face's full width.
    #
    # V is inverted because texture V runs downward: without it an asymmetric reticle would render
    # upside down. A plain crosshair is symmetric and would not show the difference -- which is
    # exactly why it is worth getting right now rather than discovering it later.
    half_h = max(abs(v.z - cap_cz) for _, vs in cap for v in vs) * S
    drop = LENS_DROP_FRACTION * (2.0 * half_h)
    print('face half-height %.2f -> dropped %.3f cm (%.0f%% of its height) below the bore'
          % (half_h, drop, LENS_DROP_FRACTION * 100))

    def lens_uv(dy, dz):
        return unreal.Vector2D(0.5 + (dy * S) / (2.0 * LENS_RADIUS),
                               0.5 - (dz * S) / (2.0 * LENS_RADIUS))

    # ---- the copy: same winding, so the new face keeps the cap's -X normal
    made = 0
    for ti, vs in cap:
        vids, eids = [], []
        for v in vs:
            dy, dz = v.y - cap_cy, v.z - cap_cz          # about the CAP's centre, not the bore
            uv = lens_uv(dy, dz)
            p = unreal.Vector(capx - EYE_OFFSET, CY + dy * S, CZ + dz * S - drop)
            r = ED.add_vertex_to_mesh(dyn, p, 0)
            vids.append(r[1] if isinstance(r, tuple) else r)
            e = UV.add_uv_element_to_mesh(dyn, 0, uv)
            eids.append(e[1] if isinstance(e, tuple) else e)
        r = ED.add_triangle_to_mesh(dyn, unreal.IntVector(vids[0], vids[1], vids[2]), 0, 0)
        nti = r[1] if isinstance(r, tuple) else r
        MAT.set_triangle_material_id(dyn, nti, glass, True)
        UV.set_mesh_triangle_uv_element_i_ds(dyn, 0, nti,
                                             unreal.IntVector(eids[0], eids[1], eids[2]))
        MAT.set_triangle_material_id(dyn, ti, 0, True)   # old cap back to housing, now hidden
        made += 1
    print('added %d lens triangle(s) at x %.2f; the original cap returned to the housing slot'
          % (made, capx - EYE_OFFSET))

to = unreal.GeometryScriptCopyMeshToAssetOptions()
to.enable_recompute_normals = False
to.enable_recompute_tangents = True
to.replace_materials = False
AU.copy_mesh_to_static_mesh(dyn, asset, to, unreal.GeometryScriptMeshWriteLOD())
unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outermost()], False)
print('saved')
