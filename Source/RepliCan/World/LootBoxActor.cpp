#include "World/LootBoxActor.h"
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
	// Lifted off and propped against the far edge, as the bay's opened crates are (the defaults);
	// a swapped-in pair carries its own seat and open pose.
	if (bOpen) { Lid->SetRelativeLocationAndRotation(LidSeat + LidOpenOffset, LidOpenRotation); }
	else { Lid->SetRelativeLocationAndRotation(LidSeat, FRotator::ZeroRotator); }
}

void ALootBoxActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (CrateMesh && Crate) { Crate->SetStaticMesh(CrateMesh); }
	if (LidMesh && Lid) { Lid->SetStaticMesh(LidMesh); }
	PlaceLid();
}
