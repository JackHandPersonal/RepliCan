// A container the player can open from the inspect menu: a Synty crate with
// its lid, holding up to Capacity items (friendly names, like the player's
// inventory). "Open" lifts the lid and brings up the transfer screen.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootBoxActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class REPLICAN_API ALootBoxActor : public AActor
{
	GENERATED_BODY()

public:
	ALootBoxActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") FString DisplayName = TEXT("Footlocker");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") FString Description = TEXT("A PCS-issue footlocker.");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") int32 Capacity = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") TArray<FString> Items;
	UPROPERTY(VisibleAnywhere, Category = "Box") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "Box") TObjectPtr<UStaticMeshComponent> Crate;
	UPROPERTY(VisibleAnywhere, Category = "Box") TObjectPtr<UStaticMeshComponent> Lid;

	UFUNCTION(BlueprintCallable, Category = "Box") void SetOpen(bool bInOpen);
	UFUNCTION(BlueprintPure, Category = "Box") bool IsOpen() const { return bOpen; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	void PlaceLid();

private:
	bool bOpen = false;
};
