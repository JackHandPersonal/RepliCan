// Per-expression bone-pose value for the eyes, driven onto a
// UFaceAnimInstance via the keyboard-driven SetExpression path. Brow
// posing has its own independent system now (AFaceController::SetBrowHeight/
// SetBrowAngle, driving UFaceAnimInstance::BrowHeightCm/BrowAngleDeg),
// not tied to named expressions, so it isn't part of this struct.

#pragma once

#include "CoreMinimal.h"
#include "FacialExpressionPose.generated.h"

USTRUCT(BlueprintType)
struct FFacialExpressionPose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	float EyesHingeDeg = 0.f;
};
