#include "World/LitterActor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

ALitterActor::ALitterActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	SetRootComponent(Instances);
	Instances->SetMobility(EComponentMobility::Static);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(false);   // a paper scrap's shadow is not worth a shadow pass per instance
	Instances->bReceivesDecals = true;
}

void ALitterActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Mesh && Instances && Instances->GetStaticMesh() != Mesh) { Instances->SetStaticMesh(Mesh); }
}
