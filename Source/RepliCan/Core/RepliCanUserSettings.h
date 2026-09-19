// Per-machine settings that outlive a session (Saved/Config/.../
// GameUserSettings.ini via UGameUserSettings): whether the first-run intro
// has been seen, plus the usual window/resolution options.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "RepliCanUserSettings.generated.h"

UCLASS()
class REPLICAN_API URepliCanUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	static URepliCanUserSettings* Get();

	// Set once the intro has played to the end (or been skipped).
	UPROPERTY(Config) bool bIntroSeen = false;
	// Settings toggle: play the intro on every start, not just the first.
	UPROPERTY(Config) bool bAlwaysShowIntro = false;

	// ---- Environment ----------------------------------------------------
	// Read by AEnvironmentDirector, which owns the level's atmosphere at play time. Each of
	// these is a switch on the settings page; the density values are 0 to 4 steps rather than
	// raw numbers so the page can show them as a bar.
	UPROPERTY(Config) bool bEnvGrade = true;             // exposure, colour, bloom, vignette, grain
	UPROPERTY(Config) bool bEnvAmbientOcclusion = true;
	UPROPERTY(Config) bool bEnvFog = true;
	UPROPERTY(Config) bool bEnvVolumetricFog = true;     // the light shafts; costs more than the fog itself
	UPROPERTY(Config) bool bEnvDust = true;
	UPROPERTY(Config) bool bEnvLampFlicker = true;
	// Vermin, and how often they turn up. 0 is rarely, 4 is often.
	UPROPERTY(Config) bool bEnvPests = true;
	UPROPERTY(Config) int32 EnvPestRateStep = 2;
	// Off by default: overriding exposure is what makes a map look brighter than it did, and
	// the level's light was set by the renderer's own auto exposure before this existed.
	UPROPERTY(Config) bool bEnvFixedExposure = false;
	UPROPERTY(Config) int32 EnvExposure = 2;
	// 0..7 now, not 0..4: an interior needs finer steps down at the bottom of the range than
	// the old table could express.
	UPROPERTY(Config) int32 EnvFogDensity = 3;
	UPROPERTY(Config) int32 EnvAOStrength = 4;
	UPROPERTY(Config) int32 EnvDustDensity = 2;

	// ---- Audio ----------------------------------------------------------
	// Footstep loudness as a 5 step dial, 2 being the level the sounds were balanced at.
	// A multiplier rather than an absolute, so the character's own per-surface and crouch
	// scaling still does its work underneath it.
	UPROPERTY(Config) int32 FootstepVolumeStep = 2;

	// Key rebinds, action id -> "Key|Modifier". Only commands the player has actually changed
	// appear here; everything else reads its default out of the InputBindings table, so a new
	// command does not need a migration and clearing an entry restores the default exactly.
	UPROPERTY(Config) TMap<FString, FString> KeyBindings;
};
