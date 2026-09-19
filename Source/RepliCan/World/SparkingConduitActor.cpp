#include "World/SparkingConduitActor.h"
#include "Weapons/SparkFx.h"
#include "Narrative/VoiceLines.h"
#include "World/AmbientPlayer.h"
#include "Weapons/TimedFx.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

ASparkingConduitActor::ASparkingConduitActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ASparkingConduitActor::BeginPlay()
{
	Super::BeginPlay();
	Schedule();
}

void ASparkingConduitActor::Schedule()
{
	if (!GetWorld()) { return; }
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &ASparkingConduitActor::Fire, FMath::FRandRange(FMath::Max(0.5f, IntervalMin), FMath::Max(IntervalMin + 0.5f, IntervalMax)), false);
}

void ASparkingConduitActor::Fire()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	const FVector At = GetActorLocation();
	const FVector Dir = Normal.GetSafeNormal().IsNearlyZero() ? FVector(0.0f, 0.0f, -1.0f) : Normal.GetSafeNormal();
	const bool bSurge = FMath::FRand() < SurgeChance;
	// YELLOW, AND ABOVE ONE. These were (0.55, 0.75, 1.0) and (1.0, 0.85, 0.3) -- inside 0..1, which
	// is the range of a PAINTED SURFACE. A spark is a burning speck; at no more than wall brightness
	// it never reaches the bloom and reads as a dark fleck, which is the same fault the robot's
	// sparks had. Warm white and hot amber rather than the arc's blue-white: this is a failing
	// conduit throwing molten metal, not a clean electrical discharge.
	SparkFx::Burst(World, At, Dir, bSurge ? Count * 2 : Count, Scale,
		FLinearColor(7.0f, 4.6f, 1.3f, 1.0f), FLinearColor(15.0f, 7.4f, 1.1f, 1.0f));
	if (bSurge)
	{
		static UNiagaraSystem* Surge = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Synty/PolygonSciFiHorror/FX/NS_Electricity_Surge_01.NS_Electricity_Surge_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Surge) { TimedFx::Spawn(World, Surge, At, Dir.Rotation(), FVector(0.6f), SurgeFadeAfterSeconds); }
	}
	UAmbientPlayer::PlayFileAt(this, World, FString::Printf(TEXT("spark_crackle_%02d.wav"), FMath::RandRange(1, 3)),
		At, Volume * (bSurge ? 1.4f : 1.0f), FMath::FRandRange(0.9f, 1.1f));
	Schedule();
}
