// A broken conduit: every few seconds a spit of sparks, a crackle, and now and then a surge of the
// pack's electricity effect. A distraction with a rhythm the ear learns and then mistrusts.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SparkingConduitActor.generated.h"

class UNiagaraSystem;

UCLASS()
class REPLICAN_API ASparkingConduitActor : public AActor
{
	GENERATED_BODY()
public:
	ASparkingConduitActor();
	virtual void BeginPlay() override;

	// MOSTLY NOT SPARKING. These were 4 and 11, which is often enough that the deck reads as
	// permanently broken and the ear stops noticing. A fault you catch out of the corner of your eye
	// every half minute is worth more than one you can set your watch by.
	UPROPERTY(EditAnywhere, Category = "Conduit") float IntervalMin = 14.0f;
	UPROPERTY(EditAnywhere, Category = "Conduit") float IntervalMax = 46.0f;
	UPROPERTY(EditAnywhere, Category = "Conduit") FVector Normal = FVector(0.0f, 0.0f, -1.0f);   // the way the sparks leave
	UPROPERTY(EditAnywhere, Category = "Conduit") int32 Count = 10;
	UPROPERTY(EditAnywhere, Category = "Conduit") float Scale = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Conduit") float SurgeChance = 0.3f;    // a bigger discharge, this often
	// The surge is a looping Niagara asset, so nothing ever declares it finished and bAutoDestroy
	// never fires: 58 of them were found alive in one session. It is stopped by the clock instead.
	UPROPERTY(EditAnywhere, Category = "Conduit") float SurgeFadeAfterSeconds = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Conduit") float Volume = 0.55f;

private:
	FTimerHandle Timer;
	void Schedule();
	void Fire();
};
