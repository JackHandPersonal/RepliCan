using UnrealBuildTool;

public class RepliCan : ModuleRules
{
	public RepliCan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// THE MODULE ROOT ITSELF, so an include can name the folder it lives in:
		// "Weapons/WeaponCatalog.h". This module has no Public/Private split, and for that shape
		// UnrealBuildTool adds the source directories it finds -- the leaves -- not the root they
		// hang from. While the module was flat those were the same thing and nothing was needed
		// here. The moment the files moved into UI/ Weapons/ Characters/ World/ Items/ Narrative/
		// Core/, every path-qualified include stopped resolving: 88 translation units each failing
		// on its own first line with "Cannot open include file: 'Weapons/WeaponCatalog.h'" for a
		// header sitting right there. One line, and the folder-qualified form works module-wide.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"UMG", "Niagara", "RHI", "RenderCore",
			"Slate",
			"SlateCore",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"ProceduralMeshComponent"
		});

		// Editor only: PIE drops the pawn at the level viewport's camera (see ABasePlayerController::PlaceAtEditorCamera).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}
	}
}
