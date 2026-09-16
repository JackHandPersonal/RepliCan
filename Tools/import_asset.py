"""
Import an FBX (exported with one of the ClaudeTest Blender presets) into the
already-open Unreal Editor via Python remote execution. No manual clicking required.

Usage:
    python import_asset.py --fbx <path.fbx> --dest /Game/Props --type static
    python import_asset.py --fbx <path.fbx> --dest /Game/Characters --type skeletal
    python import_asset.py --fbx <path.fbx> --dest /Game/Characters/Animations --type animation --skeleton /Game/Characters/SK_Hero.SK_Hero_Skeleton
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue_remote

STATIC_MESH_TEMPLATE = """
import unreal

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = True
options.import_textures = False
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.generate_lightmap_u_vs = True
options.static_mesh_import_data.auto_generate_collision = True

task = unreal.AssetImportTask()
task.filename = r"{fbx_path}"
task.destination_path = "{dest_path}"
task.automated = True
task.save = True
task.replace_existing = True
task.options = options

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
print("Imported:", list(task.get_editor_property("imported_object_paths")))
"""

SKELETAL_MESH_TEMPLATE = """
import unreal

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.import_materials = True
options.import_textures = False
options.skeletal_mesh_import_data.import_morph_targets = True

task = unreal.AssetImportTask()
task.filename = r"{fbx_path}"
task.destination_path = "{dest_path}"
task.automated = True
task.save = True
task.replace_existing = True
task.options = options

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
print("Imported:", list(task.get_editor_property("imported_object_paths")))
"""

ANIMATION_TEMPLATE = """
import unreal

options = unreal.FbxImportUI()
options.import_mesh = False
options.import_as_skeletal = True
options.import_animations = True
options.skeleton = unreal.load_asset("{skeleton_path}")

task = unreal.AssetImportTask()
task.filename = r"{fbx_path}"
task.destination_path = "{dest_path}"
task.automated = True
task.save = True
task.replace_existing = True
task.options = options

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
print("Imported:", list(task.get_editor_property("imported_object_paths")))
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fbx", required=True, help="Path to the exported FBX file")
    parser.add_argument("--dest", required=True, help="Destination Content path, e.g. /Game/Props")
    parser.add_argument("--type", choices=["static", "skeletal", "animation"], default="static")
    parser.add_argument("--skeleton", help="Existing Skeleton asset path (required for --type animation)")
    args = parser.parse_args()

    fbx_path = os.path.abspath(args.fbx)
    if not os.path.isfile(fbx_path):
        print(f"FBX not found: {fbx_path}")
        sys.exit(1)

    if args.type == "static":
        script = STATIC_MESH_TEMPLATE.format(fbx_path=fbx_path, dest_path=args.dest)
    elif args.type == "skeletal":
        script = SKELETAL_MESH_TEMPLATE.format(fbx_path=fbx_path, dest_path=args.dest)
    else:
        if not args.skeleton:
            print("--skeleton is required for --type animation")
            sys.exit(1)
        script = ANIMATION_TEMPLATE.format(fbx_path=fbx_path, dest_path=args.dest, skeleton_path=args.skeleton)

    result = ue_remote.run(script, mode=ue_remote.ue_re.MODE_EXEC_FILE)
    for entry in result.get("output", []):
        print(entry.get("output", ""), end="")
    if not result.get("success"):
        print("Import failed:", result.get("result"))
        sys.exit(1)


if __name__ == "__main__":
    main()
