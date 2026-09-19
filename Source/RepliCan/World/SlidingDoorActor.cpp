#include "SlidingDoorActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "AmbientPlayer.h"

ASlidingDoorActor::ASlidingDoorActor()
{
	Tags.Add(TEXT("inspectable"));   // opts into the reticle (ABasePlayerController::ResolveInspectable)
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto Load = [](const TCHAR* Name) -> UStaticMesh*
	{
		static const TCHAR* Base = TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/");
		ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(*(FString(Base) + Name + TEXT(".") + Name));
		return Finder.Succeeded() ? Finder.Object : nullptr;
	};
	Frame05Mesh = Load(TEXT("SM_Bld_Wall_Doorframe_05"));
	LeafL05Mesh = Load(TEXT("SM_Bld_Wall_Doorframe_Door_L_05"));
	LeafR05Mesh = Load(TEXT("SM_Bld_Wall_Doorframe_Door_R_05"));
	Frame01Mesh = Load(TEXT("SM_Bld_Wall_Doorframe_01"));
	Door01Mesh = Load(TEXT("SM_Bld_Wall_Doorframe_Door_01"));
	LiftWallMesh = Load(TEXT("SM_Bld_Lift_Wall_01"));
	LiftDoorLMesh = Load(TEXT("SM_Bld_Lift_Wall_Door_01"));
	LiftDoorRMesh = Load(TEXT("SM_Bld_Lift_Wall_Door_02"));
	Frame06Mesh = Load(TEXT("SM_Bld_Wall_Doorframe_06"));
	Door06Mesh = Load(TEXT("SM_Bld_Wall_Door_06"));

	auto Mesh = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Root);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		return C;
	};
	Frame = Mesh(TEXT("Frame"));
	LeafL = Mesh(TEXT("LeafL"));
	LeafR = Mesh(TEXT("LeafR"));
	// Everything stays movable: a static frame under a movable root would not follow the actor when it is placed.

	// The approach volume straddles the wall so either side opens it; it
	// covers the opening plus a stride on each side.
	Approach = CreateDefaultSubobject<UBoxComponent>(TEXT("Approach"));
	Approach->SetupAttachment(Root);
	Approach->SetCollisionProfileName(TEXT("NoCollision"));   // a marker volume; see AnyPawnNear
	ApplyKind();
}

void ASlidingDoorActor::ApplyKind()
{
	if (Kind == EDoorKind::Lift)
	{
		// Both leaves sit at their closed positions inside the mesh itself: the actor origin is theirs.
		Frame->SetStaticMesh(LiftWallMesh);
		LeafL->SetStaticMesh(LiftDoorLMesh);
		LeafL->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		LeafL->SetRelativeScale3D(FVector::OneVector);
		LeafR->SetStaticMesh(LiftDoorRMesh);
		LeafR->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Approach->SetRelativeLocation(FVector(250.0f, 0.0f, 120.0f));
		Approach->SetBoxExtent(FVector(130.0f, 160.0f, 120.0f));
	}
	else if (Kind == EDoorKind::Cabin)
	{
		// The leaf sits closed inside the mesh itself, like the lift's; it slides left into the wall.
		Frame->SetStaticMesh(Frame06Mesh);
		LeafL->SetStaticMesh(Door06Mesh);
		LeafL->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		LeafL->SetRelativeScale3D(FVector::OneVector);
		LeafR->SetStaticMesh(nullptr);
		LeafR->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Approach->SetRelativeLocation(FVector(250.0f, 0.0f, 120.0f));
		Approach->SetBoxExtent(FVector(110.0f, 120.0f, 120.0f));
	}
	else if (Kind == EDoorKind::Hatch)
	{
		Frame->SetStaticMesh(Frame01Mesh);
		LeafL->SetStaticMesh(Door01Mesh);
		LeafL->SetRelativeLocation(FVector(HatchHingeX, 0, HatchLeafZ));
		LeafL->SetRelativeScale3D(FVector(HatchLeafScaleX, 1.0f, HatchLeafScaleZ));   // the 169 x 256 leaf stretched to fill the 183 x 291 opening
		LeafR->SetStaticMesh(nullptr);
		LeafR->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Approach->SetRelativeLocation(FVector(260.0f, 0.0f, 120.0f));
		Approach->SetBoxExtent(FVector(140.0f, 160.0f, 120.0f));   // short reach across the line: walking down the hall middle must not open every cabin
	}
	else
	{
		Frame->SetStaticMesh(Frame05Mesh);
		LeafL->SetStaticMesh(LeafL05Mesh);
		LeafL->SetRelativeLocationAndRotation(FVector(LeafLClosedX, 0, 0), FRotator::ZeroRotator);   // closed, so the class default and a freshly loaded actor read right
		LeafL->SetRelativeScale3D(FVector::OneVector);
		LeafR->SetStaticMesh(LeafR05Mesh);
		LeafR->SetRelativeLocationAndRotation(FVector(LeafRClosedX, 0, 0), FRotator::ZeroRotator);
		Approach->SetRelativeLocation(FVector(250.0f, 0.0f, 120.0f));
		Approach->SetBoxExtent(FVector(160.0f, 220.0f, 120.0f));
	}
	Frame->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
}

void ASlidingDoorActor::Configure(EDoorKind InKind, float InSwingSign, bool bInLocked)
{
	Kind = InKind; SwingSign = InSwingSign; bLocked = bInLocked;
	ApplyKind();
	PlaceLeaves();
}

void ASlidingDoorActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyKind();
	OpenAmount = bHoldOpen ? 1.0f : 0.0f;   // a door left open starts open, silently
	bWasOpening = bHoldOpen;
	PlaceLeaves();
}

bool ASlidingDoorActor::AnyPawnNear() const
{
	if (!Approach || !GetWorld()) { return false; }
	const FTransform BoxToWorld = Approach->GetComponentTransform();
	const FVector Extent = Approach->GetUnscaledBoxExtent();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn* P = *It;
		if (!P) { continue; }   // hidden pawns count too: the player is hidden through the bed scenes
		const FVector L = BoxToWorld.InverseTransformPosition(P->GetActorLocation());
		if (FMath::Abs(L.X) <= Extent.X && FMath::Abs(L.Y) <= Extent.Y && FMath::Abs(L.Z) <= Extent.Z) { return true; }
	}
	return false;
}

void ASlidingDoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bWantOpen = bHoldOpen || (!bLocked && !IsManual() && AnyPawnNear());
	const float Target = bWantOpen ? 1.0f : 0.0f;
	if (FMath::IsNearlyEqual(OpenAmount, Target)) { bWasOpening = bWantOpen; return; }
	if (bWantOpen != bWasOpening)
	{
		// The travel starts: the big doors hiss, the small ones slide.
		bWasOpening = bWantOpen;
		// Opening settles quietly; closing lands with a thunk (separate files, see RawAudio).
		const FString File = FString::Printf(TEXT("door_%s_%s.wav"), Kind == EDoorKind::Wide ? TEXT("whoosh") : TEXT("slide"), bWantOpen ? TEXT("open") : TEXT("close"));
		UAmbientPlayer::PlayOneShot(this, GetWorld(), File, Kind == EDoorKind::Wide ? 0.8f : 0.5f, 1.0f);
	}
	OpenAmount = FMath::FInterpConstantTo(OpenAmount, Target, DeltaSeconds, 1.0f / FMath::Max(0.05f, SlideSeconds));
	PlaceLeaves();
}

void ASlidingDoorActor::PlaceLeaves()
{
	// Ease the travel so the leaves start and stop softly.
	const float S = FMath::SmoothStep(0.0f, 1.0f, OpenAmount);
	if (Kind == EDoorKind::Hatch)
	{
		LeafL->SetRelativeLocationAndRotation(FVector(HatchHingeX, 0, HatchLeafZ), FRotator(0.0f, SwingSign * SwingDegrees * S, 0.0f));
		return;
	}
	if (Kind == EDoorKind::Cabin)
	{
		LeafL->SetRelativeLocation(FVector(-CabinSlide * S, 0, 0));
		return;
	}
	if (Kind == EDoorKind::Lift)
	{
		LeafL->SetRelativeLocation(FVector(-LiftSlide * S, 0, 0));
		LeafR->SetRelativeLocation(FVector(LiftSlide * S, 0, 0));
		return;
	}
	const float T = S * SlideDistance;
	LeafL->SetRelativeLocation(FVector(LeafLClosedX - T, 0, 0));
	LeafR->SetRelativeLocation(FVector(LeafRClosedX + T, 0, 0));
}

void ASlidingDoorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// In the editor the leaves show the held-open state so the fit can be checked.
	if (!HasActorBegunPlay()) { ApplyKind(); OpenAmount = bHoldOpen ? 1.0f : 0.0f; bWasOpening = bHoldOpen; PlaceLeaves(); }
}
