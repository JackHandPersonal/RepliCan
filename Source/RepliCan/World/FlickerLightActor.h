// A small point light that pulses irregularly, like firelight seen through
// a grille: three unrelated slow sines plus random flares that die away.
// Placed as a level actor (Tools/facility_layout.py) beside a lamp housing
// so the glow has a visible source. Never brighter than BaseIntensity * 1.3.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlickerLightActor.generated.h"

class UPointLightComponent;

UCLASS()
class REPLICAN_API AFlickerLightActor : public AActor
{
	GENERATED_BODY()

public:
	AFlickerLightActor();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float BaseIntensity = 6.0f;   // candelas at the mean level
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float AttenuationRadius = 450.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") FLinearColor Color = FLinearColor(1.0f, 0.18f, 0.06f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float Speed = 1.0f;           // pulse rate multiplier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float FlaresPerSecond = 0.5f; // average rate of the random flares
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float Seed = 0.0f;            // phase offset so neighbours differ
	// 0 = all irregular flicker and flares; 1 = a clean pulse at Period seconds; between = both.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float Regularity = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flicker") float Period = 2.0f;

	UPROPERTY(VisibleAnywhere, Category = "Flicker") TObjectPtr<UPointLightComponent> Light;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	void ApplyStatic();

private:
	float Clock = 0.0f;
	float Flare = 0.0f;
};
