using UnrealBuildTool;

public class RepliCan : ModuleRules
{
	public RepliCan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
			"JsonUtilities"
		});

		// Editor only: PIE drops the pawn at the level viewport's camera (see ABasePlayerController::PlaceAtEditorCamera).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}
	}
}
