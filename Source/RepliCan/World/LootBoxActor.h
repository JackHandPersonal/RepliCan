// A container the player can open from the inspect menu: a Synty crate with
// its lid, holding up to Capacity items (friendly names, like the player's
// inventory). "Open" lifts the lid and brings up the transfer screen.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootBoxActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

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
	// ANY CRATE AND LID (the defaults are the Space kit's footlocker, whose lid is modelled in place).
	// A pack whose lid is its own piece about its own origin names the pair here, with the lid's
	// closed seat relative to the crate and where it goes when the box stands open -- read off the
	// pack's demo map (Docs/<Pack>_DemoAssemblies.json) and set per instance by Tools/facility_layout.py.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") TObjectPtr<UStaticMesh> CrateMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") TObjectPtr<UStaticMesh> LidMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") FVector LidSeat = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") FVector LidOpenOffset = FVector(-14.0f, 0.0f, 34.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box") FRotator LidOpenRotation = FRotator(0.0f, 0.0f, -58.0f);
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
