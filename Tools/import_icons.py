"""Imports RawArt/Icons/T_Icon_*.png (the matted icons, not the _black/_white shots) as
/Game/RepliCan/Icons/T_Icon_<Name>: UI textures, no mip streaming, sRGB. Re-runnable."""
import unreal, os, glob
RAW = r'C:\Dev\Games\RepliCan\RawArt\Icons'; DEST = '/Game/RepliCan/Icons'
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for f in sorted(glob.glob(os.path.join(RAW, 'T_Icon_*.png'))):
    if f.endswith('_black.png') or f.endswith('_white.png'): continue
    t = unreal.AssetImportTask(); t.filename = f; t.destination_path = DEST
    t.destination_name = os.path.splitext(os.path.basename(f))[0]; t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t)
tools.import_asset_tasks(tasks)
for t in tasks:
    for p in t.get_editor_property('imported_object_paths'):
        tex = unreal.load_asset(str(p))
        if tex:
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property('never_stream', True)
            unreal.EditorAssetLibrary.save_asset(str(p).split('.')[0])
        print('icon', p)
print('imported', len(tasks))
