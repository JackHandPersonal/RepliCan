using UnrealBuildTool;
using System.Collections.Generic;

public class RepliCanTarget : TargetRules
{
	public RepliCanTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("RepliCan");
		// Bake NPC voice lines (Piper TTS) before compiling; incremental, and a
		// no-op on machines without the Piper venv (see Tools/bake_voices.cmd).
		PreBuildSteps.Add("call \"$(ProjectDir)\\Tools\\bake_voices.cmd\"");
	}
}
