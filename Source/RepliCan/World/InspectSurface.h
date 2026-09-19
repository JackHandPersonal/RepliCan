// A marker you lay over any bit of world geometry to make it inspectable:
// a thin box you place and scale in the editor (visible there as a faint
// green slab), invisible in play except for a glow while the player's
// inspect reticle is on it. It answers the inspect trace on the Visibility
// channel only, so it never blocks movement. Name, Description and Actions
// feed the inspect menu exactly like a tagged prop.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InspectSurface.generated.h"

class UStaticMeshComponent;

UCLASS()
class REPLICAN_API AInspectSurface : public AActor
{
	GENERATED_BODY()

public:
	AInspectSurface();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect") FString Name = TEXT("Panel");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect") FString Description;
	// Menu entries; "Inspect" shows the description, anything else fires OnInspectAction on the controller.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect") TArray<FString> Actions = { TEXT("Inspect") };
	// The slab: 100 x 100 x 4 cm before the actor's scale. Scale X/Y to cover the surface.
	UPROPERTY(VisibleAnywhere, Category = "Inspect") TObjectPtr<UStaticMeshComponent> Slab;

	// The controller's hover highlight lands here (the slab is hidden in play otherwise).
	UFUNCTION(BlueprintCallable, Category = "Inspect") void SetHovered(bool bOn);

protected:
	virtual void BeginPlay() override;
};
