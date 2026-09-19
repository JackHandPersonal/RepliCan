"""Write imported textures back out as PNG files, so their artwork can actually be looked at.

An imported texture is a uasset and nothing outside the editor can open it, which makes questions
like "what does the label on that panel say?" unanswerable from the outside. This exports the ones
named below to the scratch folder at full resolution through the editor's own PNG exporter.

It reads assets and writes files outside the project; it changes nothing in the level or the
content browser.

  Tools/ue_remote --file Tools/export_texture_png

Edit WANTED to choose different textures. A path without a leading slash is taken as relative to
the sci-fi horror pack's texture folder, since that is what this is usually pointed at.
"""
import unreal, os

OUT = 'C:/Users/jhand/AppData/Local/Temp/claude/C--Dev-Claude/f0f1126f-2fae-4098-9f9e-f7a1889d2c6d/scratchpad/tex'
HORROR = '/Game/Synty/PolygonSciFiHorror/Textures/'

WANTED = [
    'Alts/T_PolygonSciFiHorror_01_A',
    'Alts/T_PolygonSciFiHorror_02_A',
    'Alts/T_PolygonSciFiHorror_03_A',
    'Alts/T_PolygonSciFiHorror_04_A',
]

os.makedirs(OUT, exist_ok=True)
done, missing = [], []
for name in WANTED:
    path = name if name.startswith('/') else HORROR + name
    asset = unreal.load_asset(path)
    if not asset:
        missing.append(path)
        continue
    target = os.path.join(OUT, path.rsplit('/', 1)[-1] + '.png')
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = target
    task.automated = True
    task.prompt = False
    task.exporter = unreal.TextureExporterPNG()
    unreal.Exporter.run_asset_export_task(task)
    if os.path.exists(target):
        done.append((path.rsplit('/', 1)[-1],
                     asset.blueprint_get_size_x(), asset.blueprint_get_size_y(),
                     os.path.getsize(target)))
    else:
        missing.append(path + ' (exporter wrote nothing)')

print('exported %d of %d to %s' % (len(done), len(WANTED), OUT))
for name, w, h, size in done:
    print('   %-34s %d x %d  %.1f MB' % (name, w, h, size / 1048576.0))
for m in missing:
    print('   MISSING ' + m)
