"""Import the terminal font face: C:\\Dev\\Assets\\Fonts\\VT323-Regular.ttf ->
/Game/RepliCan/UI/F_Crt_Face (UFontFace). CrtStyle.h wraps it in a runtime
UFont, so no Font asset is needed (the Typeface structs aren't scriptable).

Run inside the editor: Tools/ue_remote.py --file Tools/import_crt_font.py
"""
import sys
import unreal

TTF_PATH = r"C:\Dev\Assets\Fonts\VT323-Regular.ttf"
DEST = "/Game/RepliCan/UI"

ttf = sys.argv[1] if len(sys.argv) > 1 else TTF_PATH
task = unreal.AssetImportTask()
task.filename = ttf
task.destination_path = DEST
task.destination_name = "F_Crt_Face"
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
face = unreal.load_asset(DEST + "/F_Crt_Face")
if face:
    face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.LAZY_LOAD)
    unreal.EditorAssetLibrary.save_loaded_asset(face)
print("face:", face)
