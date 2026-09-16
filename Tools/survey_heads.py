"""How standard are Synty's human heads? Run in the editor (Tools/ue_remote.py --file).

Reads every *_Head part in /Game/RepliCan/CutLibrary (the head of each cut character: the
same geometry the character ships with, cut at the neck by bone weight) and, per head:
triangles, vertices, bounds, materials. Then groups them -- pack, and Male / Female / Other
from the name -- and, within and across groups, asks how alike they are two ways:

  identical  the exact same vertex set (positions rounded to 0.1 cm): one base head reused
  overlap    the share of one head's vertices that coincide (to 0.1 cm) with a vertex of the other

The report is what a person deciding "can one hair piece / one helmet / one morph fit every
head in this category" needs. Written to the log and to Saved/ClaudeAssist/heads_report.txt.
"""
import unreal, os, itertools
MQ = unreal.GeometryScript_MeshQueries; AU = unreal.GeometryScript_AssetUtils
ROOT = '/Game/RepliCan/CutLibrary'
reg = unreal.AssetRegistryHelpers.get_asset_registry()
assets = [a for a in reg.get_assets_by_path(ROOT, recursive=True) if str(a.asset_name).endswith('_Head')]


def first(r, cls):
    if isinstance(r, tuple):
        for x in r:
            if isinstance(x, cls): return x
    return r


heads = []
for a in sorted(assets, key=lambda a: str(a.package_name)):
    path = str(a.package_name); name = str(a.asset_name)[:-5]
    pack = path.split('/')[4]
    sk = unreal.load_asset(path)
    if not sk: continue
    dyn = unreal.DynamicMesh()
    r = AU.copy_mesh_from_skeletal_mesh(sk, dyn, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD()); dyn = r[0] if isinstance(r, tuple) else dyn
    r = unreal.GeometryScript_MeshRepair.compact_mesh(dyn); dyn = r[0] if isinstance(r, tuple) else dyn
    nt = dyn.get_triangle_count()
    # ONLY THE VERTICES A TRIANGLE USES. The cut parts were made by deleting the other parts'
    # triangles, and the vertices those triangles used are still in the asset, unreferenced:
    # a 334-triangle head reports 7,836 vertices and a bounding box the size of the whole body.
    # Counting them made every head look like its own body and no two heads alike.
    verts = set(); xs = []; ys = []; zs = []
    for t in range(nt):
        r = MQ.get_triangle_positions(dyn, t)
        for p in (x for x in r if isinstance(x, unreal.Vector)) if isinstance(r, tuple) else ():
            verts.add((round(p.x, 1), round(p.y, 1), round(p.z, 1))); xs.append(p.x); ys.append(p.y); zs.append(p.z)
    nv = len(verts)
    if not xs: continue
    class _B: pass
    box = _B(); box.min = unreal.Vector(min(xs), min(ys), min(zs)); box.max = unreal.Vector(max(xs), max(ys), max(zs))
    size = box.max - box.min
    low = name.lower()
    sex = 'Female' if 'female' in low else ('Male' if 'male' in low else 'Other')
    mats = [str(m.material_slot_name) for m in sk.materials]
    heads.append(dict(name=name, pack=pack, sex=sex, tris=nt, verts=nv, size=(round(size.x, 1), round(size.y, 1), round(size.z, 1)), top=round(box.max.z, 1), mats=mats, vset=verts))

lines = []
def out(s=''): lines.append(s)
out('HEAD SURVEY: %d heads' % len(heads))
out()
cats = {}
for h in heads: cats.setdefault((h['pack'], h['sex']), []).append(h)
out('%-12s %-7s %3s   %-20s %-20s %-12s' % ('PACK', 'SEX', 'N', 'TRIS min/mean/max', 'VERTS min/mean/max', 'HEIGHT cm'))
for (pack, sex), hs in sorted(cats.items()):
    t = [h['tris'] for h in hs]; v = [h['verts'] for h in hs]; z = [h['size'][2] for h in hs]
    out('%-12s %-7s %3d   %5d/%5d/%5d      %5d/%5d/%5d      %5.1f..%5.1f' % (pack, sex, len(hs), min(t), sum(t) / len(t), max(t), min(v), sum(v) / len(v), max(v), min(z), max(z)))
out()
out('EACH HEAD')
for h in heads:
    out('  %-12s %-7s %-34s tris %5d  verts %5d  size %s  top %s  mats %s' % (h['pack'], h['sex'], h['name'], h['tris'], h['verts'], h['size'], h['top'], ','.join(h['mats'])))
out()
sig = {}
for h in heads: sig.setdefault(frozenset(h['vset']), []).append(h)
out('IDENTICAL GEOMETRY (same vertex set to 0.1 cm): %d distinct heads among %d' % (len(sig), len(heads)))
for vs, hs in sorted(sig.items(), key=lambda kv: -len(kv[1])):
    if len(hs) > 1: out('  x%d  %s' % (len(hs), ', '.join(h['name'] for h in hs)))
out()


def overlap(a, b):
    if not a['vset']: return 0.0
    return len(a['vset'] & b['vset']) / float(len(a['vset']))


out('OVERLAP: mean share of vertices a head of the row group shares with a head of the column group (1.00 = the same head)')
keys = sorted(cats)
out('%-22s' % '' + ''.join('%-20s' % ('%s/%s' % k) for k in keys))
for ka in keys:
    row = '%-22s' % ('%s/%s' % ka)
    for kb in keys:
        pairs = [(a, b) for a in cats[ka] for b in cats[kb] if a is not b]
        row += '%-20s' % ('%.2f' % (sum(overlap(a, b) for a, b in pairs) / len(pairs)) if pairs else '-')
    out(row)
out()
out('WITHIN EACH GROUP: the pair least alike, and the pair most alike')
for k in keys:
    hs = cats[k]
    if len(hs) < 2: continue
    scored = sorted(((overlap(a, b), a['name'], b['name']) for a, b in itertools.combinations(hs, 2)))
    out('  %-20s least %.2f  %s vs %s   |   most %.2f  %s vs %s' % (('%s/%s' % k,) + scored[0] + scored[-1]))
text = '\n'.join(lines)
print(text)
d = os.path.join(unreal.Paths.project_saved_dir(), 'ClaudeAssist'); os.makedirs(d, exist_ok=True)
open(os.path.join(d, 'heads_report.txt'), 'w', encoding='utf-8').write(text)
