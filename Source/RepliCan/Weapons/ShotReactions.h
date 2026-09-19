// What a hit does to the thing it hit, beyond the mark the impact leaves: a person flinches
// toward the shot and bleeds; a loose thing is knocked about by physics, or bursts and is gone.
// Walls, doors, machines and anything bolted down take the impact effect and nothing more.
#pragma once
#include "CoreMinimal.h"
struct FHitResult;
namespace ShotReactions
{
	// Which part of a body a bone belongs to, from the Mannequin bone names every character rig
	// here shares: Head, Neck, Torso, ArmL, ArmR, LegL, LegR; empty for no bone.
	REPLICAN_API FString RegionOf(FName Bone);
	// What the last shot struck, for the fire note: "head", "torso", ...; empty for nothing bodily.
	REPLICAN_API const FString& LastRegion();
	// Damage is the weapon's raw points; 0 marks without hurting. A person under it keeps the
	// score in a UVitalityComponent: armour off first, the head half again; a limb past a quarter
	// of the pool is injured, past half comes off; at zero the body falls.
	// bBlunt: a hammer or a fist rather than a bullet or an edge: no blood or cuts, a harder shove and a heavier stagger.
	REPLICAN_API void React(UWorld* World, const FHitResult& Hit, const FVector& ShotDir, AActor* Instigator, float Damage = 0.0f, bool bBlunt = false);
	// A melee blow with a budget: the damage lands as React does, and the result says what the blow
	// spent. A kill costs what was left in the pool (plus the armour that soaked some), a limb
	// destroyed costs what it took to get there; anything less spends the whole budget, and so
	// does a wall, a bolted thing or a blunt weapon, which stop the swing. Spent >= Budget = stopped.
	struct FStrike { float Spent = 0.0f; bool bPerson = false; bool bDied = false; bool bDestroyed = false; };
	REPLICAN_API FStrike Strike(UWorld* World, const FHitResult& Hit, const FVector& Dir, AActor* Instigator, float Budget, bool bBlunt);
	// Takes a region off outright: "Head", "ArmL", "ArmR", "LegL", "LegR" (for the Sever command and the destroyed-limb rule).
	REPLICAN_API bool Sever(UWorld* World, AActor* Who, const FString& Region, const FVector& Dir);
	// A ragdoll that behaves like a body: its mass set from Brawn (48 kg at 0, 80 at 50, 112 at
	// 100), damped so it comes to rest, and every mesh that rides its pose (parts, a head actor,
	// hair) told not to collide, since their bodies sit inside the ragdoll's and would fight it.
	REPLICAN_API void SettleRagdoll(AActor* Who, class USkeletalMeshComponent* Body, int32 Brawn);
	// The Lyra death clip for the side a shot came from (front has three takes), or null.
	REPLICAN_API class UAnimSequence* DeathClipFor(const AActor* Who, const FVector& ShotDir);
	// A pool of blood on whatever floor is under a point, spreading over a few seconds.
	REPLICAN_API void PoolAt(UWorld* World, const FVector& From, float SizeCm);
	// The body falls: a character through ABaseCharacter::Die, an extra as a ragdoll.
	REPLICAN_API void Kill(UWorld* World, AActor* Who, const FVector& Dir);
}
