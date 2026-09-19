// Native base class for a facial-control AnimBP (e.g. ABP_KnightFace).
// Exposes the three facial control values as strongly-typed variables the
// AnimGraph's Transform (Modify) Bone nodes read directly, and that
// FaceController sets from C++ each frame -- this is the seam that keeps
// jaw/brow/eye posing independent of whatever body animation is playing
// underneath. Not tied to any one character's rig -- any skeleton whose
// AnimBP derives from this and wires up the same bone-control nodes can
// use it.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FaceAnimInstance.generated.h"

UCLASS()
class REPLICAN_API UFaceAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	// Degrees, rotation around the bone's verified hinge axis (local Z).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float JawHingeDeg = 0.f;

	// Degrees, rotation around the bone's verified hinge axis (local Z).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float EyesHingeDeg = 0.f;

	// Uniform-ish scale applied to the eyes bone for blinking: 1.0 = open,
	// near 0 = closed. Matches the scale-based blink technique verified
	// earlier this project (the eyeball geometry is small relative to the
	// static eyelid-rim already sculpted into the head mesh, so squashing
	// it reads as closed without needing separate eyelid geometry).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float EyesBlinkScale = 1.f;

	// Centimeters, vertical bone-space translation (local Z on this rig) of
	// the single combined "Eyebrows" bone -- the rig has no separate L/R
	// eyebrow bones, so this moves both together.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float BrowHeightCm = 0.f;

	// Degrees, rotation of the same combined Eyebrows bone about its
	// front-to-back axis: positive tilts the left end up (and the right end
	// down), negative does the reverse. Since it's one rigid bar, this seesaw
	// rotation is what produces a "one eyebrow raised" look without needing
	// separate bones per side.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float BrowAngleDeg = 0.f;
};
