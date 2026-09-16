#include "LootBoxActor.h"
#include "Components/StaticMeshComponent.h"

ALootBoxActor::ALootBoxActor()
{
	Tags.Add(TEXT("inspectable"));   // opts into the reticle (ABasePlayerController::ResolveInspectable)
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	auto Mesh = [this](const TCHAR* Name, const TCHAR* Path) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(Path);
		if (Finder.Succeeded()) { C->SetStaticMesh(Finder.Object); }
		C->SetCollisionProfileName(TEXT("BlockAll"));
		return C;
	};
	Crate = Mesh(TEXT("Crate"), TEXT("/Game/PolygonSciFiSpace/Meshes/Props/SM_Prop_Crate_01.SM_Prop_Crate_01"));
	Lid = Mesh(TEXT("Lid"), TEXT("/Game/PolygonSciFiSpace/Meshes/Props/SM_Prop_Crate_Lid_01.SM_Prop_Crate_Lid_01"));
	Lid->SetCollisionProfileName(TEXT("NoCollision"));
	PlaceLid();
}

void ALootBoxActor::SetOpen(bool bInOpen)
{
	bOpen = bInOpen;
	PlaceLid();
}

void ALootBoxActor::PlaceLid()
{
	if (!Lid) { return; }
	// Lifted off and propped against the far edge, as the bay's opened crates are.
	if (bOpen) { Lid->SetRelativeLocationAndRotation(FVector(-14.0f, 0.0f, 34.0f), FRotator(0.0f, 0.0f, -58.0f)); }
	else { Lid->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator); }
}

void ALootBoxActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PlaceLid();
}
