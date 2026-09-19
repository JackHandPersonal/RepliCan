// A carpet of one small thing -- paper, cans, bottles -- as ONE instanced component, so a room can
// carry hundreds of pieces of litter for one draw call per mesh rather than one per piece.
// Tools/facility_layout.py scatters the instances (seeded, so the same mess every run). No
// collision: litter is walked through, and the reticle has nothing to say about it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LitterActor.generated.h"

class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class REPLICAN_API ALitterActor : public AActor
{
	GENERATED_BODY()
public:
	ALitterActor();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") TObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(VisibleAnywhere, Category = "Litter") TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
	virtual void OnConstruction(const FTransform& Transform) override;
};
