"""Cuts the feature under the last Claude Assist click (F9 in play, then click) out of the
clicked static mesh into its own inspectable piece. Reads Saved/ClaudeAssist/last_click.json:
actor label + mesh + world hit point. Seed = triangles whose centroid lies within SEED_R of the
hit point (world space, through the actor's transform), grown along shared vertices while
every vertex stays within GROW of the seed cloud's bounds, so the whole raised feature comes
along and the flat wall face never does. Writes /Game/RepliCan/Cut/SM_<Mesh>_<Feature> and a
*_No<Feature> copy of the wall, swaps the actor to the copy, spawns the piece at the same
transform with inspect tags and complex-as-simple collision, saves assets and level.

Optional overrides via a small JSON next to the script (cut_from_click.json):
{"feature": "Button", "name": "Cabin panel", "desc": "...", "actions": ["Press"], "seed_r": 10, "lift": 0.5, "reach": 40}
Run inside the editor (not during PIE): python Tools/ue_remote.py --file Tools/cut_from_click.py
"""
import unreal, json, os, traceback, io
try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils; SEL = unreal.GeometryScript_MeshSelection; ED = unreal.GeometryScript_MeshEdits
    EAL = unreal.EditorAssetLibrary
    proj = unreal.Paths.project_dir()
    click = json.load(io.open(os.path.join(proj, 'Saved', 'ClaudeAssist', 'last_click.json'), encoding='utf-8'))
    cfg_path = os.path.join(proj, 'Tools', 'cut_from_click.json')
    cfg = json.load(io.open(cfg_path, encoding='utf-8')) if os.path.exists(cfg_path) else {}
    FEATURE = cfg.get('feature', 'Feature'); SEED_R = float(cfg.get('seed_r', 10.0)); GROW = float(cfg.get('grow', 12.0))
    label = click['actor']['label']; mesh_path = click['component']['mesh'].split('.')[0]
    hit = unreal.Vector(click['location']['x'], click['location']['y'], click['location']['z'])
    print('click on', label, mesh_path, 'at', hit)
    wall = [a for a in eas.get_all_level_actors() if a.get_actor_label() == label][0]
    T = wall.get_actor_transform()
    sm = unreal.load_asset(mesh_path); short = sm.get_name().replace('SM_Bld_', '').replace('SM_Prop_', '')
    OUTDIR = '/Game/RepliCan/Cut'; PIECE = '%s/SM_%s_%s' % (OUTDIR, short, FEATURE); REST = '%s/SM_%s_No%s' % (OUTDIR, short, FEATURE)
    dm = unreal.DynamicMesh()
    r = AU.copy_mesh_from_static_mesh(sm, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dm = r[0] if isinstance(r, tuple) else dm
    nt = MQ.get_num_triangle_i_ds(dm)
    tri = {}; vpos = {}
    def vp(v):
        if v not in vpos:
            r = MQ.get_vertex_position(dm, v); p = r[0] if isinstance(r, tuple) else r
            vpos[v] = T.transform_location(p)
        return vpos[v]
    for t in range(nt):
        r = MQ.get_triangle_indices(dm, t); idx = r[0] if isinstance(r, tuple) else r
        if int(idx.x) < 0: continue
        tri[t] = (int(idx.x), int(idx.y), int(idx.z))
    seed = set()
    for t, (a, b, c) in tri.items():
        pa, pb, pc = vp(a), vp(b), vp(c)
        cen = unreal.Vector((pa.x + pb.x + pc.x) / 3, (pa.y + pb.y + pc.y) / 3, (pa.z + pb.z + pc.z) / 3)
        if (cen - hit).length() <= SEED_R: seed.add(t)
    if not seed:
        # nothing within reach of the point: take the single nearest triangle as the seed
        best = min(tri, key=lambda t: min((vp(v) - hit).length() for v in tri[t])); seed.add(best)
    print('triangles', len(tri), 'seed', len(seed))
    def inside(p, a, b): return a.x <= p.x <= b.x and a.y <= p.y <= b.y and a.z <= p.z <= b.z
    sel = set(seed)
    # Growth: walk to triangles that touch the selection (shared vertex POSITION, since Synty's
    # flanges are often unwelded) but only through triangles that stand proud of the wall: the
    # wall plane is the largest triangle near the click, and a triangle joins only if one of its
    # vertices sits more than LIFT in front of that plane along the click normal. The flat strip
    # joining neighbouring boxes lies in the wall plane, so the walk stops at it. A wide bounds
    # box (REACH) around the seed is a safety net against anything else.
    nrm = unreal.Vector(click['normal']['x'], click['normal']['y'], click['normal']['z'])
    LIFT = float(cfg.get('lift', 0.5)); REACH = float(cfg.get('reach', 40.0))
    pts = [vp(v) for t in sel for v in tri[t]]
    rlo = unreal.Vector(min(p.x for p in pts) - REACH, min(p.y for p in pts) - REACH, min(p.z for p in pts) - REACH)
    rhi = unreal.Vector(max(p.x for p in pts) + REACH, max(p.y for p in pts) + REACH, max(p.z for p in pts) + REACH)
    # the wall plane: the rearmost surface (least along the click normal) inside the reach box
    def depth(v):
        p = vp(v); return p.x * nrm.x + p.y * nrm.y + p.z * nrm.z
    base = min(depth(v) for t in tri for v in tri[t] if inside(vp(v), rlo, rhi))
    def lift(v): return depth(v) - base
    def proud(t): return max(lift(v) for v in tri[t]) > LIFT
    # touching = a vertex within MERGE cm of one of the selection's vertices (a 1-cm hash grid,
    # neighbouring cells included, so rounding never splits a coincident pair)
    MERGE = float(cfg.get('merge', 1.0))
    def cell(p): return (int(p.x // MERGE), int(p.y // MERGE), int(p.z // MERGE))
    grid = {}
    for t, vs in tri.items():
        for v in vs: grid.setdefault(cell(vp(v)), []).append((v, t))
    def touching(v):
        p = vp(v); cx, cy, cz = cell(p); out = set()
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    for v2, t2 in grid.get((cx + dx, cy + dy, cz + dz), ()):
                        if (vp(v2) - p).length() <= MERGE: out.add(t2)
        return out
    changed = True
    while changed:
        changed = False
        frontier = set(n2 for t in sel for v in tri[t] for n2 in touching(v)) - sel
        for t in frontier:
            if proud(t) and all(inside(vp(v), rlo, rhi) for v in tri[t]): sel.add(t); changed = True
    print('wall plane at depth %.1f, seed lift %.1f' % (base, max(lift(v) for t in seed for v in tri[t])))
    pts = [vp(v) for t in sel for v in tri[t]]
    print('grown selection', len(sel), 'world bounds x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f' % (min(p.x for p in pts), max(p.x for p in pts), min(p.y for p in pts), max(p.y for p in pts), min(p.z for p in pts), max(p.z for p in pts)))
    keep = sorted(sel); drop = sorted(set(tri) - sel)
    def make(path, delete_ids):
        if EAL.does_asset_exist(path): EAL.delete_asset(path)
        assert EAL.duplicate_asset(mesh_path, path), 'dup failed ' + path
        dst = unreal.load_asset(path)
        cp = unreal.DynamicMesh()
        r = AU.copy_mesh_from_static_mesh(sm, cp, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); cp = r[0] if isinstance(r, tuple) else cp
        r = SEL.convert_index_array_to_mesh_selection(cp, delete_ids, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
        s = [x for x in r if isinstance(x, unreal.GeometryScriptMeshSelection)][0] if isinstance(r, tuple) else r
        r = ED.delete_selected_triangles_from_mesh(cp, s); cp = r[0] if isinstance(r, tuple) else cp
        opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        opts.set_editor_property('enable_recompute_normals', False); opts.set_editor_property('enable_recompute_tangents', False)
        opts.set_editor_property('replace_materials', False); opts.set_editor_property('enable_remove_degenerates', True)
        AU.copy_mesh_to_static_mesh(cp, dst, opts, unreal.GeometryScriptMeshWriteLOD())
        bs = dst.get_editor_property('body_setup')
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        agg = bs.get_editor_property('agg_geom')
        for k in ('convex_elems', 'box_elems', 'sphere_elems', 'sphyl_elems'): agg.set_editor_property(k, [])
        bs.set_editor_property('agg_geom', agg); dst.set_editor_property('body_setup', bs)
        EAL.save_asset(path); return dst
    piece = make(PIECE, drop); rest = make(REST, keep)
    wc = wall.get_component_by_class(unreal.StaticMeshComponent)
    mats = [wc.get_material(i) for i in range(wc.get_num_materials())]
    wc.set_static_mesh(rest)
    for i, m in enumerate(mats): wc.set_material(i, m)
    plabel = label + '_' + FEATURE
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == plabel: eas.destroy_actor(a)
    pa = eas.spawn_actor_from_object(piece, T.translation, T.rotation.rotator()); pa.set_actor_label(plabel)
    pc = pa.get_component_by_class(unreal.StaticMeshComponent); pc.set_mobility(unreal.ComponentMobility.STATIC)
    for i, m in enumerate(mats):
        if i < pc.get_num_materials(): pc.set_material(i, m)
    tags = ['inspectable', 'name:' + cfg.get('name', FEATURE), 'desc:' + cfg.get('desc', '')] + ['action:' + x for x in cfg.get('actions', ['Inspect'])] + list(cfg.get('extra_tags', []))
    pa.set_editor_property('tags', [unreal.Name(t) for t in tags])
    print('piece', plabel, 'tags', tags, 'saved level', les.save_current_level())
    print('done')
except Exception:
    print('ERROR ' + traceback.format_exc().replace('\n', ' | '))
