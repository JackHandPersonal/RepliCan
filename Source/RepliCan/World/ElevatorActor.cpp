#include "ElevatorActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "EngineUtils.h"
#include "AmbientPlayer.h"
#include "SignActor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* KitCar      = TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Lift_01.SM_Bld_Lift_01");
	const TCHAR* KitCarDoorL = TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Lift_Door_01.SM_Bld_Lift_Door_01");
	const TCHAR* KitCarDoorR = TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Lift_Door_02.SM_Bld_Lift_Door_02");
	const TCHAR* KitShaftL   = TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Lift_Wall_Door_01.SM_Bld_Lift_Wall_Door_01");
	const TCHAR* KitShaftR   = TEXT("/Game/PolygonSciFiSpace/Meshes/Buildings/SM_Bld_Lift_Wall_Door_02.SM_Bld_Lift_Wall_Door_02");
	const TCHAR* KitPanel    = TEXT("/Game/PolygonSciFiSpace/Meshes/Props/SM_Prop_Buttons_11.SM_Prop_Buttons_11");

	UStaticMesh* Kit(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet); }
}

AElevatorActor::AElevatorActor()
{
	PrimaryActorTick.bCanEverTick = true;
	// PrePhysics: the car has to have moved before CharacterMovement works out where the person
	// standing on it ends up. Moving it later means the rider lags the floor by a frame, which
	// at lift speeds is a visible judder underfoot.
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Car = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Car"));
	SetRootComponent(Car);
	Car->SetMobility(EComponentMobility::Movable);
	Car->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Car->SetCollisionProfileName(TEXT("BlockAll"));
	Car->SetCanEverAffectNavigation(false);

	CarDoorL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarDoorL"));
	CarDoorL->SetupAttachment(Car);
	CarDoorL->SetMobility(EComponentMobility::Movable);
	CarDoorL->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CarDoorL->SetCollisionProfileName(TEXT("BlockAll"));

	CarDoorR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarDoorR"));
	CarDoorR->SetupAttachment(Car);
	CarDoorR->SetMobility(EComponentMobility::Movable);
	CarDoorR->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CarDoorR->SetCollisionProfileName(TEXT("BlockAll"));

	// The car mesh's own collision may or may not be a closed box; a deck the player can
	// certainly stand on is worth the one component rather than trusting the art.
	CarVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CarVolume"));
	CarVolume->SetupAttachment(Car);
	CarVolume->SetBoxExtent(FVector(150.0f, 150.0f, 130.0f));
	CarVolume->SetRelativeLocation(FVector(250.0f, -168.0f, 130.0f));   // the car mesh's own centre
	CarVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CarVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
	CarVolume->SetGenerateOverlapEvents(true);

	// The panel. Buttons_11 is the biggest button board in the kit (47 x 39), authored lying
	// flat with its face up +Z. It goes on the car's +X inside wall -- the wall on your RIGHT as
	// you face the door -- beside the doorway at hand height. Pitch +90 sends +Z to -X, which
	// turns the face into the car. The car's outer skin is at x 422.3; 12 cm in from that is a
	// guess at the wall's thickness and is the one number here that was not measured.
	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Car);
	Panel->SetRelativeLocation(FVector(410.0f, -45.0f, 125.0f));
	Panel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Panel->SetCollisionProfileName(TEXT("BlockAll"));
	Panel->SetCastShadow(false);

	// THE MESHES ARE ASSIGNED HERE, NOT ONLY AT BEGINPLAY. Loaded at BeginPlay they exist in
	// play and nowhere else: in the editor viewport the lift was an empty transform, and the
	// day the shaft-wall pieces at unserviced floors came out there was nothing left to see of
	// it at all -- "the lift is gone", which it was not. A ConstructorHelpers finder puts them
	// on the class default object, so every placed car shows in the editor as it will in play.
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CarMesh(KitCar);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorLMesh(KitCarDoorL);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorRMesh(KitCarDoorR);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> PanelMesh(KitPanel);
		if (CarMesh.Succeeded())   { Car->SetStaticMesh(CarMesh.Object); }
		if (DoorLMesh.Succeeded()) { CarDoorL->SetStaticMesh(DoorLMesh.Object); }
		if (DoorRMesh.Succeeded()) { CarDoorR->SetStaticMesh(DoorRMesh.Object); }
		if (PanelMesh.Succeeded()) { Panel->SetStaticMesh(PanelMesh.Object); }
	}

	// Up under the car's ceiling. The car mesh runs z -66..300 and its interior centres on
	// (250, -168), so 250 is just below the roof and over the middle of the floor.
	CarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CarLight"));
	CarLight->SetupAttachment(Car);
	CarLight->SetRelativeLocation(FVector(250.0f, -168.0f, 250.0f));
	// Movable, like every light in this map -- a stationary one would want a baked lightmap,
	// and this one moves ten storeys.
	CarLight->SetMobility(EComponentMobility::Movable);
	CarLight->SetIntensityUnits(ELightUnits::Candelas);
	CarLight->SetIntensity(26.0f);
	CarLight->SetAttenuationRadius(650.0f);
	CarLight->SetLightColor(FLinearColor(1.0f, 0.88f, 0.70f));
	// No shadows: the car is a box with one occupant and shadowing it costs more than it says.
	CarLight->SetCastShadows(false);
}

void AElevatorActor::BeginPlay()
{
	Super::BeginPlay();
	StartZ = GetActorLocation().Z;

	Car->SetStaticMesh(Kit(KitCar));
	CarDoorL->SetStaticMesh(Kit(KitCarDoorL));
	CarDoorR->SetStaticMesh(Kit(KitCarDoorR));
	Panel->SetStaticMesh(Kit(KitPanel));
	if (!Car->GetStaticMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("Elevator: the SciFi Space lift meshes are missing; nothing to ride"));
	}
	SpawnShaftDoors();
	// Shut at rest. The first version started fully open, which meant the very first thing the
	// player ever saw the lift do was close.
	SetDoorOpen(0.0f);
	bWasOpening = false;
}

void AElevatorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	StartZ = GetActorLocation().Z;
	SpawnShaftDoors();
	SetDoorOpen(0.0f);
}

void AElevatorActor::SpawnShaftDoors()
{
	// Idempotent: called from OnConstruction (editor) and BeginPlay (play), so whatever pair of
	// arrays already exists is torn down first rather than doubled.
	for (TArray<TObjectPtr<UStaticMeshComponent>>* Arr : { &ShaftDoorL, &ShaftDoorR })
	{
		for (const TObjectPtr<UStaticMeshComponent>& C : *Arr) { if (C) { C->DestroyComponent(); } }
		Arr->Reset();
	}
	// One pair per level, all at the same XY as the car -- which is the entire trick the demo
	// map revealed: nothing in this assembly is offset or rotated relative to anything else.
	UStaticMesh* L = Kit(KitShaftL);
	UStaticMesh* R = Kit(KitShaftR);
	if (!L || !R) { return; }

	for (int32 Floor = -FloorsDown; Floor <= FloorsUp; ++Floor)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			// The arrays stay indexed by floor (SetDoorOpen relies on it), so an unserviced floor
			// gets a null entry rather than a pair of leaves onto solid wall.
			if (!IsFloorServiced(Floor)) { (Side == 0 ? ShaftDoorL : ShaftDoorR).Add(nullptr); continue; }
			UStaticMeshComponent* Leaf = NewObject<UStaticMeshComponent>(this);
			// A construction-script component: the editor owns its lifetime and rebuilds it with
			// the actor, and it is never saved into the level as a stray.
			Leaf->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			Leaf->SetupAttachment(GetRootComponent());
			Leaf->RegisterComponent();
			// Attached to the ROOT but placed in world terms at the floor's own height: the
			// root is the car and it moves, so the leaves are detached from it and parked.
			Leaf->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Leaf->SetStaticMesh(Side == 0 ? L : R);
			Leaf->SetMobility(EComponentMobility::Movable);
			Leaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Leaf->SetCollisionProfileName(TEXT("BlockAll"));
			Leaf->SetCanEverAffectNavigation(false);
			Leaf->SetWorldLocation(FVector(GetActorLocation().X, GetActorLocation().Y, StartZ + FloorZ(Floor)));
			Leaf->SetWorldRotation(GetActorRotation());
			(Side == 0 ? ShaftDoorL : ShaftDoorR).Add(Leaf);
		}
	}
}

FString AElevatorActor::FloorName(int32 Floor) const
{
	if (Floor == 0) { return TEXT("Bay 01"); }
	return Floor > 0 ? FString::Printf(TEXT("Deck %02d"), Floor)
	                 : FString::Printf(TEXT("Sub %02d"), -Floor);
}

FString AElevatorActor::StatusLine() const
{
	// Levels are numbered DOWN from the top of the shaft: the top floor (+4) is LVL 0 and the
	// bay (floor 0) is LVL 4, so the basement is LVL 14. One row on the sign, per the user.
	auto Level = [](int32 Floor) { return FString::FromInt(4 - Floor); };
	if (bMoving)
	{
		const TCHAR* Arrow = (TargetFloor > CurrentFloor) ? TEXT("UP") : TEXT("DOWN");
		return FString::Printf(TEXT("%s TO LVL %s"), Arrow, *Level(TargetFloor));
	}
	return FString::Printf(TEXT("LVL %s%s"), *Level(CurrentFloor), DoorAlpha > 0.5f ? TEXT(" OPEN") : TEXT(""));
}

void AElevatorActor::UpdateSign()
{
	if (!StatusSign) { return; }
	const FString Want = LiftName + TEXT(" - ") + StatusLine();   // one row: "LIFT B4 - LVL 4"
	if (Want == LastSignText) { return; }
	LastSignText = Want;
	StatusSign->SetText(Want);
}

bool AElevatorActor::IsInspectPoint(const UPrimitiveComponent* Comp) const
{
	if (!Comp) { return true; }
	// Inside: the panel. Outside: the doors, which is where a call button lives on a real lift.
	if (Comp == Panel) { return true; }
	if (Comp == CarDoorL || Comp == CarDoorR) { return true; }
	for (const TObjectPtr<UStaticMeshComponent>& L : ShaftDoorL) { if (Comp == L) { return true; } }
	for (const TObjectPtr<UStaticMeshComponent>& R : ShaftDoorR) { if (Comp == R) { return true; } }
	return false;
}

bool AElevatorActor::IsFloorServiced(int32 Floor) const
{
	return ServicedFloors.Num() == 0 || ServicedFloors.Contains(Floor);
}

TArray<FString> AElevatorActor::FloorMenu() const
{
	// Top to bottom, the way a lift panel reads. Only the floors that GO somewhere: a sealed deck
	// is not on the panel at all, so the list is two entries long until the others are built
	// rather than fifteen entries of which thirteen refuse you.
	TArray<FString> Out;
	for (int32 Floor = FloorsUp; Floor >= -FloorsDown; --Floor)
	{
		if (!IsFloorServiced(Floor)) { continue; }
		Out.Add(FloorName(Floor) + (Floor == CurrentFloor ? TEXT("  <") : TEXT("")));
	}
	return Out;
}

int32 AElevatorActor::FloorFromName(const FString& Name) const
{
	FString Clean = Name;
	Clean.RemoveFromEnd(TEXT("  <"));
	Clean.RemoveFromEnd(TEXT("   SEALED"));
	for (int32 Floor = -FloorsDown; Floor <= FloorsUp; ++Floor)
	{
		if (FloorName(Floor) == Clean) { return Floor; }
	}
	return -1000;
}

bool AElevatorActor::IsSomeoneAboard() const
{
	if (!CarVolume) { return false; }
	TArray<AActor*> Overlapping;
	CarVolume->GetOverlappingActors(Overlapping, ACharacter::StaticClass());
	return Overlapping.Num() > 0;
}

bool AElevatorActor::AnyPawnNear() const
{
	if (!GetWorld()) { return false; }
	// The threshold in world terms, which moves with the car.
	const FTransform CarToWorld = GetActorTransform();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn* P = *It;
		if (!P) { continue; }
		const FVector L = CarToWorld.InverseTransformPosition(P->GetActorLocation()) - DoorwayLocal;
		if (FMath::Abs(L.X) <= ApproachHalfWidthCm
			&& FMath::Abs(L.Y) <= ApproachDepthCm
			&& FMath::Abs(L.Z) <= ApproachHalfHeightCm)
		{
			return true;
		}
	}
	return false;
}

void AElevatorActor::GoToFloor(int32 Floor)
{
	Floor = FMath::Clamp(Floor, -FloorsDown, FloorsUp);
	if (!IsFloorServiced(Floor))
	{
		// Nothing behind those doors. Refusing is the whole point of the list.
		return;
	}
	if (Floor == CurrentFloor && !bMoving)
	{
		// Already here: open up rather than doing nothing, which is what a real call button does.
		DwellLeft = DwellSeconds;
		return;
	}
	TargetFloor = Floor;
	bMoving = true;
	DwellLeft = 0.0f;
}

void AElevatorActor::SetDoorOpen(float Alpha)
{
	DoorAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float Slide = DoorSlideCm * DoorAlpha;
	// The leaves part along the wall's own X, which is the axis the opening runs along.
	if (CarDoorL) { CarDoorL->SetRelativeLocation(FVector(-Slide, 0.0f, 0.0f)); }
	if (CarDoorR) { CarDoorR->SetRelativeLocation(FVector(Slide, 0.0f, 0.0f)); }

	// Only the shaft doors at the floor the car is actually at ever move. The rest stay shut,
	// which is what stops the shaft being a row of open holes with a lift somewhere else.
	//
	// THE LEAVES PART ALONG THE CAR'S OWN X, NOT THE WORLD'S. The car doors get this right for
	// free because they are children and take a RELATIVE offset; the shaft leaves are detached
	// and were being given a WORLD X offset, which is only the same thing at yaw zero. This
	// shaft is built at yaw 180, where the two are opposite -- so the leaves swapped sides and
	// drove together, leaving a 7 cm gap in a 184 cm opening. Measured, with the kit's own
	// numbers (Wall_Door_01 local x 159.5..257.6, _02 246.5..344.2, slide 96):
	//
	//     closed            L 492.4..590.5   R 405.8..503.5   -> covers the opening
	//     world X offset    L 396.4..494.5   R 501.8..599.5   -> 7 cm gap, still shut
	//     local X offset    L 588.4..686.5   R 309.8..407.5   -> 181 cm clear
	const int32 Index = CurrentFloor + FloorsDown;
	const FVector Origin = GetActorLocation();
	for (int32 i = 0; i < ShaftDoorL.Num(); ++i)
	{
		const float Here = (i == Index) ? Slide : 0.0f;
		// The car's own +X, in world terms, scaled by how far the leaves have travelled.
		const FVector Part = GetActorRotation().RotateVector(FVector(Here, 0.0f, 0.0f));
		const float Z = StartZ + FloorZ(i - FloorsDown);
		if (ShaftDoorL.IsValidIndex(i) && ShaftDoorL[i])
		{
			ShaftDoorL[i]->SetWorldLocation(FVector(Origin.X - Part.X, Origin.Y - Part.Y, Z));
		}
		if (ShaftDoorR.IsValidIndex(i) && ShaftDoorR[i])
		{
			ShaftDoorR[i]->SetWorldLocation(FVector(Origin.X + Part.X, Origin.Y + Part.Y, Z));
		}
	}
}

void AElevatorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSign();

	if (bMoving)
	{
		// Doors first: it does not move until they are shut.
		if (DoorAlpha > 0.0f)
		{
			SetDoorOpen(DoorAlpha - DeltaSeconds / FMath::Max(DoorSeconds, 0.05f));
			return;
		}

		const float Target = StartZ + FloorZ(TargetFloor);
		FVector Loc = GetActorLocation();
		const float Remaining = Target - Loc.Z;
		const float Distance = FMath::Abs(Remaining);
		if (Distance < 1.0f)
		{
			Loc.Z = Target;
			SetActorLocation(Loc);
			CurrentFloor = TargetFloor;
			bMoving = false;
			DwellLeft = DwellSeconds;
			bWasOpening = false;   // so the arrival opening plays its sound
			return;
		}
		// Ease in and out over the last EaseCm at either end, so it gathers itself and settles
		// rather than snapping to speed.
		const float FromStart = FMath::Abs(Loc.Z - (StartZ + FloorZ(CurrentFloor)));
		const float Ramp = FMath::Clamp(FMath::Min(FromStart, Distance) / FMath::Max(EaseCm, 1.0f), 0.12f, 1.0f);
		Loc.Z += FMath::Sign(Remaining) * SpeedCmPerSecond * Ramp * DeltaSeconds;
		// No sweep: a lift is not pushing its way through the world, and sweeping it against
		// the rider is what makes platforms shove people through floors.
		SetActorLocation(Loc, false, nullptr, ETeleportType::None);
		return;
	}

	// Stopped. The doors now work on approach, like every other door in the station: open for
	// whoever is at the threshold, shut again once they have gone. A floor that is not fitted
	// out has a hole behind those doors and never opens at all.
	if (DwellLeft > 0.0f) { DwellLeft -= DeltaSeconds; }
	if (!IsFloorServiced(CurrentFloor))
	{
		if (DoorAlpha > 0.0f) { SetDoorOpen(DoorAlpha - DeltaSeconds / FMath::Max(DoorSeconds, 0.05f)); }
		return;
	}

	// The dwell is a MINIMUM hold after arriving, not the whole behaviour: it keeps the doors
	// open for a moment when the car is called to an empty floor, so it visibly answers.
	const bool bWantOpen = !bAutoDoors || DwellLeft > 0.0f || AnyPawnNear();
	const float Target = bWantOpen ? 1.0f : 0.0f;
	if (FMath::IsNearlyEqual(DoorAlpha, Target)) { bWasOpening = bWantOpen; return; }
	if (bWantOpen != bWasOpening)
	{
		bWasOpening = bWantOpen;
		// The same two samples the station's other sliding doors use, so a lift door and a
		// corridor door are recognisably the same fittings.
		UAmbientPlayer::PlayOneShot(this, GetWorld(),
			bWantOpen ? TEXT("door_slide_open.wav") : TEXT("door_slide_close.wav"), 0.5f, 1.0f);
	}
	SetDoorOpen(DoorAlpha + FMath::Sign(Target - DoorAlpha) * DeltaSeconds / FMath::Max(DoorSeconds, 0.05f));
}
