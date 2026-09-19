#include "World/InspectSurface.h"
#include "Components/StaticMeshComponent.h"

AInspectSurface::AInspectSurface()
{
	Tags.Add(TEXT("inspectable"));   // opts into the reticle (ABasePlayerController::ResolveInspectable)
	PrimaryActorTick.bCanEverTick = false;
	Slab = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Slab"));
	SetRootComponent(Slab);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/RepliCan/Materials/M_InspectGlow.M_InspectGlow"));
	if (Cube.Succeeded()) { Slab->SetStaticMesh(Cube.Object); }
	if (Glow.Succeeded()) { Slab->SetMaterial(0, Glow.Object); }
	Slab->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.04f));
	Slab->SetCastShadow(false);
	// Answers the inspect trace (Visibility) and nothing else: no physics, no pawn blocking.
	Slab->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Slab->SetCollisionResponseToAllChannels(ECR_Ignore);
	Slab->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Slab->SetGenerateOverlapEvents(false);
}

void AInspectSurface::BeginPlay()
{
	Super::BeginPlay();
	Slab->SetHiddenInGame(true);
}

void AInspectSurface::SetHovered(bool bOn)
{
	Slab->SetHiddenInGame(!bOn);
}
