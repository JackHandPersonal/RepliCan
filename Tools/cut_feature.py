"""Cuts one feature (a button box, a vent, a plate...) out of a placed kit wall into its own
inspectable static mesh. Edit WALL_LABEL / SRC / PIECE / REST and the tags, put a Cube actor
labelled 'Cube' roughly over the feature in the editor, then run via ue_remote.py. Seed =
triangles whose centroid is inside the cube (x1.5 slack), grown along shared vertices while
every vertex stays within 12 cm of the seed cloud, so a loose cube still takes the whole
feature and never the wall face. Afterwards set the piece's collision to complex-as-simple
(scratch piece_collision.py) and point the layout at the *_NoX wall copy."""
import unreal, json, traceback, io
try:
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils; SEL = unreal.GeometryScript_MeshSelection; ED = unreal.GeometryScript_MeshEdits
    EAL = unreal.EditorAssetLibrary
    WALL_LABEL = 'Cabin_S1_Blank'; SRC = '/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Crew_Blank_02'
    OUTDIR = '/Game/RepliCan/Cut'; PIECE = OUTDIR + '/SM_Crew_Blank_02_Button'; REST = OUTDIR + '/SM_Crew_Blank_02_NoButton'
    SLACK = 1.5; GROW = 12.0
    
    actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
    wall = actors[WALL_LABEL]; cube = actors['Cube']
    T = wall.get_actor_transform()
    co, ce = cube.get_actor_bounds(False)
    lo = unreal.Vector(co.x - ce.x * SLACK - 2, co.y - ce.y * SLACK - 2, co.z - ce.z * SLACK - 2)
    hi = unreal.Vector(co.x + ce.x * SLACK + 2, co.y + ce.y * SLACK + 2, co.z + ce.z * SLACK + 2)
    
    sm = unreal.load_asset(SRC)
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
    def inside(p, a, b): return a.x <= p.x <= b.x and a.y <= p.y <= b.y and a.z <= p.z <= b.z
    seed = set()
    for t, (a, b, c) in tri.items():
        pa, pb, pc = vp(a), vp(b), vp(c)
        cen = unreal.Vector((pa.x + pb.x + pc.x) / 3, (pa.y + pb.y + pc.y) / 3, (pa.z + pb.z + pc.z) / 3)
        if inside(cen, lo, hi): seed.add(t)
    print('triangles', len(tri), 'seed', len(seed))
    sel = set(seed)
    if sel:
        pts = [vp(v) for t in sel for v in tri[t]]
        slo = unreal.Vector(min(p.x for p in pts) - GROW, min(p.y for p in pts) - GROW, min(p.z for p in pts) - GROW)
        shi = unreal.Vector(max(p.x for p in pts) + GROW, max(p.y for p in pts) + GROW, max(p.z for p in pts) + GROW)
        vert_tris = {}
        for t, vs in tri.items():
            for v in vs: vert_tris.setdefault(v, []).append(t)
        changed = True
        while changed:
            changed = False
            frontier = set(nt2 for t in sel for v in tri[t] for nt2 in vert_tris[v]) - sel
            for t in frontier:
                if all(inside(vp(v), slo, shi) for v in tri[t]): sel.add(t); changed = True
        pts = [vp(v) for t in sel for v in tri[t]]
        print('grown selection', len(sel), 'world bounds x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f' % (min(p.x for p in pts), max(p.x for p in pts), min(p.y for p in pts), max(p.y for p in pts), min(p.z for p in pts), max(p.z for p in pts)))
        keep = sorted(sel); drop = sorted(set(tri) - sel)
        def make(path, delete_ids):
            if EAL.does_asset_exist(path): EAL.delete_asset(path)
            assert EAL.duplicate_asset(SRC, path), 'dup failed ' + path
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
            EAL.save_asset(path); return dst
        piece = make(PIECE, drop); rest = make(REST, keep)
        # swap the wall, place the piece, drop the cube
        wc = wall.get_component_by_class(unreal.StaticMeshComponent)
        mats = [wc.get_material(i) for i in range(wc.get_num_materials())]
        wc.set_static_mesh(rest)
        for i, m in enumerate(mats): wc.set_material(i, m)
        old = actors.get(WALL_LABEL + '_Button')
        if old: eas.destroy_actor(old)
        pa = eas.spawn_actor_from_object(piece, T.translation, T.rotation.rotator()); pa.set_actor_label(WALL_LABEL + '_Button')
        pc = pa.get_component_by_class(unreal.StaticMeshComponent); pc.set_mobility(unreal.ComponentMobility.STATIC)
        for i, m in enumerate(mats):
            if i < pc.get_num_materials(): pc.set_material(i, m)
        pa.set_editor_property('tags', [unreal.Name('name:Cabin panel'), unreal.Name('desc:A recessed control box. One of its buttons is worn shiny.'), unreal.Name('action:Press')])
        eas.destroy_actor(cube)
        print('saved level', les.save_current_level())
    print('done')
except Exception:
    io.open(r'C:/Users/jhand/AppData/Local/Temp/claude/C--Dev-Claude/f0f1126f-2fae-4098-9f9e-f7a1889d2c6d/scratchpad/cut_err.txt', 'w').write(traceback.format_exc())
