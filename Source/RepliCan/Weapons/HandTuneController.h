// The controller of the hand-tuning page's stand-in: the page tunes a copy of the player in a
// booth off the map, and that copy needs a controller so its aim (the control rotation) can be
// set. An AAIController steers its control rotation from movement every tick; this one holds
// whatever the page gave it.
#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "HandTuneController.generated.h"

UCLASS()
class REPLICAN_API AHandTuneController : public AAIController
{
	GENERATED_BODY()
public:
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override {}
};
