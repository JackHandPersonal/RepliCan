// Spent brass: a casing thrown from the ejection port on every shot of a cartridge weapon, tumbling
// on a ballistic arc, one bounce on the floor with a tink, then lying there a few seconds before it
// goes. Pooled: a handful of small components on the pawn, reused round-robin, no actors spawned.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BrassFx.generated.h"

class UStaticMeshComponent;

USTRUCT()
struct FBrassCase
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Comp;
	FVector Velocity = FVector::ZeroVector;
	FVector Spin = FVector::ZeroVector;   // degrees per second about each axis
	float FloorZ = -1e9f;
	float Age = 0.0f;
	bool bLanded = false;
	bool bLive = false;
	bool bShell = false; int32 Bounces = 0;
};

UCLASS()
class REPLICAN_API UBrassFx : public UObject
{
	GENERATED_BODY()
public:
	// Throws one casing from Where with the weapon's right and up, for the pawn that fired.
	void Eject(AActor* Owner, const FVector& Where, const FVector& Right, const FVector& Up, const FVector& Forward, bool bShotgunHull);
	void Tick(float DeltaSeconds);
	UPROPERTY(EditAnywhere) int32 PoolSize = 24;
	UPROPERTY(EditAnywhere) float LifeSeconds = 7.0f;
private:
	UPROPERTY() TArray<FBrassCase> Pool;
	UPROPERTY() TObjectPtr<AActor> PoolOwner;
	int32 Next = 0;
	void Tink(const FVector& At, bool bShell, float Strength);
};
