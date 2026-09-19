// One mouse-sized alien pest, crossing a room and gone.
//
// The behaviour is the whole point. Something that walks smoothly from A to B at a constant
// speed reads as a toy on rails; vermin move in BURSTS -- a fast scuttle, a dead stop, a look
// around, another scuttle. The freeze is what sells it, and it is the thing a naive
// interpolation leaves out.
//
// Nothing here decides when or where a pest appears; AEnvironmentDirector does that, and it
// takes care to spawn them where the player is not looking so they are never seen popping into
// existence. See its pest section.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PestActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class REPLICAN_API APestActor : public AActor
{
	GENERATED_BODY()

public:
	APestActor();

	// Everything it needs for its whole short life: what it looks like, where it starts, where
	// it is going, and how big. It dies on arrival without being told to.
	void Launch(UStaticMesh* Mesh, const FVector& Start, const FVector& Goal, float Scale);

	virtual void Tick(float DeltaSeconds) override;

	// ---- The character of the thing -----------------------------------------
	// A mouse tops about 3 m/s in a panic. These are alien and unbothered, so a little less.
	UPROPERTY(EditAnywhere, Category = "Pest") float DashSpeed = 210.0f;
	UPROPERTY(EditAnywhere, Category = "Pest") FVector2D DashSeconds = FVector2D(0.18f, 0.55f);
	// The stop. Long enough to register as a pause, short enough not to look broken.
	UPROPERTY(EditAnywhere, Category = "Pest") FVector2D FreezeSeconds = FVector2D(0.12f, 0.70f);
	// How far off a straight line it wanders. Vermin do not travel in straight lines; they
	// follow edges and change their minds.
	UPROPERTY(EditAnywhere, Category = "Pest") float WanderCm = 55.0f;
	// Body bob and roll while running, standing in for legs nobody will ever see move.
	UPROPERTY(EditAnywhere, Category = "Pest") float BobHeightCm = 0.45f;
	UPROPERTY(EditAnywhere, Category = "Pest") float BobHz = 14.0f;
	UPROPERTY(EditAnywhere, Category = "Pest") float RollDegrees = 7.0f;
	// It gives up and leaves rather than getting stuck forever on a chair leg.
	UPROPERTY(EditAnywhere, Category = "Pest") float MaxLifeSeconds = 14.0f;
	// Shrinking away over a few frames beats vanishing between one frame and the next, on the
	// occasions when the player IS looking.
	UPROPERTY(EditAnywhere, Category = "Pest") float FadeSeconds = 0.25f;
	UPROPERTY(EditAnywhere, Category = "Pest") float ArriveRadiusCm = 30.0f;
	// Claws on plate, at the start of a burst. Audible only when it is close: something you
	// hear but cannot place is far more unsettling than something you hear and can.
	// (There were five Skitter* settings here. The pests are silent now -- see PickNextBurst for
	// why -- so they are gone rather than left as knobs that do nothing.)

	UPROPERTY(VisibleAnywhere, Category = "Pest") TObjectPtr<UStaticMeshComponent> Body;

private:
	// Keeps it on the deck and off the walls: a downward trace each step, so it follows a ramp
	// or a floor plate edge instead of hovering where the spawn point happened to be.
	bool GroundAt(const FVector& Where, float& OutZ) const;
	void PickNextBurst();

	FVector Goal = FVector::ZeroVector;
	FVector Heading = FVector::ForwardVector;
	float BaseScale = 1.0f;
	float Age = 0.0f;
	float StateLeft = 0.0f;
	float BobPhase = 0.0f;
	float Dying = -1.0f;      // >= 0 once it has started shrinking away
	bool bRunning = true;
};
