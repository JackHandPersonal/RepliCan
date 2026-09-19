#include "World/AmbientMotes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "CollisionQueryParams.h"
#include "Weapons/TimedFx.h"

UAmbientMotesComponent::UAmbientMotesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.15f;   // nothing here needs a frame's resolution
}

void UAmbientMotesComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!System && !SystemPath.IsEmpty())
	{
		System = LoadObject<UNiagaraSystem>(nullptr, *SystemPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	Clock = FMath::FRandRange(Interval.X, Interval.Y);
}

bool UAmbientMotesComponent::EyeOf(FVector& OutLoc, FRotator& OutRot) const
{
	// The camera, not the pawn: a puff has to be placed where it will be SEEN, and in third person
	// that is a different place and a different direction from where the body is facing.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		OutLoc = PC->PlayerCameraManager->GetCameraLocation();
		OutRot = PC->PlayerCameraManager->GetCameraRotation();
		return true;
	}
	if (Pawn)
	{
		Pawn->GetActorEyesViewPoint(OutLoc, OutRot);
		return true;
	}
	return false;
}

void UAmbientMotesComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	UWorld* World = GetWorld();
	if (!World || !System) { return; }
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || !Pawn->IsPlayerControlled()) { return; }   // one set of air, the one being looked at

	// HOW MANY ARE IN THE AIR. Prune the ones that have gone, and if the air is already as full as
	// it is meant to get, do not add another. With a fade this is belt and braces -- the steady
	// state is only a handful -- but it is what actually guarantees "subtle": no combination of a
	// long fade, a short interval or a stall can pile them up again.
	for (int32 i = Live.Num() - 1; i >= 0; --i) { if (!Live[i].IsValid()) { Live.RemoveAtSwap(i); } }

	Clock -= DeltaSeconds;
	if (Clock > 0.0f) { return; }
	if (Live.Num() >= MaxAlive) { return; }

	FVector EyeLoc; FRotator EyeRot;
	if (!EyeOf(EyeLoc, EyeRot)) { return; }
	// The deck is meant to feel thicker than the rest of the station, so the same effect simply
	// happens oftener down there rather than being a second, different effect.
	const bool bDeck = EyeLoc.Z < DeckBelowZ;
	Clock = FMath::FRandRange(Interval.X, Interval.Y) * (bDeck ? DeckIntervalScale : 1.0f);

	// A point in the open air ahead: along the view, spread sideways, and free to be above or below
	// the eye so the specks are not all on one plane.
	const FVector Fwd = EyeRot.Vector();
	const FVector Right = FRotationMatrix(EyeRot).GetUnitAxis(EAxis::Y);
	const FVector At = EyeLoc
		+ Fwd * FMath::FRandRange(NearCm, FarCm)
		+ Right * FMath::FRandRange(-SpreadCm, SpreadCm)
		+ FVector(0.0f, 0.0f, FMath::FRandRange(-RiseCm * 0.6f, RiseCm));

	// OPEN AIR ONLY. A puff that blooms inside a wall or a crate is worse than no puff, and the
	// cheapest way to know is to ask for a small sphere's worth of room at the spot.
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AmbientMotes), false, GetOwner());
	if (World->OverlapBlockingTestByChannel(At, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(ClearanceCm), Q))
	{
		return;
	}
	// And it must be air the player can actually see into, not air on the far side of a wall.
	FHitResult Blocked;
	if (World->LineTraceSingleByChannel(Blocked, EyeLoc, At, ECC_Visibility, Q))
	{
		return;
	}

	if (UNiagaraComponent* C = TimedFx::Spawn(World, System, At, FRotator::ZeroRotator,
			FVector(Scale * FMath::FRandRange(0.8f, 1.25f)), FadeAfterSeconds))
	{
		Live.Add(C);
	}
}
