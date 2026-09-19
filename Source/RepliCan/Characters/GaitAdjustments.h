// Runtime "adjustments" layered over whatever locomotion clip is playing --
// none of these are baked into an animation. They are applied by
// FCharacterAnimInstanceProxy every frame, after the clip (and any retarget)
// has produced the final pose: posture terms edit bones in component space,
// timing terms warp the clip's play rate. All default to "no change", live
// in FCharacterConfig::Gait (so they save to the character's JSON) and are
// editable in the Character Manager's Gait section.
#pragma once

#include "CoreMinimal.h"
#include "GaitAdjustments.generated.h"

USTRUCT(BlueprintType)
struct REPLICAN_API FGaitAdjustments
{
	GENERATED_BODY()

	// ---- Posture (pose edits, active in every state except authored poses) --

	// Legs abducted at the hip, in degrees per leg (+ wider, - narrower).
	// Feet are re-levelled so they still sit flat; the ankles end up about
	// sin(deg) * leg length further apart (10 deg ~ 15cm on this rig).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StanceWidthDegrees = 0.0f;

	// Extra abduction that eases in only while walking/jogging/running and
	// back out at rest. The Synty jog swings the ankles to within ~5cm of
	// each other (measured live), which reads knock-kneed; 5 deg here keeps
	// the stride ~15cm apart at the crossing without widening the idle stance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MovingStanceWidthDegrees = 5.0f;

	// Feet turned outward about vertical, degrees per foot (- = pigeon-toed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ToeOutDegrees = 0.0f;

	// Forward bend of the spine, degrees (spread over spine_01..03; the neck
	// takes back half of it so the character still looks ahead).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HunchDegrees = 0.0f;

	// Sideways lean of the spine, degrees (+ toward the character's right).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LeanDegrees = 0.0f;

	// Scale on the arms' swing away from the reference pose (0 = arms hang
	// still, 1 = as animated, 2 = twice the swing).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ArmSwingScale = 1.0f;

	// Scale on the pelvis' vertical bob about its running average height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BounceScale = 1.0f;

	// Hip sway: the pelvis shifts and rolls toward whichever foot is planted
	// (0 = none, 1 = pronounced swagger).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Sway = 0.0f;

	// ---- Timing / irregularity (locomotion states only) --------------------

	// Multiplier on the locomotion play rate (stride cadence). The character
	// still covers the same ground, so > 1 reads as short quick steps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CadenceScale = 1.0f;

	// 0..1: random play-rate jitter, occasional hitches, and noisy sway /
	// lean, all seeded per character so a crowd doesn't move in unison.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stumble = 0.0f;

	// 0..1: the step on the weak leg is hurried and the body drops toward it,
	// the step on the good leg lingers. Which leg is weak: LimpSide.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LimpAmount = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString LimpSide = TEXT("Left");

	bool IsIdentity() const
	{
		return StanceWidthDegrees == 0.0f && MovingStanceWidthDegrees == 0.0f && ToeOutDegrees == 0.0f && HunchDegrees == 0.0f && LeanDegrees == 0.0f
			&& ArmSwingScale == 1.0f && BounceScale == 1.0f && Sway == 0.0f
			&& CadenceScale == 1.0f && Stumble == 0.0f && LimpAmount == 0.0f;
	}
};
