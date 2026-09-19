// What a shot leaves behind where it lands: a scorch decal, a burst of sparks or dust, a
// moment of light, and a sound.
//
// The muzzle flash already sells the shot leaving the gun; without this, the shot arrives
// nowhere -- the trace reports a hit in the diagnostic line and the world does not react at
// all, which reads as the weapon not working.
//
// Which effect plays is DATA, in UI/Impacts.json, not a switch here. A rule matches against a
// string built from what was hit (actor label, mesh path, material name) and names the Niagara
// system, decal, light and sound to use. Adding "shots spark off the pipes" is an edit to that
// file; nothing in this header knows what a pipe is. Same rule as the weapons: see
// Docs/HeldAssetStandard.md for why.
#pragma once

#include "CoreMinimal.h"

struct FHitResult;
class UWorld;
class AActor;
class UNiagaraComponent;

namespace ImpactEffects
{
	// One particle system fired once. Seconds > 0 stops the emitter after that long, which is
	// how an ambient loop from a pack (a spark shower, a smoke column) becomes a burst.
	struct FBurst { FString System; float Scale = 1.0f; float Seconds = 0.0f; };
	REPLICAN_API UNiagaraComponent* SpawnBurst(UWorld* World, const FBurst& B, const FVector& Where, const FRotator& Facing);

	// Everything a landed shot does, in one call. Safe to call with a miss (bBlockingHit false):
	// it simply does nothing, so the caller does not need to branch.
	// VolumeScale multiplies the rule's sound: first person turns the impacts down so the report is heard.
	REPLICAN_API void Play(UWorld* World, const FHitResult& Hit, AActor* Instigator, float VolumeScale = 1.0f);

	// Drops the cache so an edited Impacts.json is picked up without restarting.
	REPLICAN_API void Reload();
	REPLICAN_API int32 NumRules();
}
