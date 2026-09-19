#include "World/PestActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "World/AmbientPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

APestActor::APestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	// Nothing about a pest should ever be in the way: no collision, no shadow-casting cost, and
	// it must never block a shot or count as a hit. It is set dressing that happens to move.
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCastShadow(true);
	Body->SetMobility(EComponentMobility::Movable);
}

void APestActor::Launch(UStaticMesh* Mesh, const FVector& Start, const FVector& InGoal, float Scale)
{
	Body->SetStaticMesh(Mesh);
	BaseScale = FMath::Max(0.05f, Scale);
	Body->SetWorldScale3D(FVector(BaseScale));
	Goal = InGoal;
	Heading = (Goal - Start).GetSafeNormal2D();
	if (Heading.IsNearlyZero()) { Heading = FVector::ForwardVector; }
	SetActorLocation(Start);
	SetActorRotation(Heading.Rotation());
	// It starts moving. A pest that spawns frozen looks like a prop somebody left there.
	bRunning = true;
	StateLeft = FMath::FRandRange(DashSeconds.X, DashSeconds.Y);
	BobPhase = FMath::FRandRange(0.0f, 6.28f);
	Age = 0.0f;
	Dying = -1.0f;
}

bool APestActor::GroundAt(const FVector& Where, float& OutZ) const
{
	const UWorld* W = GetWorld();
	if (!W) { return false; }
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PestGround), false, this);
	// From knee height down through the deck: enough to catch a step up onto a plate without
	// finding the ceiling of the deck below.
	if (W->LineTraceSingleByChannel(Hit, Where + FVector(0, 0, 45.0f), Where - FVector(0, 0, 120.0f), ECC_Visibility, Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void APestActor::PickNextBurst()
{
	bRunning = !bRunning;
	StateLeft = bRunning ? FMath::FRandRange(DashSeconds.X, DashSeconds.Y)
	                     : FMath::FRandRange(FreezeSeconds.X, FreezeSeconds.Y);
	if (bRunning)
	{
		// SILENT, DELIBERATELY (2026-09-17, the user's call: "I don't want them to make a sound, the
		// ambient would cover anything that subtle"). A pest dashes about once a second and this
		// played a skitter on every dash; six of them inside the audible radius came to roughly eight
		// a second, which is not vermin heard in the distance but a constant clicking that follows
		// the player around the map. It was reported as clicking three times and cost a long hunt
		// through leaked audio components before anyone thought to ask what in the game is SUPPOSED
		// to make a small repetitive noise. Anything quiet enough to be right here would be under the
		// ambient bed anyway, so there is nothing to gain by tuning it down instead of out.
		// The pests are seen, not heard. RawAudio/pest_skitter_*.wav are left on disk, unused.
		// A new heading each burst: mostly toward the goal, deflected a little. Choosing the
		// deflection once per burst rather than every frame is what makes the path look
		// decided rather than noisy -- it commits, then commits again somewhere else.
		const FVector Straight = (Goal - GetActorLocation()).GetSafeNormal2D();
		const FVector Side = FVector::CrossProduct(Straight, FVector::UpVector);
		const float Off = FMath::FRandRange(-WanderCm, WanderCm) / 100.0f;
		Heading = (Straight + Side * Off).GetSafeNormal2D();
		if (Heading.IsNearlyZero()) { Heading = Straight; }
	}
}

void APestActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	// Shrinking away, then gone.
	if (Dying >= 0.0f)
	{
		Dying += DeltaSeconds;
		const float T = FMath::Clamp(1.0f - Dying / FMath::Max(0.01f, FadeSeconds), 0.0f, 1.0f);
		Body->SetWorldScale3D(FVector(BaseScale * T));
		if (T <= 0.0f) { Destroy(); }
		return;
	}

	FVector Loc = GetActorLocation();
	const float ToGoal = FVector::Dist2D(Loc, Goal);
	if (ToGoal <= ArriveRadiusCm || Age >= MaxLifeSeconds)
	{
		Dying = 0.0f;
		return;
	}

	StateLeft -= DeltaSeconds;
	if (StateLeft <= 0.0f) { PickNextBurst(); }

	if (bRunning)
	{
		Loc += Heading * DashSpeed * DeltaSeconds;
		BobPhase += DeltaSeconds * BobHz * 6.2831853f;
	}

	// Follow the floor. Without this it skims at its spawn height and walks through a step.
	float GroundZ = Loc.Z;
	if (GroundAt(Loc, GroundZ))
	{
		Loc.Z = FMath::FInterpTo(Loc.Z, GroundZ, DeltaSeconds, 18.0f);
	}
	// The bob is applied on top of the ground, not blended into it, so a freeze sits flat.
	const float Bob = bRunning ? FMath::Sin(BobPhase) * BobHeightCm : 0.0f;
	SetActorLocation(FVector(Loc.X, Loc.Y, Loc.Z + Bob));

	FRotator Rot = Heading.Rotation();
	// A little roll with the bob: at this size that is the only suggestion of legs the player
	// will ever get, and it is enough.
	Rot.Roll = bRunning ? FMath::Sin(BobPhase * 0.5f) * RollDegrees : 0.0f;
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Rot, DeltaSeconds, 12.0f));
}
